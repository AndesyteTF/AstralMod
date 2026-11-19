#include "ProfileBridge.h"

#include "../Web/Web.h"
#include "../../Utils/Log.h"

#include <Windows.h>
#include <wincodec.h>
#include <atomic>
#include <cstring>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace ProfileBridge
{
namespace
{
    constexpr wchar_t kPipeName[]{ L"\\\\.\\pipe\\AstralMod.Profile.v1" };
    constexpr unsigned long kHelloMagic{ 0x4F4C5341u }; // ASLO
    constexpr unsigned long kProfileMagic{ 0x46505341u }; // ASPF
    constexpr unsigned long kAckMagic{ 0x4B435341u }; // ASCK
    constexpr unsigned short kVersion{ 1 };
    constexpr unsigned short kAvatarSize{ 64 };

#pragma pack(push, 1)
    struct Hello
    {
        unsigned long magic;
        unsigned short version;
        unsigned short size;
        unsigned long processId;
    };

    struct Profile
    {
        unsigned long magic;
        unsigned short version;
        unsigned short size;
        unsigned short avatarSize;
        unsigned short flags;
        char username[64];
        unsigned char avatar[kAvatarSize * kAvatarSize * 4];
    };

    // Sent back by the module through the same connection once the profile is
    // applied. Flags: 1 = username applied, 2 = avatar applied.
    struct Ack
    {
        unsigned long magic;
        unsigned short version;
        unsigned short size;
        unsigned long processId;
        unsigned short flags;
    };
#pragma pack(pop)

    std::thread s_thread;
    std::atomic<bool> s_stop{ false };
    std::atomic<bool> s_delivered{ false };
    std::atomic<bool> s_running{ false };
    std::atomic<unsigned long> s_claimed_pid{ 0 };
    std::mutex s_lock;

    std::wstring Widen(const std::string& text)
    {
        return std::wstring(text.begin(), text.end());
    }

    bool DecodeAvatar(const std::vector<unsigned char>& encoded, unsigned char* pixels)
    {
        if (encoded.empty() || !pixels) return false;

        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICBitmapScaler* scaler = nullptr;
        IWICFormatConverter* converter = nullptr;
        bool ok = false;

        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromMemory(
                const_cast<BYTE*>(reinterpret_cast<const BYTE*>(encoded.data())),
                static_cast<DWORD>(encoded.size()))) &&
            SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr,
                WICDecodeMetadataCacheOnLoad, &decoder)) &&
            SUCCEEDED(decoder->GetFrame(0, &frame)) &&
            SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
            SUCCEEDED(scaler->Initialize(frame, kAvatarSize, kAvatarSize,
                WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
            SUCCEEDED(converter->Initialize(scaler, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom)) &&
            SUCCEEDED(converter->CopyPixels(nullptr, kAvatarSize * 4,
                kAvatarSize * kAvatarSize * 4, pixels)))
        {
            // Premask the square Discord image to a circle so the DLL only needs
            // a normal RGBA texture upload.
            const float radius = static_cast<float>(kAvatarSize) * 0.5f;
            for (unsigned y = 0; y < kAvatarSize; ++y)
            {
                for (unsigned x = 0; x < kAvatarSize; ++x)
                {
                    const float dx = static_cast<float>(x) + 0.5f - radius;
                    const float dy = static_cast<float>(y) + 0.5f - radius;
                    if (dx * dx + dy * dy > radius * radius)
                        pixels[(y * kAvatarSize + x) * 4 + 3] = 0;
                }
            }
            ok = true;
        }

        if (converter) converter->Release();
        if (scaler) scaler->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        return ok;
    }

    void Worker(DiscordOAuth::Session session)
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        Profile profile{};
        profile.magic = kProfileMagic;
        profile.version = kVersion;
        profile.size = static_cast<unsigned short>(sizeof(Profile));
        strncpy_s(profile.username, session.username.c_str(), _TRUNCATE);

        if (!session.userId.empty() && !session.avatar.empty())
        {
            const std::wstring url = L"https://cdn.discordapp.com/avatars/" +
                Widen(session.userId) + L"/" + Widen(session.avatar) + L".png?size=64";
            const Binary avatar = Web::DownloadFile(url);
            if (DecodeAvatar(avatar, profile.avatar))
            {
                profile.avatarSize = kAvatarSize;
                profile.flags |= 1;
            }
        }

        // Serve every client that asks. The inject flow retries with fresh
        // game processes (each retry loads a new module instance that needs
        // the profile), so a single delivery must not retire the server.
        while (!s_stop.load(std::memory_order_acquire))
        {
            HANDLE pipe = CreateNamedPipeW(kPipeName, PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1, sizeof(Profile), sizeof(Hello), 0, nullptr);
            if (pipe == INVALID_HANDLE_VALUE) break;

            const bool connected = ConnectNamedPipe(pipe, nullptr) != FALSE ||
                                   GetLastError() == ERROR_PIPE_CONNECTED;
            if (connected)
            {
                Hello hello{};
                DWORD read = 0;
                if (ReadFile(pipe, &hello, sizeof(hello), &read, nullptr) &&
                    read == sizeof(hello) && hello.magic == kHelloMagic &&
                    hello.version == kVersion && hello.size == sizeof(hello))
                {
                    DWORD written = 0;
                    if (WriteFile(pipe, &profile, sizeof(profile), &written, nullptr) &&
                        written == sizeof(profile))
                    {
                        FlushFileBuffers(pipe);
                        s_delivered.store(true, std::memory_order_release);
                        Log::Info("ProfileBridge: delivered Discord profile to PID {}", hello.processId);

                        // The module confirms through the same connection that
                        // it applied the profile; only then can the loader close
                        // fully (see WinMain, which also verifies the game
                        // survived, so crashed inject attempts get re-served).
                        Ack ack{};
                        DWORD ackRead = 0;
                        if (ReadFile(pipe, &ack, sizeof(ack), &ackRead, nullptr) &&
                            ackRead == sizeof(ack) && ack.magic == kAckMagic &&
                            ack.version == kVersion && ack.size == sizeof(ack))
                        {
                            s_claimed_pid.store(ack.processId, std::memory_order_release);
                            Log::Info("ProfileBridge: module in PID {} confirmed using the profile (flags {:#x})",
                                      ack.processId, ack.flags);
                        }
                    }
                }
            }
            DisconnectNamedPipe(pipe);
            CloseHandle(pipe);
        }

        s_running.store(false, std::memory_order_release);
        CoUninitialize();
    }
}

void Start(const DiscordOAuth::Session& session)
{
    Stop();
    s_stop.store(false, std::memory_order_release);
    s_delivered.store(false, std::memory_order_release);
    s_running.store(true, std::memory_order_release);
    s_claimed_pid.store(0, std::memory_order_release);
    s_thread = std::thread(Worker, session);
}

void Stop()
{
    s_stop.store(true, std::memory_order_release);
    if (s_thread.joinable())
    {
        // Wake a server blocked in ConnectNamedPipe.
        HANDLE wake = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        if (wake != INVALID_HANDLE_VALUE) CloseHandle(wake);
        s_thread.join();
    }
    s_running.store(false, std::memory_order_release);
}

bool Delivered()
{
    return s_delivered.load(std::memory_order_acquire);
}

unsigned long ClaimedPid()
{
    return s_claimed_pid.load(std::memory_order_acquire);
}

bool Active()
{
    return s_running.load(std::memory_order_acquire);
}
} // namespace ProfileBridge
