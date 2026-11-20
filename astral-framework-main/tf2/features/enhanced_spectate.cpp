#include "base.hpp"

#include "enhanced_spectate.hpp"
#include "entity_cache.hpp"

ASTRAL_NO_DESTROY enhanced_spectate g_enhanced_spectate{ };

void enhanced_spectate::toggle( u32 account )
{
    this->restore( );
    this->m_target_account = this->m_target_account == account ? 0 : account;
    push_notification( notify_kind_t::information, ICON_EYE,
                       ASTRAL_XORSTR_STATIC( "Enhanced spectate" ),
                       this->m_target_account ? ASTRAL_XORSTR_STATIC( "Spectate target selected" )
                                              : ASTRAL_XORSTR_STATIC( "Spectate stopped" ) );
}

C_TFPlayer *enhanced_spectate::find_target( ) const
{
    for ( const auto *cached : g_entity_cache.players( ) )
    {
        player_info_t info{ };
        if ( g_tf2.m_engine->GetPlayerInfo( cached->m_index, &info ) &&
             info.friendsID == this->m_target_account )
        {
            return cached->m_entity->As< C_TFPlayer * >( );
        }
    }
    return nullptr;
}

void enhanced_spectate::apply( )
{
    if ( !this->m_target_account || !g_cl.m_local )
    {
        return;
    }

    C_TFPlayer *target{ this->find_target( ) };
    if ( !target || target == g_cl.m_local )
    {
        this->clear( );
        return;
    }

    this->m_original_target = g_cl.m_local->m_hObserverTarget( );
    this->m_original_mode   = g_cl.m_local->m_iObserverMode( );

    C_BaseEntity *view_target{ target };
    const i32 target_mode{ target->m_iObserverMode( ) };
    if ( ( target_mode == OBS_MODE_IN_EYE || target_mode == OBS_MODE_CHASE ) &&
         target->m_hObserverTarget( ).Get( ) )
    {
        view_target = target->m_hObserverTarget( ).Get( );
    }

    g_cl.m_local->m_hObserverTarget( ).Set( view_target );
    g_cl.m_local->m_iObserverMode( ) = g_cfg.m_misc.m_enhanced_spectate_thirdperson
                                           ? OBS_MODE_CHASE
                                           : OBS_MODE_IN_EYE;
    this->m_applied = true;
}

void enhanced_spectate::restore( )
{
    if ( !this->m_applied )
    {
        return;
    }
    if ( g_cl.m_local )
    {
        g_cl.m_local->m_hObserverTarget( ) = this->m_original_target;
        g_cl.m_local->m_iObserverMode( )   = this->m_original_mode;
    }
    this->m_applied = false;
}

void enhanced_spectate::clear( )
{
    this->restore( );
    this->m_target_account = 0;
}
