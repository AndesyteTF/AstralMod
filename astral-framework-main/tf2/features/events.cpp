#include "events.hpp"

#include "auto_vacc.hpp"
#include "astral_sense.hpp"
#include "crits.hpp"
#include "esp.hpp"
#include "event_notifiers.hpp"
#include "hitmarker.hpp"
#include "misc.hpp"
#include "player_data.hpp"
#include "player_cloud.hpp"
#include "resolver.hpp"

ASTRAL_NO_DESTROY game_events g_events{ };

bool game_events::listen( const char *name )
{
    return g_tf2.m_game_events->AddListener( this, name, false );
}

bool game_events::install( )
{
    if ( this->m_installed || !g_tf2.m_game_events )
    {
        return this->m_installed;
    }

    this->m_installed = this->listen( ASTRAL_XORSTR( "item_pickup" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_hurt" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "stats_resetround" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_death" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_spawn" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_connect_client" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_disconnect" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_changeclass" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "vote_cast" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_buff" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "teamplay_round_win" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "teamplay_point_captured" ).get( ) ) &&
                        this->listen( ASTRAL_XORSTR( "player_chargedeployed" ).get( ) );

    return this->m_installed;
}

void game_events::remove( )
{
    if ( this->m_installed && g_tf2.m_game_events )
    {
        g_tf2.m_game_events->RemoveListener( this );
        this->m_installed = false;
    }
}

void game_events::FireGameEvent( IGameEvent *event )
{
    if ( !event || !g_cl.m_is_running )
    {
        return;
    }

    const hash_t name{ HASH_RT( event->GetName( ) ) };
    astral_sense_game_event( event );
    g_event_notifiers.on_game_event( event, name );

    switch ( name )
    {
        case HASH_CT( "player_hurt" ):
        {
            g_crits.on_player_hurt( event );
            g_auto_vacc.on_player_hurt( event );
            g_resolver.on_player_hurt( event );
            g_hitmarker.on_hurt( event );
            break;
        }

        case HASH_CT( "stats_resetround" ):
        {
            g_crits.reset_round_history( );
            break;
        }

        case HASH_CT( "player_buff" ):
        {
            g_player_data.on_player_buff(
                g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "userid" ).get( ) ) ),
                g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "buff_owner" ).get( ) ) ) );
            break;
        }

        case HASH_CT( "player_death" ):
        {
            astral_sense_player_death( event );
            g_hitmarker.on_death( event );
            g_misc.noisemaker_notify_event( event );
            // -- FALLTHROUGH --
        }

        case HASH_CT( "player_spawn" ):
        case HASH_CT( "player_disconnect" ):
        {
            const i32 index{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "userid" ).get( ) ) ) };
            if ( name == HASH_CT( "player_disconnect" ) )
            {
                g_player_cloud.on_player_disconnected( index );
                g_cl.invalidate_party_roster( );
            }
            g_crits.on_player_life_change( index );
            const bool dead{ name == HASH_CT( "player_disconnect" ) ||
                             ( name == HASH_CT( "player_death" ) &&
                               !( event->GetInt( ASTRAL_XORSTR( "death_flags" ).get( ) ) & TF_DEATH_FEIGN_DEATH ) ) };
            g_player_data.on_life_change( index, dead );
            break;
        }

        case HASH_CT( "player_connect_client" ):
        {
            const i32 index{ event->GetInt( ASTRAL_XORSTR( "index" ).get( ) ) + 1 };
            g_player_cloud.on_player_connected( index );
            g_cl.invalidate_party_roster( );
            g_crits.on_player_life_change( index );
            g_player_data.on_life_change( index );
            break;
        }

        case HASH_CT( "item_pickup" ):
        {
            const string_view item{ event->GetString( ASTRAL_XORSTR( "item" ).get( ) ) };
            usize             kind{ config::PICKUP_COUNT };

            if ( item.contains( ASTRAL_XORSTR( "medkit" ).get( ) ) || item == ASTRAL_XORSTR( "healthammokit" ).get( ) )
            {
                kind = config::PICKUP_HEALTH;
            } else if ( item.contains( ASTRAL_XORSTR( "ammopack" ).get( ) ) )
            {
                kind = config::PICKUP_AMMO;
            } else
            {
                return;
            }

            g_esp.on_item_pickup( g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "userid" ).get( ) ) ), kind );
            break;
        }

        default: break;
    }
}

int game_events::GetEventDebugID( )
{
    // EVENT_DEBUG_ID_INIT, what the engine expects back from a live listener
    return 42;
}
