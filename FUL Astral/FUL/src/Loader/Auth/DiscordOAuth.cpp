// _WINSOCKAPI_ stops <windows.h> (pulled in by Utils.h) from dragging in the
// ancient winsock.h, which conflicts with WinSock2.h. WinSock2 must be
// included before any windows header so all socket calls use the WinSock 2 API.
#define _WINSOCKAPI_
#include <WinSock2.h>
#include <WS2tcpip.h>

#include "DiscordOAuth.h"
#include "../../Utils/Utils.h"
#include "../../Utils/xorstr.hpp"

#include <wininet.h>
#include <shellapi.h>
#include <bcrypt.h>

#include <array>
#include <chrono>
#include <algorithm>
#include <ctime>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

namespace DiscordOAuth
{
// Discord application client ID. Replace the placeholder with the value from
// https://discord.com/developers/applications .
const char* kClientIdValue = "YOUR_APPLICATION_ID_HERE";
const char* kClientSecretValue = ""; // optional; only needed for confidential clients
const wchar_t* kRedirectUriValue = L"http://localhost:17987/callback";
const int kCallbackPortValue = 17987;

// Narrow (ASCII) copy of the redirect URI for query-string encoding.
const std::string kRedirectUriNarrow = "http://localhost:17987/callback";

const std::string& ClientId()
{
    static const std::string id = kClientIdValue;
    return id;
}
const std::string& ClientSecret()
{
    static const std::string secret = kClientSecretValue;
    return secret;
}

const char* kAuthHost = "https://discord.com";
const char* kTokenPath = "/api/oauth2/token";
const char* kUserPath = "/api/users/@me";

// Registry value names (HKCU\Software\AstralMod, see Utils::GetStrSetting).
const char* kRegAccessToken = "DiscordAccessToken";
const char* kRegRefreshToken = "DiscordRefreshToken";
const char* kRegExpiresAt = "DiscordExpiresAt";
const char* kRegUserId = "DiscordUserId";
const char* kRegUsername = "DiscordUsername";
const char* kRegAvatar = "DiscordAvatar";

// Minimal "key":"value" JSON extraction (same style as Loader/Auth/Auth.cpp).
std::string JsonStr(const std::string& json, const std::string& key, size_t from = 0)
{
    std::string needle = "\"" + key + "\"";
    size_t start = json.find(needle, from);
    if (start == std::string::npos) { return {}; }
    size_t colon = json.find(':', start + needle.size());
    if (colon == std::string::npos) { return {}; }
    size_t begin = json.find('"', colon + 1);
    if (begin == std::string::npos) { return {}; }
    ++begin;
    size_t end = json.find('"', begin);
    if (end == std::string::npos || end == begin) { return {}; }
    return json.substr(begin, end - begin);
}

// "key":number extraction.
long long JsonInt(const std::string& json, const std::string& key, size_t from = 0)
{
    std::string needle = "\"" + key + "\":";
    size_t start = json.find(needle, from);
    if (start == std::string::npos) { return 0; }
    size_t begin = start + needle.size();
    size_t end = begin;
    while (end < json.size() && (json[end] == '-' || (json[end] >= '0' && json[end] <= '9')))
        ++end;
    if (end == begin) { return 0; }
    return std::stoll(json.substr(begin, end - begin));
}

// Percent-encode a string for use in a query string.
std::string UrlEncode(const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            out.push_back(static_cast<char>(c));
        }
        else
        {
            out.push_back('%');
            out.push_back(hex[c >> 4]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

std::string Base64Url(const BYTE* data, size_t size)
{
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    out.reserve((size * 4 + 2) / 3);
    for (size_t i = 0; i < size; i += 3)
    {
        const unsigned value = (static_cast<unsigned>(data[i]) << 16) |
            (i + 1 < size ? static_cast<unsigned>(data[i + 1]) << 8 : 0) |
            (i + 2 < size ? static_cast<unsigned>(data[i + 2]) : 0);
        out.push_back(alphabet[(value >> 18) & 63]);
        out.push_back(alphabet[(value >> 12) & 63]);
        if (i + 1 < size) out.push_back(alphabet[(value >> 6) & 63]);
        if (i + 2 < size) out.push_back(alphabet[value & 63]);
    }
    return out;
}

std::string RandomUrlToken(size_t bytes = 32)
{
    std::vector<BYTE> random(bytes);
    if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return {};
    return Base64Url(random.data(), random.size());
}

std::string PkceChallenge(const std::string& verifier)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<BYTE, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return {};
    NTSTATUS status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    if (status >= 0)
        status = BCryptHashData(hash,
            reinterpret_cast<PUCHAR>(const_cast<char*>(verifier.data())),
            static_cast<ULONG>(verifier.size()), 0);
    if (status >= 0)
        status = BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return status >= 0 ? Base64Url(digest.data(), digest.size()) : std::string{};
}

// Current unix epoch in seconds.
long long NowSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
// Generic WinINet GET. Returns body on success, empty on failure.
std::string HttpGet(const std::string& url, const std::string& bearerToken)
{
    HINTERNET hInternet = InternetOpenW(L"Framework Unified Loader", INTERNET_OPEN_TYPE_DIRECT,
                                        nullptr, nullptr, 0);
    if (!hInternet) { return {}; }

    DWORD timeout = 10000;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    std::wstring headers;
    if (!bearerToken.empty())
        headers = L"Authorization: Bearer " + std::wstring(bearerToken.begin(), bearerToken.end()) + L"\r\n";

    const std::wstring wurl(url.begin(), url.end());
    HINTERNET hRequest = InternetOpenUrlW(hInternet, wurl.c_str(),
                                          headers.empty() ? nullptr : headers.c_str(),
                                          static_cast<DWORD>(headers.size()),
                                          INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE |
                                              INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES,
                                          0);
    std::string body;
    if (hRequest)
    {
        DWORD status = 0, statusSize = sizeof(DWORD);
        if (!HttpQueryInfoW(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &statusSize, nullptr))
            status = 0;

        if (status >= 200 && status < 300)
        {
            char buffer[8192];
            DWORD bytesRead = 0;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
            {
                body.append(buffer, bytesRead);
                bytesRead = 0;
            }
        }
        InternetCloseHandle(hRequest);
    }
    InternetCloseHandle(hInternet);
    return body;
}

// Generic WinINet POST with application/x-www-form-urlencoded body.
std::string HttpPostForm(const std::string& url, const std::string& formBody)
{
    HINTERNET hInternet = InternetOpenW(L"Framework Unified Loader", INTERNET_OPEN_TYPE_DIRECT,
                                        nullptr, nullptr, 0);
    if (!hInternet) { return {}; }

    DWORD timeout = 10000;
    InternetSetOptionW(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    InternetSetOptionW(hInternet, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));

    const std::wstring wheaders(L"Content-Type: application/x-www-form-urlencoded\r\n");
    const std::wstring wurl(url.begin(), url.end());
    URL_COMPONENTSW parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);

    std::string body;
    if (!InternetCrackUrlW(wurl.c_str(), 0, 0, &parts))
    {
        InternetCloseHandle(hInternet);
        return {};
    }

    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring target(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength)
        target.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    HINTERNET hConnect = InternetConnectW(hInternet, host.c_str(), parts.nPort,
                                           nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    const wchar_t* acceptTypes[] = { L"application/json", nullptr };
    const DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                        INTERNET_FLAG_NO_COOKIES |
                        (parts.nScheme == INTERNET_SCHEME_HTTPS ? INTERNET_FLAG_SECURE : 0);
    HINTERNET hRequest = hConnect
        ? HttpOpenRequestW(hConnect, L"POST", target.c_str(), nullptr, nullptr,
                           acceptTypes, flags, 0)
        : nullptr;

    if (hRequest)
    {
        const char* bodyPtr = formBody.empty() ? nullptr : formBody.data();
        DWORD bodyLen = static_cast<DWORD>(formBody.size());
        if (HttpSendRequestW(hRequest, wheaders.c_str(), static_cast<DWORD>(wheaders.size()),
                             const_cast<char*>(bodyPtr), bodyLen))
        {
            // Read error responses too; callers need Discord's JSON error
            // instead of an indistinguishable empty response.
            char buffer[8192];
            DWORD bytesRead = 0;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0)
            {
                body.append(buffer, bytesRead);
                bytesRead = 0;
            }
        }
        InternetCloseHandle(hRequest);
    }
    if (hConnect) { InternetCloseHandle(hConnect); }
    InternetCloseHandle(hInternet);
    return body;
}

std::string BuildAuthUrl(const std::string& challenge, const std::string& state)
{
    std::string encodedRedirect = UrlEncode(kRedirectUriNarrow);
    return std::string(kAuthHost) + "/api/oauth2/authorize?client_id=" + UrlEncode(ClientId()) +
           "&redirect_uri=" + encodedRedirect + "&response_type=code&scope=identify" +
           "&code_challenge_method=S256&code_challenge=" + UrlEncode(challenge) +
           "&state=" + UrlEncode(state);
}

// Extracts the ?code=... (or ?error=...) value from a raw HTTP request line.
std::string ExtractQueryParam(const std::string& request, const std::string& param)
{
    size_t qmark = request.find('?');
    if (qmark == std::string::npos) { return {}; }
    size_t space = request.find(' ', qmark);
    if (space == std::string::npos) { space = request.size(); }
    std::string query = request.substr(qmark + 1, space - qmark - 1);

    size_t pos = 0;
    while (pos < query.size())
    {
        size_t eq = query.find('=', pos);
        if (eq == std::string::npos) { break; }
        std::string name = query.substr(pos, eq - pos);
        size_t nextAmp = query.find('&', eq + 1);
        size_t valEnd = (nextAmp == std::string::npos) ? query.size() : nextAmp;
        std::string val = query.substr(eq + 1, valEnd - eq - 1);
        if (name == param) { return val; }
        pos = nextAmp;
        if (pos != std::string::npos) { ++pos; }
    }
    return {};
}


const char* kClientId = kClientIdValue;
const char* kClientSecret = kClientSecretValue;
const wchar_t* kRedirectUri = kRedirectUriValue;
const int kCallbackPort = kCallbackPortValue;

Session Authenticate()
{
    Session session;

    // Discord application IDs are numeric snowflakes. Do not compare against
    // the configured value as a "placeholder"—that rejects the real ID.
    const std::string& clientId = ClientId();
    const bool validClientId = clientId.size() >= 17 && clientId.size() <= 20 &&
        std::all_of(clientId.begin(), clientId.end(), [](unsigned char c) {
            return c >= '0' && c <= '9';
        });
    if (!validClientId)
    {
        session.error = "Discord Client ID is missing or invalid.";
        return session;
    }

    const std::string codeVerifier = RandomUrlToken();
    const std::string codeChallenge = PkceChallenge(codeVerifier);
    const std::string expectedState = RandomUrlToken(24);
    if (codeVerifier.empty() || codeChallenge.empty() || expectedState.empty())
    {
        session.error = "Failed to initialize secure Discord login parameters.";
        return session;
    }

    // ---- localhost callback listener ------------------------------------
    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        session.error = "Failed to initialize network sockets.";
        return session;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        session.error = "Failed to create listen socket.";
        WSACleanup();
        return session;
    }

