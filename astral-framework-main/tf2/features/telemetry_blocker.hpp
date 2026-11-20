#pragma once

#include "link.hpp"

class KeyValues;

class telemetry_blocker
{
public:
    void service( );
    void shutdown( );

    void scrub_game_stats( KeyValues *stats );

private:
    enum : usize
    {
        CONVAR_TF_STATS_TRACK,
        CONVAR_STEAMWORKS_STATS_DISABLE,
        CONVAR_STEAMWORKS_SESSIONID_CLIENT,
        CONVAR_STEAMWORKS_SESSIONID_SERVER,
        CONVAR_REPLAY_ENABLE,
        CONVAR_TF_MATCHMAKING_OGS_ODDS,

        CONVAR_COUNT,
    };

    struct convar_state_t
    {
        ConVar *m_convar{ };
        i32     m_int{ };
        f32     m_float{ };
        bool    m_is_float{ };
        bool    m_found{ };
    };

    void apply_convars( );
    void restore_convars( );

    convar_state_t m_convars[ CONVAR_COUNT ]{ };
    usize          m_applied_mode{ config::TELEMETRY_BLOCKER_COUNT };
    bool           m_shutdown{ false };
};

extern telemetry_blocker g_telemetry_blocker;
