#include "app/assets.h"
#include "app/embedded_assets.h"
#include "Utils/Log.h"
#include "Utils/Utils.h"
#include "../../../resource.h"
#include <windows.h>
#include <shlwapi.h>
#include <exception>
#include <string>

#pragma comment(lib, "shlwapi.lib")

namespace app {

static bool tryLoad(ID3D11Device* dev, const std::wstring& path, gfx::Image& out) {
    if (path.empty() || GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    return gfx::loadImage(dev, path, out);
}

static void loadResourceImage(ID3D11Device* dev, int id, gfx::Image& out) {
    try {
        const Binary data = Utils::GetBinaryResource(id);
        if (!data.empty()) gfx::loadImageMemory(dev, data.data(), data.size(), out);
    }
    catch (const std::exception& ex) {
        Log::Warn("Assets: resource {} failed - {}", id, ex.what());
    }
}

static std::wstring exeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    PathRemoveFileSpecW(path);
    return path;
}

void Assets::load(ID3D11Device* dev) {
    const std::wstring dir = exeDir();

    if (!tryLoad(dev, dir + L"\\assets\\tf2_icon.png", tf2Icon))
        gfx::loadImageMemory(dev, kTf2IconPng, kTf2IconPng_size, tf2Icon);

    if (!tryLoad(dev, dir + L"\\assets\\tf2_hero.png", tf2Hero))
        loadResourceImage(dev, IDR_ART_TF2HERO, tf2Hero);

    if (!tryLoad(dev, dir + L"\\assets\\logo.png", logo))
        loadResourceImage(dev, IDR_ART_LOGO, logo);

    if (!tryLoad(dev, dir + L"\\assets\\banner.png", banner))
        loadResourceImage(dev, IDR_ART_BANNER, banner);

    if (!tryLoad(dev, dir + L"\\assets\\splash.png", splash))
        loadResourceImage(dev, IDR_SPLASH, splash);

    if (!tryLoad(dev, dir + L"\\assets\\logo.png", framework))
        loadResourceImage(dev, IDR_ART_LOGO, framework);

    if (!tryLoad(dev, dir + L"\\assets\\astral.png", astral))
        gfx::loadImageMemory(dev, kAstralPng, kAstralPng_size, astral);

    if (!tryLoad(dev, dir + L"\\assets\\phobia.png", phobia))
        gfx::loadImageMemory(dev, kPhobiaPng, kPhobiaPng_size, phobia);

    if (!tryLoad(dev, dir + L"\\assets\\jvnkbin.ico", jvnkbin))
        gfx::loadImageMemory(dev, kJvnkbinIco, kJvnkbinIco_size, jvnkbin);
}

void Assets::unload() {
    gfx::releaseImage(tf2Icon);
    gfx::releaseImage(tf2Hero);
    gfx::releaseImage(banner);
    gfx::releaseImage(splash);
    gfx::releaseImage(logo);
    gfx::releaseImage(framework);
    gfx::releaseImage(astral);
    gfx::releaseImage(phobia);
    gfx::releaseImage(jvnkbin);
}

}