    int reuse = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(kCallbackPort);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        listen(listenSocket, SOMAXCONN) != 0)
    {
        session.error = "Failed to bind the callback listener (is another instance running?).";
        closesocket(listenSocket);
        WSACleanup();
        return session;
    }

    // ---- open the user's browser ----------------------------------------
    std::string authUrl = BuildAuthUrl(codeChallenge, expectedState);
    std::wstring wurl(authUrl.begin(), authUrl.end());
    ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    // ---- wait for the redirect (3 minute timeout) -----------------------
    fd_set fds{};
    FD_ZERO(&fds);
    FD_SET(listenSocket, &fds);
    timeval tv{ 180, 0 };
    int ready = select(0, &fds, nullptr, nullptr, &tv);

    SOCKET client = INVALID_SOCKET;
    std::string code;
    std::string error;
    std::string returnedState;

    if (ready > 0)
    {
        client = accept(listenSocket, nullptr, nullptr);
        if (client != INVALID_SOCKET)
        {
            char buf[4096]{};
            int bytes = recv(client, buf, static_cast<int>(sizeof(buf) - 1), 0);
            if (bytes > 0)
            {
                buf[bytes] = '\0';
                std::string request(buf);
                error = ExtractQueryParam(request, "error");
                code = ExtractQueryParam(request, "code");
                returnedState = ExtractQueryParam(request, "state");
            }

            const char* okPage =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<html><body style='font-family:sans-serif;text-align:center;padding-top:40px;'>"
                "<h1 style='color:#5865F2;'>Logged in!</h1>"
                "<p>You have signed in with Discord. You can close this window and return to the loader.</p>"
                "</body></html>";
            send(client, okPage, static_cast<int>(std::strlen(okPage)), 0);
            closesocket(client);
        }
    }

    closesocket(listenSocket);
    WSACleanup();

    if (!error.empty())
    {
        session.error = "Discord authorization was denied: " + error;
        return session;
    }
    if (code.empty())
    {
        session.error = "Authorization timed out or was cancelled.";
        return session;
    }
    if (returnedState != expectedState)
    {
        session.error = "Discord authorization state mismatch. Please try again.";
        return session;
    }

    // ---- exchange the authorization code for tokens ---------------------
    std::string form = std::string("client_id=") + UrlEncode(ClientId()) +
                       "&grant_type=authorization_code" +
                       "&code=" + UrlEncode(code) +
                       "&redirect_uri=" + UrlEncode(kRedirectUriNarrow) +
                       "&code_verifier=" + UrlEncode(codeVerifier);
    if (!ClientSecret().empty())
        form += "&client_secret=" + UrlEncode(ClientSecret());

    std::string tokenUrl = std::string(kAuthHost) + kTokenPath;
    std::string tokenBody = HttpPostForm(tokenUrl, form);
    if (tokenBody.empty())
    {
        session.error = "Failed to exchange the authorization code (no response).";
        return session;
    }

    session.accessToken = JsonStr(tokenBody, "access_token");
    session.refreshToken = JsonStr(tokenBody, "refresh_token");
    long long expiresIn = JsonInt(tokenBody, "expires_in");

    if (session.accessToken.empty())
    {
        std::string detail = JsonStr(tokenBody, "error_description");
        if (detail.empty()) { detail = JsonStr(tokenBody, "error"); }
        session.error = detail.empty()
            ? "Discord did not return an access token."
            : "Discord token exchange failed: " + detail;
        return session;
    }

    long long now = NowSeconds();
    long long expiry = now + (expiresIn > 0 ? expiresIn : 604800); // default 7 days
    session.expiresAt = std::to_string(expiry);

    // ---- fetch the user's identity --------------------------------------
    std::string userUrl = std::string(kAuthHost) + kUserPath;
    std::string userBody = HttpGet(userUrl, session.accessToken);
    if (userBody.empty())
    {
        session.error = "Failed to fetch Discord user identity.";
        return session;
    }

    session.userId = JsonStr(userBody, "id");
    session.username = JsonStr(userBody, "global_name");
    if (session.username.empty())
        session.username = JsonStr(userBody, "username");
    session.avatar = JsonStr(userBody, "avatar");

    if (session.userId.empty() && session.username.empty())
    {
        session.error = "Discord did not return a user identity.";
        return session;
    }

    session.ok = true;

    // ---- persist tokens so the user stays logged in ---------------------
    Utils::SetStrSetting(kRegAccessToken, session.accessToken);
    if (!session.refreshToken.empty())
        Utils::SetStrSetting(kRegRefreshToken, session.refreshToken);
    Utils::SetStrSetting(kRegExpiresAt, session.expiresAt);
    Utils::SetStrSetting(kRegUserId, session.userId);
    Utils::SetStrSetting(kRegUsername, session.username);
    Utils::SetStrSetting(kRegAvatar, session.avatar);

    Log::Info("DiscordOAuth: logged in as {} ({})", session.username, session.userId);
    return session;
}

