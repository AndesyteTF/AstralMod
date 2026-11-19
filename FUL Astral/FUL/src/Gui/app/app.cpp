#include "app/app.h"
#include "app/embedded_fonts.h"
#include "ui/ui.h"
#include "ui/widgets.h"
#include "Loader/Auth/Auth.h"
#include "Loader/DiscordRpc/DiscordRpc.h"
#include "Loader/Music/Music.h"
#include "Loader/ProfileBridge/ProfileBridge.h"
#include "Utils/Utils.h"
#include "Utils/xorstr.hpp"
#include <filesystem>

using namespace ui;
using core::Col;
using core::Rect;
using core::Vec2;

namespace app {

static const float kBarHeight = kTitleBarHeight;
static const float kSplashDuration = 2.5f;
static const float kPaletteSpeed = 7.f;

struct LiquidPalette {
    Col a, b;
    bool rainbow;
};

static LiquidPalette liquidPalette(int product) {
    switch (product) {
    default: return { Col::hex(0x000000, 1.f), Col::hex(0xFFFFFF, 1.f), true };  
    }
}

const char* buildStamp() {
    static const char stamp[] = __TIMESTAMP__;
    return stamp;
}

const char* stageLabel(Stage s, const char* error) {
    switch (s) {
    case Stage::WaitingForSteam: return "Waiting for Steam Client";
    case Stage::EnablingBypass:  return "Enabling VACSAFE Module";
    case Stage::WaitingForGame:  return "Waiting for TF2";
    case Stage::WaitingForClient: return "Waiting for Client";
    case Stage::Loading:         return "Initializing Preloader";
    case Stage::Done:            return "Framework Module Loaded";
    case Stage::Failed:          return error && *error ? error : "Initialization failed";
    default:                     return "";
    }
}

bool App::init(ID3D11Device* dev, const LaunchInfo& launchInfo) {
    Log::Info("App: init (silent = {}, offline = {}, debug = {}, useLL = {}, secure = {}, nobypass = {})",
              launchInfo.Silent, launchInfo.Offline, launchInfo.Debug, launchInfo.UseLL, launchInfo.Secure,
              launchInfo.NoBypass);
    ui::FontData fd;
    fd.semiBold = kInterSemiBold;
    fd.semiBoldSize = kInterSemiBold_size;
    fd.bold = kInterBold;
    fd.boldSize = kInterBold_size;

    if (!ui::init(dev, fd)) return false;

    m_launchInfo = &launchInfo;
    m_launch = launchInfo;

    m_fieldUser = Utils::GetStrSetting("Username");

    if (launchInfo.Offline) {
        if (!m_fieldUser.empty()) {
            m_auth = AuthState::Offline;
            Log::Info("App: offline mode via -offline (prior login as '{}')", m_fieldUser);
        } else {
            Log::Warn("App: -offline ignored - no prior login found, showing the login screen");
        }
    }

    DiscordOAuth::Session restored = DiscordOAuth::EnsureLoggedIn();
    if (restored.ok) {
        std::lock_guard<std::mutex> lock(m_authMutex);
        m_auth = AuthState::Ready;
        m_user = restored.username;
        m_discordSession = restored;
        Utils::SetStrSetting("Username", restored.username);
        Auth::FetchMeta(m_version, m_changelog, m_status);
        Log::Info("App: restored Discord session for {}", restored.username);
    }

    if (launchInfo.Silent) Music::SetMuted(true);

    m_assets.load(dev);
    return true;
}

void App::shutdown() {
    Log::Info("App: shutdown");
    if (m_loginThread.joinable()) m_loginThread.join();
    ProfileBridge::Stop();
    m_inject.shutdown();
    m_assets.unload();
    ui::shutdown();
}

void App::beginDiscordAuth() {
    if (m_loginThread.joinable()) return;

    {
        std::lock_guard<std::mutex> lock(m_authMutex);
        m_auth = AuthState::Busy;
        m_authError.clear();
    }
    m_loginThread = std::thread([this]() { discordWorker(); });
}

void App::discordWorker() {
    DiscordOAuth::Session session = DiscordOAuth::EnsureLoggedIn();
    if (!session.ok) {
        session = DiscordOAuth::Authenticate();
    }

    std::lock_guard<std::mutex> lock(m_authMutex);
    if (session.ok) {
        m_user = session.username;
        m_discordSession = session;
        m_auth = AuthState::Ready;
        m_authError.clear();
        Utils::SetStrSetting("Username", session.username);
        Auth::FetchMeta(m_version, m_changelog, m_status);
    } else {
        m_authError = session.error;
        m_auth = AuthState::Login;
    }
}


void App::launch() {
    if (m_inject.active()) return;

    m_launch.Product = m_selectedProduct;

    if (m_launch.URL.empty() && m_selectedProduct <= 1) {
        const char* setting = m_selectedProduct == 1 ? "SSEUrl" : "AVX2Url";
        const char* fallback = m_selectedProduct == 1
            ? "https://YourWebsite.Here/DLLs/SSE.dll"
            : "https://YourWebsite.Here/DLLs/AVX2.dll";
        const std::string url = Utils::GetStrSetting(setting, fallback);
        m_launch.URL.assign(url.begin(), url.end());
    }

    const char* bypassMode = m_launch.NoBypass ? "nobypass" : (m_launch.Secure ? "VACSAFE forced" : "VACSAFE auto");
    Log::Info("App: launch pressed (product {}, {})", m_selectedProduct, bypassMode);

    Music::PlayLaunch();

    ProfileBridge::Start(m_discordSession);

    m_inject.start(m_launch);
}

void App::autoStart() {
    if (m_inject.active()) return;

    ProfileBridge::Start(m_discordSession);

    m_inject.start(m_launch);
}

void App::screenSplash(float alpha) {
    if (alpha <= 0.004f) return;
    Theme& t = theme();
    gfx::Image* splash = m_assets.splash.valid() ? &m_assets.splash : nullptr;

    const float p = core::clamp01(m_boot / kSplashDuration);
    const float entry = core::clamp01(m_boot / 0.55f);
    const float outT = 1.f - alpha;

    const float cx = g().width * 0.5f;
    const float cy = g().height * 0.5f;

    const float imgW = splash ? (std::min)(232.f, g().width - 64.f) : 0.f;
    const float imgH = splash ? imgW / splash->aspect() : 0.f;
    const float barW = 204.f;
    const float barH = 4.f;
    const float blockH = imgH + 16.f + barH + 24.f;

    const float pop = core::easeOutBack(entry);
    const float rise = (1.f - core::easeOutCubic(entry)) * 10.f;
    const float scale = (splash ? pop * (1.f + outT * 0.09f) : 1.f);
    const float dy = rise - outT * 14.f;
    const float imgA = splash ? alpha * core::easeOutCubic(entry) : 0.f;

    const float top = cy - blockH * 0.5f + dy - (splash ? imgH * 0.5f : 0.f);

    if (splash) {
        const Rect imgBox(cx - imgW * scale * 0.5f, top - imgH * scale * 0.5f + imgH * 0.5f * (1.f - scale),
                          imgW * scale, imgH * scale);
        if (imgA > 0.01f) {
            dl().shadow(imgBox.shrink(-4.f), t.focus.alpha(0.20f * imgA), 26.f, 14.f, Vec2(0.f, 6.f));
            dl().image(splash->srv, imgBox, Col(1.f, 1.f, 1.f, imgA), 10.f);
        }
    }

    const Rect track(cx - barW * 0.5f, top + imgH + 16.f, barW, barH);
    dl().rect(track, Col(1.f, 1.f, 1.f, 0.10f * alpha), barH * 0.5f);
    dl().border(track.shrink(0.5f), Col(1.f, 1.f, 1.f, 0.14f * alpha), 1.f, barH * 0.5f - 0.5f);

    const float fillW = track.w * core::easeInOutCubic(p);
    if (fillW > 0.5f) {
        const Rect fill(track.x, track.y, fillW, track.h);
        dl().shadow(fill, t.focus.alpha(0.30f * alpha), 8.f, barH * 0.5f, Vec2());
        dl().rect(fill, t.focus.alpha(0.9f * alpha), barH * 0.5f);

        if (fillW > 20.f) {
            const float phase = 0.5f + 0.5f * std::sin(g().time * 2.4f);
            const float shx = (std::max)(fill.x + 4.f, (std::min)(fill.r() - 16.f, fill.x + 8.f + (track.w - 16.f) * phase));
            dl().rect(Rect(shx, track.y, 12.f, track.h), Col(1.f, 1.f, 1.f, 0.38f * alpha), barH * 0.5f);
        }
    }

    const Rect labelBox(cx - barW * 0.5f, track.b() + 9.f, barW, 12.f);
    const char* steps[] = { "Loading resources", "Preparing components", "Finalizing framework" };
    const float centers[] = { 0.17f, 0.5f, 0.83f };
    for (int i = 0; i < 3; ++i) {
        const float wa = core::clamp01(1.f - std::fabs(p - centers[i]) * 7.f);
        if (wa > 0.01f)
            ui::text(fonts().body, labelBox, steps[i], Col(1.f, 1.f, 1.f, 0.75f * wa * alpha),
                     AlignH::Left, AlignV::Middle);
    }
    std::string pct = std::to_string((int)(p * 100.f)) + "%";
    ui::text(fonts().body, labelBox, pct.c_str(), Col(1.f, 1.f, 1.f, 0.55f * alpha), AlignH::Right, AlignV::Middle);
}

void App::screenLogin(const Rect& panel, float alpha) {
    if (alpha <= 0.004f) return;
    Theme& t = theme();

    AuthState auth;
    std::string authError;
    {
        std::lock_guard<std::mutex> lock(m_authMutex);
        auth = m_auth;
        authError = m_authError;
    }
    const bool busy = auth == AuthState::Busy;

    const float x = std::floor(panel.x + 22.f);
    const float w = panel.w - 44.f;
    const float y0 = panel.y + kBarHeight + 10.f;

    if (!busy) {
        const float rise = core::easeOutCubic(anim(id("login.title"), 0, 1.f, 6.f));
        Rect titleBox(panel.x, y0 + (1.f - rise) * 6.f, panel.w, 22.f);
        ui::textShadow(fonts().title, titleBox, "Sign in", t.text.alpha(alpha), AlignH::Center, AlignV::Middle,
                       0.6f, 0.6f, 1.f);
    }

    const float formY = y0 + (busy ? 0.f : 26.f);
    Rect discordRect(x, formY + 26.f, w, 38.f);

    const bool pressedButton = busy ? false : button("login.discord", discordRect, "Sign in with Discord");

    if (!busy && pressedButton) beginDiscordAuth();

    if (busy) {
        spinnerRing(Vec2(discordRect.center().x - 20.f, discordRect.center().y), 9.f, 2.f, t.focus, t.barGlow, alpha);
        ui::text(fonts().body, Rect(discordRect.x + 12.f, discordRect.y, discordRect.w - 24.f, discordRect.h),
                 "Contacting Discord...", t.text.alpha(0.85f * alpha), AlignH::Left, AlignV::Middle);
    }

    if (!authError.empty() && !busy) {
        Rect errBox(panel.x + 16.f, discordRect.b() + 8.f, panel.w - 32.f, 16.f);
        ui::text(fonts().body, errBox, authError.c_str(), Col(1.f, 0.55f, 0.52f, alpha), AlignH::Center, AlignV::Middle);
    }

}

void statusPill(const Rect& r, const std::string& status, float alpha) {
    if (status.empty()) return;
    Theme& t = theme();
    const bool detected = status == "detected";
    const bool undetected = status == "undetected";

    Col fill = Col::hex(0x292929, 0.90f * alpha);
    Col accent = Col::hex(0x696969, alpha);
    const char* label = "Unknown";
    if (detected) { accent = Col::hex(0xFF5A5A, alpha); label = "Detected"; }
    if (undetected) { accent = Col::hex(0x4ADE80, alpha); label = "Undetected"; }

    const float breathe = 0.5f + 0.5f * std::sin(g().time * (undetected ? 2.6f : 4.f));
    const float pulse = undetected || detected ? 0.6f + 0.4f * breathe : 0.35f;

    if (undetected || detected)
        dl().shadow(r, accent.alpha((0.16f + 0.14f * breathe) * alpha), 12.f, r.h * 0.5f, Vec2());

    dl().rect(r, fill, r.h * 0.5f);
    dl().border(r.shrink(0.5f), accent.alpha((0.25f + 0.45f * pulse) * alpha), 1.f, r.h * 0.5f - 0.5f);

    dl().circle(Vec2(r.x + 12.f, r.center().y), 2.6f + breathe * (undetected ? 1.0f : 0.3f), accent.alpha(alpha * pulse));

    ui::text(fonts().body, Rect(r.x + 20.f, r.y, r.w - 20.f, r.h), label, t.text.alpha(0.92f * alpha),
             AlignH::Left, AlignV::Middle);
}

void App::screenProduct(const Rect& panel, float alpha) {
    if (alpha <= 0.004f) return;
    Theme& t = theme();

    AuthState auth;
    std::string user, version, status;
    {
        std::lock_guard<std::mutex> lock(m_authMutex);
        auth = m_auth;
        user = m_user;
        version = m_version;
        status = m_status;
    }
    const bool onlined = auth == AuthState::Ready;
    const float top = panel.y + kBarHeight;

    const float lx = panel.x + 14.f;
    const float lw = 180.f;
    Rect leftCard(lx, top + 8.f, lw, panel.b() - 8.f - top - 8.f);
    const float cardR = 10.f;
    dl().rect(leftCard, t.row.alpha(0.0f), cardR);
    dl().border(leftCard.shrink(0.5f), Col::hex(0xFFFFFF, 0.06f * alpha), 1.f, cardR - 0.5f);

    ui::text(fonts().body, Rect(leftCard.x + 12.f, leftCard.y + 10.f, leftCard.w - 24.f, 14.f), "SOFTWARE",
             Col::hex(0xFFFFFF, 0.42f * alpha), AlignH::Left, AlignV::Middle);

    if (!m_payloadsInit) {
        m_payloadsInit = true;
        m_payloads = Utils::EnumSidecarPayloads();
    }

    const auto toUtf8 = [](const std::wstring& w) {
        if (w.empty()) return std::string();
        const int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string out(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], len, nullptr, nullptr);
        return out;
    };

