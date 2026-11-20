#include "triggerbot.hpp"
#include "entity_cache.hpp"

#include "gui/gui.hpp"
#include "lagcomp.hpp"
#include "melee.hpp"

ASTRAL_NO_DESTROY triggerbot g_triggerbot{ };

void triggerbot::run( )
{
    if ( !g_cfg.m_triggerbot.m_enabled )
    {
        return;
    }

    if ( !g_cl.m_cmd || !g_cl.m_local || !g_cl.m_weapon || !g_cl.m_local->IsAlive( ) )
    {
        return;
    }

    if ( g_cl.m_local->IsTaunting( ) || g_cl.m_local->IsInKart( ) || g_cl.m_local->IsGhost( ) )
    {
        return;
    }

    // with no key bound it runs constantly, like the airblast
    if ( !gui::bind_active( g_cfg.m_triggerbot.m_key ) )
    {
        return;
    }

    // the ragebot or melee already committed a shot on this command
    if ( g_cl.m_will_fire || g_melee.aimed_this_command( ) )
    {
        return;
    }

    // the weapon cannot attack yet
    if ( !g_cl.weapon_attack_allowed( g_tf2.m_globals->curtime ) ||
         g_cl.m_weapon->m_bInReload( ) ||
         g_cl.m_weapon->m_flNextPrimaryAttack( ) > g_tf2.m_globals->curtime )
    {
        return;
    }

    // the crosshair's real direction: engine view angles are untouched by silent aim
    vec3 view{ };
    g_tf2.m_engine->GetViewAngles( view );

    vec3 forward{ };
    math::angle_vectors( view, &forward );

    const vec3 shoot_pos{ g_cl.m_local->Weapon_ShootPosition( ) };
    const vec3 end{ shoot_pos + forward * MAX_TRACE_LENGTH };

    if ( !this->over_player( shoot_pos, end ) && !this->over_building( shoot_pos, end ) )
    {
        return;
    }

    g_cl.m_cmd->buttons |= IN_ATTACK;
}

bool triggerbot::player_allowed( C_TFPlayer *player ) const
{
    const bool *ignores{ g_cfg.m_triggerbot.m_ignores };

    if ( player->IsHiddenByCloak( ) && ignores[ config::IGNORE_CLOAKED ] )
    {
        return false;
    }

    if ( player->IsDisguised( ) && ignores[ config::IGNORE_DISGUISED ] )
    {
        return false;
    }

    if ( player->IsFeigned( ) && ignores[ config::IGNORE_FEIGNED ] )
    {
        return false;
    }

    if ( player->IsUbered( ) && ignores[ config::IGNORE_UBERED ] )
    {
        return false;
    }

    if ( player->IsBonked( ) && ignores[ config::IGNORE_BONKED ] )
    {
        return false;
    }

    if ( player->IsTaunting( ) && ignores[ config::IGNORE_TAUNTING ] )
    {
        return false;
    }

    if ( ignores[ config::IGNORE_VACCINATOR ] &&
         ( player->IsBulletUbered( ) || player->IsBlastUbered( ) || player->IsFireUbered( ) ) )
    {
        return false;
    }

    if ( ignores[ config::IGNORE_FRIENDS ] || ignores[ config::IGNORE_ASTRAL_USERS ] ||
         ignores[ config::IGNORE_PARTY ] )
    {
        player_info_t info{ };
        if ( g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) )
        {
            if ( ignores[ config::IGNORE_FRIENDS ] && g_cl.is_friend( info.friendsID ) )
            {
                return false;
            }

            if ( ignores[ config::IGNORE_ASTRAL_USERS ] && g_cl.is_astral_user( info.friendsID ) )
            {
                return false;
            }

            if ( ignores[ config::IGNORE_PARTY ] && g_cl.is_party_member( info.friendsID ) )
            {
                return false;
            }
        }
    }

    return true;
}

