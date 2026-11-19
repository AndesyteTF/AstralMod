#pragma once

#include "../Auth/DiscordOAuth.h"

namespace ProfileBridge
{
    void Start(const DiscordOAuth::Session& session);
    void Stop();
    bool Delivered();

    // PID of the module that confirmed it applied the profile ("I got the info
    // and I'm using it now"), or 0 when nobody confirmed yet. Used by the
    // loader exit path to decide when a full close is safe.
    unsigned long ClaimedPid();

    // True while the pipe server thread is still serving the profile. Used by
    // the loader exit path to decide whether to keep the process alive briefly
    // after the window closes (see WinMain).
    bool Active();
}