    struct Product { int id; std::string name; std::string version; std::string desc; std::string rowId; };
    std::vector<Product> products;
    products.reserve(m_payloads.size() + 2);
    products.push_back({ 0, "AstralMod AVX2", "AVX2", "Optimized for CPUs with AVX2 support", "product.avx2" });
    products.push_back({ 1, "AstralMod SSE", "SSE", "Compatibility build for older CPUs", "product.sse" });
    int i = 2;
    for (const std::wstring& path : m_payloads) {
        std::wstring stem = std::filesystem::path(path).stem().wstring();
        if (stem.empty()) stem = path;
        products.push_back({
            i,
            toUtf8(stem),
            "External",
            "An externally detected DLL",
            "product." + std::to_string(i),
        });
        ++i;
    }
    const int kRowCount = (int)products.size();

    const auto findProduct = [&](int id) -> const Product& {
        for (const Product& p : products)
            if (p.id == id) return p;
        return products[0];
    };

    const float rowH = 40.f;
    const float rowGap = 4.f;
    dl().pushClip(leftCard);
    for (int i = 0; i < kRowCount; ++i) {
        Rect r(leftCard.x + 8.f, leftCard.y + 30.f + i * (rowH + rowGap), leftCard.w - 16.f, rowH);
        if (gameRow(products[i].rowId.c_str(), r, nullptr, products[i].name.c_str(), m_selectedProduct == products[i].id))
            m_selectedProduct = products[i].id;
    }
    dl().popClip();

