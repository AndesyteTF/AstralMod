#pragma once
#include <d3d11.h>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "core/types.h"
#include "app/assets.h"
#include "app/inject.h"
#include "Loader/Auth/DiscordOAuth.h"
#include "LaunchInfo.h"

namespace app {

enum class AuthState : int { Login, Busy, Ready, Offline };

struct PanelSize {
    float width = 0.f;
    float height = 0.f;
};

constexpr float kTitleBarHeight = 32.f;

class App {
public:
    bool init(ID3D11Device* dev, const LaunchInfo& launchInfo);
    void shutdown();
    void frame(float dt);

    core::Rect panelRect() const { return m_panel; }
    void requestClose() { m_closing = true; }
    bool finished() const { return m_closing && m_closeT >= 1.f; }
    float fadeAlpha() const { return m_fadeIn * m_fade; }
    void autoStart();

private:
    void screenSplash(float alpha);
    void screenLogin(const core::Rect& panel, float alpha);
    void screenProduct(const core::Rect& panel, float alpha);
    void screenStatus(const core::Rect& panel, float alpha);

    void beginDiscordAuth();
    void discordWorker();
    void launch();

    int m_selectedProduct = 0;

    std::vector<std::wstring> m_payloads;
    bool m_payloadsInit = false;

    core::Col m_bgA;
    core::Col m_bgB;
    float m_rainbowMix = 0.f;
    bool m_bgInit = false;

    Assets m_assets;
    InjectFlow m_inject;
    const LaunchInfo* m_launchInfo = nullptr;
    LaunchInfo m_launch;

    std::thread m_loginThread;
    std::mutex m_authMutex;
    AuthState m_auth = AuthState::Login;
    std::string m_authError;
    std::string m_user;
    std::string m_version;
    std::string m_changelog;
    std::string m_status;
    std::string m_expires;

    std::string m_fieldUser;

    DiscordOAuth::Session m_discordSession;

    Stage m_lastStage = Stage::Idle;
    bool m_rpcBooted = false;

    bool m_musicStarted = false;

    PanelSize m_target;
    core::Rect m_panel;
    float m_curW = 0.f, m_curH = 0.f;
    float m_velW = 0.f, m_velH = 0.f;
    bool m_sizeInit = false;
    float m_boot = 0.f;
    bool m_bootDone = false;
    float m_bootBlend = 1.f;
    float m_statusBlend = 0.f;
    bool m_closing = false;
    float m_closeT = 0.f;
    float m_fade = 1.f;
    float m_fadeIn = 0.f;
    float m_winT = 0.f;
    float m_openT = 0.f;
    float m_doneT = 0.f;
    bool m_doneClosing = false;
};

const char* buildStamp();

const char* stageLabel(Stage s, const char* error);

}
