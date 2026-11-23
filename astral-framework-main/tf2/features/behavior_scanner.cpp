#include "base.hpp"
#include "gui/theme.hpp"
#include "steam.hpp"

#include "behavior_scanner.hpp"
#include "entity_cache.hpp"
#include "party_network.hpp"

ASTRAL_NO_DESTROY behavior_scanner g_behavior_scanner{ };

namespace
{
constexpr float  FLAG_SCORE{ 100.0f };
constexpr float  SCORE_DECAY{ 2.0f };
constexpr float  SHOT_SNAP_DEGREES{ 35.0f };
constexpr float  SHOT_SNAP_WEIGHT{ 34.0f };
constexpr i32    CHOKE_TICKS{ MAX_NEW_COMMANDS };
constexpr i32    CHOKE_STREAK{ 4 };
constexpr float  CHOKE_WEIGHT{ 15.0f };
}

void behavior_scanner::clear( )
{
    for ( record_t &record : this->m_records )
    {
        record = record_t{ };
    }
}

void behavior_scanner::adopt( record_t &record, i32 index, u32 handle )
{
    record          = record_t{ };
    record.m_handle = handle;

    player_info_t info{ };
    record.m_eligible = g_tf2.m_engine->GetPlayerInfo( index, &info ) && info.friendsID &&
                        !info.fakeplayer && !info.ishltv && !info.isreplay;
    record.m_account = info.friendsID;
}

bool behavior_scanner::excluded( const record_t &record ) const
{
    const u32 marks{ g_cfg.player_marks( record.m_account ) };
    return g_steam.is_friend( record.m_account ) ||
           g_party_network.has_peer( record.m_account ) ||
           g_cfg.player_tag( record.m_account ) == config::PLAYER_TAG_FRIEND ||
           ( marks & ( config::PLAYER_MARK_CHEATER | config::PLAYER_MARK_SUSPECTED_CHEATER ) );
}

void behavior_scanner::on_network_update( )
{
    if ( !g_cfg.m_misc.m_behavior_scanner || g_tf2.m_engine->IsPlayingDemo( ) ||
         g_tf2.sv_cheats->GetInt( ) )
    {
        return;
    }

    const i32 server_tick{ g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick };
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    for ( const auto *cached : g_entity_cache.players( ) )
    {
        const i32 index{ cached->m_index };
        if ( index == local || index < 1 || index >= MAX_TRACKED_PLAYERS )
        {
            continue;
        }

        record_t &record{ this->m_records[ index ] };
        if ( record.m_handle != cached->m_handle || !record.m_eligible )
        {
            this->adopt( record, index, cached->m_handle );
        }
        if ( !record.m_eligible || record.m_flagged || this->excluded( record ) )
        {
            record.m_flagged |= this->excluded( record );
            continue;
        }

        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( player->IsDormant( ) || !player->IsAlive( ) )
        {
            record.m_tracked = false;
            continue;
        }

        const i32 sim_tick{ TIME_TO_TICKS( player->m_flSimulationTime( ) ) };
        if ( record.m_tracked && sim_tick == record.m_sim_tick )
        {
            record.m_seen_tick = server_tick;
            continue;
        }

        const vec3 eye{ player->m_angEyeAngles( ).x, player->m_angEyeAngles( ).y, 0.0f };
        if ( record.m_tracked )
        {
            record.m_previous_eye = record.m_eye;
            if ( record.m_seen_tick - record.m_changed_tick >= CHOKE_TICKS )
            {
                if ( ++record.m_choke_streak == CHOKE_STREAK )
                {
                    record.m_choke_streak = 0;
                    this->add_score( record, index, CHOKE_WEIGHT,
                                     ASTRAL_XORSTR_STATIC( "repeated packet choke" ) );
                }
            } else
            {
                record.m_choke_streak = 0;
            }
        } else
        {
            record.m_previous_eye = eye;
            record.m_choke_streak = 0;
        }

        record.m_eye          = eye;
        record.m_sim_tick     = sim_tick;
        record.m_changed_tick = server_tick;
        record.m_seen_tick    = server_tick;
        record.m_tracked      = true;
    }
}

void behavior_scanner::on_fire_bullets( i32 shooter )
{
    if ( !g_cfg.m_misc.m_behavior_scanner || shooter < 1 || shooter >= MAX_TRACKED_PLAYERS )
    {
        return;
    }

    record_t &record{ this->m_records[ shooter ] };
    if ( !record.m_tracked || record.m_flagged || this->excluded( record ) ||
         record.m_changed_tick != g_tf2.m_cl->m_ClockDriftMgr.m_nServerTick )
    {
        return;
    }

    if ( math::calc_fov( record.m_previous_eye, record.m_eye ) >= SHOT_SNAP_DEGREES )
    {
        this->add_score( record, shooter, SHOT_SNAP_WEIGHT,
                         ASTRAL_XORSTR_STATIC( "repeated shot snaps" ) );
    }
}

void behavior_scanner::add_score( record_t &record, i32 index, float amount, const char *reason )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const float decay{ static_cast< float >( now - record.m_score_time ) * SCORE_DECAY };
    record.m_score      = math::bigger( record.m_score - decay, 0.0f ) + amount;
    record.m_score_time = now;
    if ( record.m_score >= FLAG_SCORE )
    {
        this->flag( record, index, reason );
    }
}

void behavior_scanner::flag( record_t &record, i32 index, const char *reason )
{
    if ( this->excluded( record ) )
    {
        record.m_flagged = true;
        return;
    }

    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( index, &info ) )
    {
        return;
    }

    // Heuristics never promote a player to Cheater and never submit a cloud report.
    const u32 marks{ g_cfg.player_marks( record.m_account ) };
    if ( !g_cfg.set_player_marks( record.m_account,
                                  marks | config::PLAYER_MARK_SUSPECTED_CHEATER ) )
    {
        return;
    }

    g_cl.mark_party_suspected( record.m_account );

    record.m_flagged = true;
    char message[ 128 ]{ };
    str::format_to( message, sizeof( message ), ASTRAL_XORSTR( "Behavior Scanner: {}" ).get( ), reason );
    push_notification( notify_kind_t::warning, ICON_EYE, g_cl.get_player_name( index ), message );
}