    const Product& sel = findProduct(m_selectedProduct);

    const float rx = panel.x + leftCard.r() + 16.f;
    const float rw = panel.r() - rx - 14.f;
    const float ry = top + 8.f;
    const float rh = panel.b() - 8.f - ry;
    Rect right(rx, ry, rw, rh);

    const float stagger = core::easeOutCubic(anim(id("product.stagger"), 0, 1.f, 7.f));
    const float dy = (1.f - stagger) * 10.f;

    float markS = 34.f;
    gfx::Image* markImg = &m_assets.framework;
    const float markAspect = markImg && markImg->valid() ? markImg->aspect() : 1.f;
    Rect markBox(right.x, right.y + dy, markS * markAspect, markS);
    logoMark(markImg, markBox, alpha * stagger);

    Rect nameBox(markBox.r() + 10.f, right.y + dy - 3.f, right.w - markBox.r() - 10.f, 20.f);
    ui::textShadow(fonts().title, nameBox, sel.name.c_str(), t.text.alpha(alpha),
                   AlignH::Left, AlignV::Middle, 0.3f, 0.6f, 1.f);

    Rect pill(right.x, nameBox.b() + 20.f + dy, 96.f, 20.f);
    statusPill(pill, status.empty() && !onlined ? XorStr("unknown").str() : status, alpha);

