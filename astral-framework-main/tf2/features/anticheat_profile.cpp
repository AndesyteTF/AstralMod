#include "anticheat_profile.hpp"

#include "../config.hpp"

void anticheat_profile::service( )
{
    const usize profile{ g_cfg.m_misc.m_anticheat_profile };
    if ( profile == config::misc_t::ANTICHEAT_PROFILE_OFF ) return;

    // Shared conservative baseline: do not manufacture packet timing, command
    // bursts, impossible angles, random seeds, or forced critical commands.
    g_cfg.m_anti_hack.m_anti_aim_enabled = false;
    g_cfg.m_anti_hack.m_fakelag_enabled = false;
    g_cfg.m_anti_hack.m_retain_blastjump = false;
    g_cfg.m_exploits.m_fake_latency = false;
    g_cfg.m_exploits.m_extended_backtrack = false;
    g_cfg.m_exploits.m_rapid_fire = false;
    g_cfg.m_exploits.m_warp = false;
    g_cfg.m_exploits.m_speedhack = false;
    g_cfg.m_exploits.m_fake_duck = false;
    g_cfg.m_exploits.m_passive_recharge = false;
    g_cfg.m_exploits.m_seed_prediction = false;
    g_cfg.m_crits.m_enabled = false;
    g_cfg.m_melee.m_always_crit = false;
    g_cfg.m_resolver.m_enabled = false;

    if ( profile == config::misc_t::ANTICHEAT_PROFILE_STAC )
    {
        // Keep manual/smoothed aim usable, but remove silent/locking output and
        // automated attacks that are straightforward to score server-side.
        usize *aim_types[]{
            &g_cfg.m_ragebot.m_aim_type,
            &g_cfg.m_ragebot.m_aim_ring_inner_type,
            &g_cfg.m_ragebot.m_aim_ring_inner_thirdperson_type,
            &g_cfg.m_ragebot.m_aim_ring_inner_firstperson_type,
            &g_cfg.m_ragebot.m_aim_ring_outer_type,
            &g_cfg.m_ragebot.m_aim_ring_outer_thirdperson_type,
            &g_cfg.m_ragebot.m_aim_ring_outer_firstperson_type,
            &g_cfg.m_ragebot.m_spectator_thirdperson_type,
            &g_cfg.m_ragebot.m_spectator_firstperson_type,
        };
        for ( usize *type : aim_types )
        {
            if ( *type == config::ragebot_t::AIM_SILENT ||
                 *type == config::ragebot_t::AIM_LOCKING )
                *type = config::ragebot_t::AIM_SMOOTH;
        }
        g_cfg.m_ragebot.m_auto_shoot = false;
        g_cfg.m_melee.m_aim_type = config::melee_t::AIM_PLAIN;
        g_cfg.m_triggerbot.m_enabled = false;
        g_cfg.m_melee.m_auto_backstab = false;
        g_cfg.m_air_blast.m_redirect = false;
        return;
    }

    // SMAC's common aim/trigger checks vary by server configuration. The
    // conservative profile leaves visual and informational features alone but
    // removes automated view/attack decisions completely.
    g_cfg.m_ragebot.m_enabled = false;
    g_cfg.m_triggerbot.m_enabled = false;
    g_cfg.m_melee.m_enabled = false;
    g_cfg.m_air_blast.m_aim = false;
    g_cfg.m_air_blast.m_redirect = false;
    g_cfg.m_auto_det.m_enabled = false;
}
