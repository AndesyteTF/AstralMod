#include "telemetry_blocker.hpp"

ATLAS_NO_DESTROY telemetry_blocker g_telemetry_blocker{ };

void telemetry_blocker::apply_convars( )
{
    for ( usize index{ }; index < CONVAR_COUNT; ++index )
    {
        const char *name{ };
        bool        is_float{ false };

        switch ( index )
        {
            case CONVAR_TF_STATS_TRACK: name = ATLAS_XORSTR_STATIC( "tf_stats_track" ); break;
            case CONVAR_STEAMWORKS_STATS_DISABLE: name = ATLAS_XORSTR_STATIC( "steamworks_stats_disable" ); break;
            case CONVAR_STEAMWORKS_SESSIONID_CLIENT: name = ATLAS_XORSTR_STATIC( "steamworks_sessionid_client" ); break;
            case CONVAR_STEAMWORKS_SESSIONID_SERVER: name = ATLAS_XORSTR_STATIC( "steamworks_sessionid_server" ); break;
            case CONVAR_REPLAY_ENABLE: name = ATLAS_XORSTR_STATIC( "replay_enable" ); break;
            case CONVAR_TF_MATCHMAKING_OGS_ODDS: name = ATLAS_XORSTR_STATIC( "tf_matchmaking_ogs_odds" ); is_float = true; break;
            default: break;
        }

        convar_state_t &state{ this->m_convars[ index ] };
        state.m_convar = name ? g_tf2.m_cvar->FindVar( name ) : nullptr;
        state.m_found  = false;

        if ( !state.m_convar )
        {
            continue;
        }

        state.m_is_float = is_float;
        if ( is_float )
        {
            state.m_float = state.m_convar->GetFloat( );
            state.m_convar->SetValue( 0.f );
        } else
        {
            state.m_int = state.m_convar->GetInt( );
            state.m_convar->SetValue( 0 );
        }

        state.m_found = true;
    }
}

void telemetry_blocker::restore_convars( )
{
    for ( convar_state_t &state : this->m_convars )
    {
        if ( state.m_found && state.m_convar )
        {
            if ( state.m_is_float )
            {
                state.m_convar->SetValue( state.m_float );
            } else
            {
                state.m_convar->SetValue( state.m_int );
            }
        }

        state.m_found = false;
    }
}

void telemetry_blocker::service( )
{
    if ( this->m_shutdown || !g_tf2.m_cvar )
    {
        return;
    }

    const usize desired{ g_cfg.m_misc.m_telemetry_blocker };
    if ( desired >= config::TELEMETRY_BLOCKER_COUNT || desired == this->m_applied_mode )
    {
        return;
    }

    // Re-read the game's values every time the mode changes so restoring always
    // puts back what the game had rather than what we stored on an earlier mode.
    this->restore_convars( );

    if ( desired != config::TELEMETRY_BLOCKER_OFF )
    {
        this->apply_convars( );
    }

    this->m_applied_mode = desired;
}

void telemetry_blocker::shutdown( )
{
    this->m_shutdown = true;
    this->restore_convars( );
    this->m_applied_mode = config::TELEMETRY_BLOCKER_COUNT;
}

void telemetry_blocker::scrub_game_stats( KeyValues *stats )
{
    if ( !stats )
    {
        return;
    }

    stats->SetString( ATLAS_XORSTR_STATIC( "srcid" ), "" );
    stats->SetString( ATLAS_XORSTR_STATIC( "CPUID" ), "" );
    stats->SetInt( ATLAS_XORSTR_STATIC( "CPUGhz" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "CPUModel" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "CPUFeatures0" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "CPUFeatures1" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "CPUFeatures2" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "NumCores" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "PhysicalRamMbTotal" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "PhysicalRamMbAvailable" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "VirtualRamMbTotal" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "VirtualRamMbAvailable" ), 0 );
    stats->SetString( ATLAS_XORSTR_STATIC( "GPUDrv" ), "" );
    stats->SetInt( ATLAS_XORSTR_STATIC( "GPUVendor" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "GPUDeviceID" ), 0 );
    stats->SetString( ATLAS_XORSTR_STATIC( "GPUDriverVersion" ), "" );
    stats->SetInt( ATLAS_XORSTR_STATIC( "DxLvl" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "Width" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "Height" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "Windowed" ), 0 );
    stats->SetInt( ATLAS_XORSTR_STATIC( "MaxDxLevel" ), 0 );
}