    Rect buildBox(pill.r() + 10.f, pill.y, right.r() - pill.r() - 10.f, pill.h);
    std::string buildText = "Build " + (version.empty() ? Auth::ClientVersion() : version);
    if (!Auth::BuildId().empty()) buildText += "  #" + Auth::BuildId();
    ui::text(fonts().body, buildBox, buildText.c_str(), Col::hex(0xFFFFFF, 0.55f * alpha), AlignH::Right, AlignV::Middle);

    if (onlined) {
        Rect outRect(right.r() - 64.f, markBox.center().y - 8.f, 64.f, 16.f);
        const uint32_t outId = id("product.logout");
        if (clicked(outId, outRect)) {
            DiscordOAuth::Logout();
            std::lock_guard<std::mutex> lock(m_authMutex);
            m_auth = AuthState::Login;
            m_authError.clear();
        }
        const bool outOver = hovered(outId, outRect);
        const float outT = anim(outId, 0, outOver ? 1.f : 0.f, 14.f);
        ui::text(fonts().body, outRect, "Sign out", t.focus.alpha((0.5f + 0.5f * outT) * alpha), AlignH::Right,
                 AlignV::Middle);
    }

    Rect descBox(right.x, pill.b() + 14.f + dy, right.w, 16.f);
    const std::string desc = sel.desc;
    const size_t nl = desc.find('\n');
    const std::string line1 = nl == std::string::npos ? desc : desc.substr(0, nl);
    ui::text(fonts().body, descBox, line1.c_str(), Col::hex(0xFFFFFF, 0.52f * alpha), AlignH::Left, AlignV::Middle);
    if (nl != std::string::npos && nl + 1 < desc.size())
        ui::text(fonts().body, Rect(right.x, descBox.y + 16.f, right.w, 16.f), desc.substr(nl + 1).c_str(),
                 Col::hex(0xFFFFFF, 0.52f * alpha), AlignH::Left, AlignV::Middle);