bool triggerbot::hitbox_allowed( i32 hitbox ) const
{
    const bool *boxes{ g_cfg.m_triggerbot.m_hitboxes };

    switch ( hitbox )
    {
        case HITBOX_HEAD: return boxes[ config::HITSCAN_BOX_HEAD ];
        case HITBOX_PELVIS: return boxes[ config::HITSCAN_BOX_PELVIS ];
        case HITBOX_SPINE_0: return boxes[ config::HITSCAN_BOX_STOMACH ];
        case HITBOX_SPINE_1: return boxes[ config::HITSCAN_BOX_LOWER_CHEST ];
        case HITBOX_SPINE_2: return boxes[ config::HITSCAN_BOX_CHEST ];
        case HITBOX_SPINE_3: return boxes[ config::HITSCAN_BOX_UPPER_CHEST ];
        case HITBOX_UPPERARM_L:
        case HITBOX_UPPERARM_R:
        case HITBOX_LOWERARM_L:
        case HITBOX_LOWERARM_R: return boxes[ config::HITSCAN_BOX_ARMS ];
        case HITBOX_HIP_L:
        case HITBOX_HIP_R:
        case HITBOX_KNEE_L:
        case HITBOX_KNEE_R:
        case HITBOX_FOOT_L:
        case HITBOX_FOOT_R: return boxes[ config::HITSCAN_BOX_LEGS ];
        default: return false;
    }
}

bool triggerbot::over_player( const vec3 &from, const vec3 &end ) const
{
    if ( !g_cfg.m_triggerbot.m_targets[ config::TARGET_PLAYERS ] )
    {
        return false;
    }

    for ( const auto *cached : g_entity_cache.players( ) )
    {
        C_TFPlayer *player{ cached->m_entity->As< C_TFPlayer * >( ) };
        if ( !player || player == g_cl.m_local || player->IsDormant( ) || !player->IsAlive( ) ||
             player->IsGhost( ) || player->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) )
        {
            continue;
        }

        if ( !this->player_allowed( player ) )
        {
            continue;
        }

        // the newest lag record carries the bones the server would judge the shot against
        const lag_record_t       *record{ g_lagcomp.record( cached->m_index, 0 ) };
        const studiohdr_t        *header{ record ? record->m_header : nullptr };
        const mstudiohitboxset_t *set{ header ? header->pHitboxSet( player->m_nHitboxSet( ) ) : nullptr };
        if ( !set )
        {
            continue;
        }

        f32 entry{ FLT_MAX };
        for ( i32 hitbox{ }; hitbox < set->numhitboxes; ++hitbox )
        {
            if ( !this->hitbox_allowed( hitbox ) )
            {
                continue;
            }

            const mstudiobbox_t *box{ set->pHitbox( hitbox ) };
            f32                  fraction{ };
            if ( box && box->bone < record->m_bone_count &&
                 math::ray_hits_box( from, end, record->m_bones[ box->bone ], box->bbmin,
                                     box->bbmax, 0.0f, &fraction ) )
            {
                entry = math::smaller( entry, fraction );
            }
        }

        if ( entry == FLT_MAX )
        {
            continue;
        }

        // the world must not block the shot up to the hitbox
        const vec3 point{ from + ( end - from ) * entry };
        if ( g_cl.trace_line( g_cl.m_local, player, from, point, MASK_SOLID | CONTENTS_HITBOX ) )
        {
            return true;
        }
    }

    return false;
}

bool triggerbot::over_building( const vec3 &from, const vec3 &end ) const
{
    for ( const auto *cached : g_entity_cache.buildings( ) )
    {
        C_BaseEntity *building{ cached->m_entity };
        if ( !building || building->IsDormant( ) ||
             building->m_iTeamNum( ) == g_cl.m_local->m_iTeamNum( ) )
        {
            continue;
        }

        bool wanted{ false };
        switch ( cached->m_class_id )
        {
            case ETFClassID::CObjectSentrygun:
                wanted = g_cfg.m_triggerbot.m_targets[ config::TARGET_SENTRYGUNS ];
                break;
            case ETFClassID::CObjectDispenser:
            case ETFClassID::CObjectCartDispenser:
                wanted = g_cfg.m_triggerbot.m_targets[ config::TARGET_DISPENSERS ];
                break;
            case ETFClassID::CObjectTeleporter:
                wanted = g_cfg.m_triggerbot.m_targets[ config::TARGET_TELEPORTERS ];
                break;
            default:
                wanted = false;
                break;
        }

        if ( !wanted )
        {
            continue;
        }

        // buildings have no lag history: an engine ray against their collision decides
        trace_t             trace{ };
        Ray_t               ray{ };
        CTraceFilterHitscan filter{ };
        filter.m_pSkip       = g_cl.m_local;
        filter.m_pSkipTarget = nullptr;

        ray.Init( from, end );
        g_tf2.m_trace->TraceRay( ray, MASK_SHOT, &filter, &trace );

        if ( trace.fraction < 1.0f && trace.m_pEnt == building )
        {
            return true;
        }
    }

    return false;
}
