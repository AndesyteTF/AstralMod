#include "base.hpp"
#include "steam.hpp"

#include "auto_automation.hpp"
#include "party_network.hpp"

ASTRAL_NO_DESTROY auto_automation g_auto_automation{ };

const char *auto_automation::class_name( i32 player_class ) const
{
    switch ( player_class )
    {
        case TF_CLASS_SCOUT: return ASTRAL_XORSTR_STATIC( "scout" );
        case TF_CLASS_SNIPER: return ASTRAL_XORSTR_STATIC( "sniper" );
        case TF_CLASS_SOLDIER: return ASTRAL_XORSTR_STATIC( "soldier" );
        case TF_CLASS_DEMOMAN: return ASTRAL_XORSTR_STATIC( "demoman" );
        case TF_CLASS_MEDIC: return ASTRAL_XORSTR_STATIC( "medic" );
        case TF_CLASS_HEAVYWEAPONS: return ASTRAL_XORSTR_STATIC( "heavyweapons" );
        case TF_CLASS_PYRO: return ASTRAL_XORSTR_STATIC( "pyro" );
        case TF_CLASS_SPY: return ASTRAL_XORSTR_STATIC( "spy" );
        case TF_CLASS_ENGINEER: return ASTRAL_XORSTR_STATIC( "engineer" );
        default: return nullptr;
    }
}

bool auto_automation::protected_player( u32 account ) const
{
    return account && ( g_steam.is_friend( account ) || g_party_network.has_peer( account ) ||
                        g_cfg.player_tag( account ) == config::PLAYER_TAG_FRIEND );
}

void auto_automation::think( )
{
    if ( !g_cfg.m_misc.m_auto_join || !g_tf2.m_engine->IsInGame( ) )
    {
        return;
    }

    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    if ( now < this->m_next_join )
    {
        return;
    }
    this->m_next_join = now + 1.0;

    if ( !g_cl.m_local || g_cl.m_local->m_iTeamNum( ) < TF_TEAM_RED )
    {
        g_tf2.m_engine->ClientCmd_Unrestricted( ASTRAL_XORSTR( "autoteam" ).get( ) );
        return;
    }

    const i32 desired{ static_cast< i32 >( math::clamp< usize >(
        g_cfg.m_misc.m_auto_join_class, 0, 9 ) ) };
    if ( desired && desired != g_cl.m_local->m_iClass( ) )
    {
        if ( const char *name{ this->class_name( desired ) } )
        {
            char command[ 48 ]{ };
            str::format_to( command, sizeof( command ), ASTRAL_XORSTR( "joinclass {}" ).get( ), name );
            g_tf2.m_engine->ClientCmd_Unrestricted( command );
        }
    }
}

void auto_automation::on_vote_start( bf_read &msg )
{
    if ( !g_cfg.m_misc.m_auto_vote || !g_cl.m_local )
    {
        return;
    }

    const i32 team{ msg.ReadByte( ) };
    const i32 vote_id{ msg.ReadLong( ) };
    const i32 caller{ msg.ReadByte( ) };
    char issue[ 256 ]{ }, parameter[ 256 ]{ };
    msg.ReadString( issue, sizeof( issue ) );
    msg.ReadString( parameter, sizeof( parameter ) );
    msg.ReadOneBit( );
    const i32 target{ msg.ReadByte( ) };

    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    if ( team != g_cl.m_local->m_iTeamNum( ) || caller == local || target == local ||
         !vote_id || !caller || !target )
    {
        return;
    }

    player_info_t caller_info{ }, target_info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( caller, &caller_info ) ||
         !g_tf2.m_engine->GetPlayerInfo( target, &target_info ) )
    {
        return;
    }

    const bool target_protected{ this->protected_player( target_info.friendsID ) };
    const bool caller_protected{ this->protected_player( caller_info.friendsID ) };
    const u32 target_marks{ g_cfg.player_marks( target_info.friendsID ) };
    const bool target_marked{ ( target_marks & ( config::PLAYER_MARK_CHEATER |
                                                 config::PLAYER_MARK_SUSPECTED_CHEATER |
                                                 config::PLAYER_MARK_BLACKLISTED ) ) != 0 };

    i32 option{ };
    if ( g_cfg.m_misc.m_auto_vote_protect_friends && target_protected )
    {
        option = 2;
    } else if ( g_cfg.m_misc.m_auto_vote_protect_friends && caller_protected && !target_protected )
    {
        option = 1;
    } else if ( g_cfg.m_misc.m_auto_vote_marked_players && target_marked )
    {
        option = 1;
    }

    if ( option )
    {
        char command[ 48 ]{ };
        str::format_to( command, sizeof( command ), ASTRAL_XORSTR( "vote {} option{}" ).get( ),
                        vote_id, option );
        g_tf2.m_engine->ClientCmd_Unrestricted( command );
    }
}