    Rect devBox(right.x, descBox.y + 36.f, 92.f, 18.f);
    dl().rect(devBox, t.focus.alpha(0.12f * alpha), 9.f);
    dl().border(devBox.shrink(0.5f), t.focus.alpha(0.32f * alpha), 1.f, 8.5f);
    ui::text(fonts().body, devBox, sel.version.c_str(), t.focus.alpha(0.92f * alpha),
             AlignH::Center, AlignV::Middle);

    Rect noteBox(right.x, devBox.b() + 10.f, right.w, 16.f);
    if (!m_changelog.empty()) {
        std::string line = m_changelog;
        const auto nl = line.find('\n');
        if (nl != std::string::npos) line = line.substr(0, nl);
        ui::text(fonts().body, noteBox, line.c_str(), Col::hex(0xFFFFFF, 0.42f * alpha), AlignH::Left, AlignV::Middle);
    }

    const std::string who = onlined ? "Signed in as " + user : "Offline mode - using bundled build";
    const std::string updated = "Updated " + std::string(buildStamp());
    ui::text(fonts().body, Rect(right.x, right.b() - 114.f, right.w, 16.f), who.c_str(),
             Col::hex(0xFFFFFF, 0.42f * alpha), AlignH::Left, AlignV::Middle);
    ui::text(fonts().body, Rect(right.x, right.b() - 98.f, right.w, 16.f), updated.c_str(),
             Col::hex(0xFFFFFF, 0.30f * alpha), AlignH::Left, AlignV::Middle);

    Rect launchBtn(right.x, right.b() - 34.f, right.w, 30.f);
    if (button("product.launch", launchBtn, "Secure Launch")) launch();
}