Session EnsureLoggedIn()
{
    Session session;

    std::string accessToken = Utils::GetStrSetting(kRegAccessToken);
    std::string refreshToken = Utils::GetStrSetting(kRegRefreshToken);
    std::string expiresAt = Utils::GetStrSetting(kRegExpiresAt);
    session.userId = Utils::GetStrSetting(kRegUserId);
    session.username = Utils::GetStrSetting(kRegUsername);
    session.avatar = Utils::GetStrSetting(kRegAvatar);

    if (accessToken.empty())
    {
        // No stored session at all.
        session.error = "No stored Discord session.";
        return session;
    }

    session.accessToken = accessToken;
    session.refreshToken = refreshToken;
    session.expiresAt = expiresAt;
    session.ok = true;

    long long now = NowSeconds();
    long long expiry = 0;
    try { expiry = std::stoll(expiresAt); } catch (...) { expiry = 0; }

    // Older loader builds did not persist the avatar hash. Backfill it once
    // from the existing valid login without showing OAuth again.
    const auto backfillIdentity = [&session]()
    {
        if (!session.avatar.empty() || session.accessToken.empty()) return;
        const std::string body = HttpGet(std::string(kAuthHost) + kUserPath,
                                         session.accessToken);
        if (body.empty()) return;
        session.avatar = JsonStr(body, "avatar");
        if (session.userId.empty()) session.userId = JsonStr(body, "id");
        if (session.username.empty())
        {
            session.username = JsonStr(body, "global_name");
            if (session.username.empty()) session.username = JsonStr(body, "username");
        }
        Utils::SetStrSetting(kRegAvatar, session.avatar);
        Utils::SetStrSetting(kRegUserId, session.userId);
        Utils::SetStrSetting(kRegUsername, session.username);
    };

    // Access token still valid -> restore the session and migrate old records.
    if (expiry > now + 60)
    {
        backfillIdentity();
        return session;
    }

    // Access token expired (or expires within a minute). Try to refresh.
    if (refreshToken.empty())
    {
        session.ok = false;
        session.error = "Stored session expired and no refresh token is available.";
        Log::Warn("DiscordOAuth: stored session expired, refresh token missing.");
        return session;
    }

    std::string form = std::string("grant_type=refresh_token") +
                       "&refresh_token=" + UrlEncode(refreshToken) +
                       "&client_id=" + UrlEncode(ClientId());
    if (!ClientSecret().empty())
        form += "&client_secret=" + UrlEncode(ClientSecret());

    std::string tokenUrl = std::string(kAuthHost) + kTokenPath;
    std::string tokenBody = HttpPostForm(tokenUrl, form);
    if (tokenBody.empty())
    {
        session.ok = false;
        session.error = "Failed to refresh the Discord session.";
        Log::Warn("DiscordOAuth: token refresh failed (no response).");
        return session;
    }

    std::string newAccess = JsonStr(tokenBody, "access_token");
    std::string newRefresh = JsonStr(tokenBody, "refresh_token");
    long long newExpiresIn = JsonInt(tokenBody, "expires_in");

    if (newAccess.empty())
    {
        session.ok = false;
        std::string detail = JsonStr(tokenBody, "error_description");
        if (detail.empty()) { detail = JsonStr(tokenBody, "error"); }
        session.error = detail.empty()
            ? "Discord refresh failed. Please sign in again."
            : "Discord refresh failed: " + detail;
        Log::Warn("DiscordOAuth: token refresh rejected by Discord.");
        return session;
    }

    session.accessToken = newAccess;
    if (!newRefresh.empty())
    {
        session.refreshToken = newRefresh;
        Utils::SetStrSetting(kRegRefreshToken, newRefresh);
    }
    long long expiry2 = now + (newExpiresIn > 0 ? newExpiresIn : 604800);
    session.expiresAt = std::to_string(expiry2);

    Utils::SetStrSetting(kRegAccessToken, newAccess);
    Utils::SetStrSetting(kRegExpiresAt, session.expiresAt);
    backfillIdentity();
    Log::Info("DiscordOAuth: refreshed session for {} ({})", session.username, session.userId);
    return session;
}

void Logout()
{
    Utils::SetStrSetting(kRegAccessToken, "");
    Utils::SetStrSetting(kRegRefreshToken, "");
    Utils::SetStrSetting(kRegExpiresAt, "");
    Utils::SetStrSetting(kRegUserId, "");
    Utils::SetStrSetting(kRegUsername, "");
    Utils::SetStrSetting(kRegAvatar, "");
    Log::Info("DiscordOAuth: logged out.");
}

}