void App::screenStatus(const Rect& panel, float alpha) {
    if (alpha <= 0.004f) return;
    Theme& t = theme();

    Rect content(panel.x, panel.y + kBarHeight, panel.w, panel.h - kBarHeight);

    const Stage stage = m_inject.stage();
    const float bgAlpha = stage == Stage::Done ? alpha * 0.75f : alpha;

    dl().gradientVMasked(content, Col::hex(0x1A1A1A, 0.55f * bgAlpha), Col::hex(0x1A1A1A, 0.94f * bgAlpha), panel,
                         t.radius);

    const float cycle = 1.15f;
    const float phase = std::fmod(g().time, cycle) / cycle;
    const float beat = 0.5f - 0.5f * std::cos(phase * core::kPi * 2.f);

    const float rise = core::easeOutCubic(anim(id("logo.rise"), 0, 1.f, 5.5f));
    gfx::Image* mark = m_assets.tf2Hero.valid() ? &m_assets.tf2Hero : &m_assets.logo;
    const float markAspect = mark->valid() ? mark->aspect() : 1.f;
    const float markW = content.w * 0.62f * (1.f + beat * 0.038f);
    const float markH = markW / markAspect;
    Rect markBox(content.center().x - markW * 0.5f, content.y + content.h * 0.33f - markH * 0.5f + (1.f - rise) * 8.f,
                 markW, markH);
    logoMark(mark, markBox, bgAlpha * rise * (0.86f + beat * 0.14f));

    const float spin = g().time * 0.6f;
    for (int i = 0; i < 6; ++i) {
        const float a = spin + i * core::kPi / 3.f;
        const float rr = markW * (0.52f + 0.05f * std::sin(g().time * 1.8f + i * 1.3f));
        const Vec2 p(markBox.center().x + std::cos(a) * rr, markBox.center().y + std::sin(a) * rr);
        dl().star(p, 1.6f + 0.7f * (0.5f + 0.5f * std::sin(g().time * 3.f + i * 2.f)), 0.85f,
                  Col::hex(0xD0D0D0, 0.5f * bgAlpha));
    }

    const char* label = stageLabel(stage, m_inject.errorText());
    if (label && *label) {
        Rect textBox(content.x + 10.f, markBox.b() + 10.f, content.w - 20.f, 18.f);
        const Col labelCol = stage == Stage::Failed ? Col(1.f, 0.55f, 0.52f, bgAlpha) : t.text.alpha(bgAlpha);
        ui::textShadow(fonts().body, textBox, label, labelCol, AlignH::Center, AlignV::Top, 0.2f, 0.5f, 1.f);
    }

    const float lineW = 96.f;
    const float lineAlpha = stage == Stage::Done ? bgAlpha * (1.f - (m_doneT / 1.2f)) : bgAlpha;
    if (lineAlpha > 0.004f) {
        Rect lineBox(std::floor(content.center().x - lineW * 0.5f), std::floor(content.b() - 24.f), lineW, 4.f);
        loadingLine(lineBox, phase, g().time, lineAlpha);
    }
}

void App::frame(float dt) {
    if (!m_bootDone) {
        m_boot += dt;
        if (m_boot >= kSplashDuration) m_bootDone = true;
    }
    m_openT = core::clamp01(m_openT + dt / 0.26f);
    m_fadeIn = core::easeOutCubic(m_openT);

    if (m_bootDone) m_winT = core::clamp01(m_winT + dt / 0.32f);

    if (m_closing) {
        m_closeT = core::clamp01(m_closeT + dt / 0.19f);
        m_fade = 1.f - m_closeT * m_closeT;
    }
    m_inject.update(dt);
    Music::Update();
    Theme& t = theme();

    if (m_inject.done() && !m_doneClosing) {
        m_doneT += dt;
        if ((ProfileBridge::Delivered() && m_doneT >= 0.6f) || m_doneT >= 15.0f) {
            m_doneClosing = true;
            m_closing = true;
        }
    }

    const Stage cur = m_inject.stage();
    if (cur != m_lastStage) {
        m_lastStage = cur;
        const char* label = stageLabel(cur, m_inject.errorText());
        Log::Info("App: stage -> {:d} ({})", static_cast<int>(cur), label ? label : "?");
        if (label && *label) {
            DiscordRpc::Update(label, XorStr("AstralMod Loader").str(), "logo", XorStr("AstralMod").str());
        }

        switch (cur) {
        case Stage::WaitingForSteam:
        case Stage::EnablingBypass:
            Music::PlayLaunch();
            break;
        case Stage::WaitingForGame:
            Music::PlayInject();
            break;
        case Stage::WaitingForClient:
        case Stage::Loading:
            Music::PlayLaunch();
            break;
        default:
            break;
        }
    }

    AuthState auth;
    {
        std::lock_guard<std::mutex> lock(m_authMutex);
        auth = m_auth;
    }

    const bool status = m_inject.active();

    if (m_bootDone && !m_musicStarted) {
        m_musicStarted = true;
        Log::Info("App: loading finished - starting music");
        Music::PlayInit();
    }

    m_bootBlend = anim(id("boot.blend"), 0, m_bootDone ? 0.f : 1.f, 12.f);
    m_statusBlend = anim(id("status.blend"), 0, status ? 1.f : 0.f, 10.f);

    if (!m_bootDone) {
        m_target.width = 300.f;
        m_target.height = 160.f;
    } else if (status) {
        m_target.width = 372.f;
        m_target.height = 236.f;
    } else if (auth == AuthState::Login || auth == AuthState::Busy) {
        m_target.width = 392.f;
        m_target.height = 264.f;
    } else {
        m_target.width = 520.f;
        m_target.height = 336.f;
    }

    if (!m_sizeInit) {
        m_curW = m_target.width;
        m_curH = m_target.height;
        m_sizeInit = true;
    }
    core::spring(m_curW, m_velW, m_target.width, 260.f, 30.f, dt);
    core::spring(m_curH, m_velH, m_target.height, 260.f, 30.f, dt);

    const float scale = (1.f - m_closeT * 0.05f) * core::lerp(0.88f, 1.f, core::easeOutCubic(m_winT));
    const float pw = std::floor(m_curW * scale);
    const float ph = std::floor(m_curH * scale);
    Rect panel(std::floor((g().width - pw) * 0.5f), std::floor((g().height - ph) * 0.5f), pw, ph);
    m_panel = panel;

    const float win = 1.f - m_bootBlend;
    if (win > 0.004f) {
        {
            const LiquidPalette target = liquidPalette(m_selectedProduct);
            if (!m_bgInit) {
                m_bgA = target.a;
                m_bgB = target.b;
                m_rainbowMix = target.rainbow ? 1.f : 0.f;
                m_bgInit = true;
            }
            m_bgA.r = core::approach(m_bgA.r, target.a.r, kPaletteSpeed, dt);
            m_bgA.g = core::approach(m_bgA.g, target.a.g, kPaletteSpeed, dt);
            m_bgA.b = core::approach(m_bgA.b, target.a.b, kPaletteSpeed, dt);
            m_bgB.r = core::approach(m_bgB.r, target.b.r, kPaletteSpeed, dt);
            m_bgB.g = core::approach(m_bgB.g, target.b.g, kPaletteSpeed, dt);
            m_bgB.b = core::approach(m_bgB.b, target.b.b, kPaletteSpeed, dt);
            m_rainbowMix = core::approach(m_rainbowMix, target.rainbow ? 1.f : 0.f, kPaletteSpeed, dt);

            const Rect bg = panel.expand(0.75f);
            dl().liquid(bg, m_bgA, m_bgB, m_rainbowMix, win, t.radius);
            dl().rect(bg, Col(0.f, 0.f, 0.f, 200.f / 255.f * win), t.radius, 0.f);
        }

        dl().shadow(panel.shrink(3.f), Col(0.f, 0.f, 0.f, 0.44f * win), 28.f, t.radius, Vec2(0.f, 9.f));

        dl().rect(panel, t.body.alpha(0.62f * win), t.radius);

        const float content = 1.f - m_bootBlend;
        if (status) {
            screenStatus(panel, content * m_statusBlend);
        } else if (auth == AuthState::Login || auth == AuthState::Busy) {
            screenLogin(panel, content);
        } else {
            screenProduct(panel, content);
        }

        titleBar(panel, kBarHeight, XorStr("Framework Unified Loader").c_str(), g().time, &m_assets.banner, win);

        Rect closeBtn(panel.r() - 30.f, panel.y + kBarHeight * 0.5f - 11.f, 22.f, 22.f);
        if (closeButton("chrome.close", closeBtn) && !m_closing) m_closing = true;
    }

    screenSplash(m_bootBlend);
}

}
