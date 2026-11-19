#pragma once

#include "link.hpp"

enum class movement_owner_t : u8
{
    none,
    fast_accel,
    blockbot,
    route,
    auto_bounce,
    goomba,
    wall_climb,
    pixel_surf,
    texture_bug,
    air_stuck,
    pixel_jump,
};

struct movement_lab_point_t
{
    vec3 m_position{ };
    vec3 m_normal{ };
    u8   m_kind{ };
};

class misc
{
public:
    void        on_pre_prediction( );
    void        on_prediction( );
    void        on_post_prediction( );
    void        on_level_init( const char *map_name );
    void        on_level_shutdown( );
    void        paint( ) const;
    void        auto_fullupdate( );
    void        mvm_automation( );
    void        on_override_view( CViewSetup *setup, const vec3 &eye );
    void        on_enable_fog( bool skybox_pass );
    bool        overlay_removed( IMaterial *material ) const;
    bool        is_footstep( const char *name ) const;
    bool        mutes_footstep( const char *name ) const;
    bool        is_weapon_sound( const StartSoundParams_t &params ) const;
    const char *giant_shoot_sound( const char *sound ) const;
    void        noisemaker_spam( );
    void        noisemaker_notify_event( IGameEvent *event );
    void        fake_duck( );
    bool        is_taunt_exploit_taking_place( );

    bool bhop_held( ) const
    {
        return g_cfg.m_misc.m_bhop && this->m_bhop_jump_held;
    }

private:
    void mvm_station_move( );
    void null_movement( );
    void bhop( );
    void delayed_hop( );
    void edge_jump( );
    void long_jump( );
    void mini_jump( );
    void auto_crouch( );
    void jump_stats( );
    void fan_jump( );
    void fast_stop( );
    void fast_accel( );
    void blockbot( );
    void auto_bounce( );
    void goomba( );
    void wall_climb( );
    void pixel_surf( );
    void texture_bug( );
    void air_stuck( );
    void pixel_jump( );
    void movement_finder( );
    void movement_lab_hotkeys( );
    void route_follow( );
    void auto_strafe( );
    void half_sideways( );
    void double_jump_strafe( );
    void strafe_optimizer( );
    void avoid_strafe_walls( );
    void taunt_spin( );
    void anti_afk( );
    void perform_taunt_cancel_exploit( );
    void fov( CViewSetup *setup );
    void freecam( CViewSetup *setup, const vec3 &eye );
    void freecam_input( );
    void thirdperson( CViewSetup *setup, const vec3 &eye );
    void advance_thirdperson( bool wanted );
    void restart_minigun_effects( C_TFMinigun *minigun );
    bool claim_movement( movement_owner_t owner );
    void load_movement_lab( );
    void save_movement_lab( );

    float  m_strafe_side{ };
    float  m_taunt_spin_yaw{ };
    bool   m_bhop_jump_held{ };
    i32    m_delayed_hop_landing_ticks{ };
    bool   m_edge_grounded{ };
    i32    m_long_jump_duck_ticks{ };
    bool   m_mini_jump_release{ };
    bool   m_mini_jump_holding{ };
    bool   m_wall_climb_jump_release{ };
    i32    m_pixel_surf_hold{ };
    i32    m_pixel_surf_park_ticks{ };
    vec3   m_pixel_surf_normal{ };
    i32    m_texture_bug_hold{ };
    vec3   m_texture_bug_normal{ };
    bool   m_pixel_jump_release{ };
    i32    m_air_stuck_hold{ };
    vec3   m_air_stuck_normal{ };
    movement_lab_point_t m_lab_points[ 64 ]{ };
    usize  m_lab_point_count{ };
    i32    m_lab_scan_cooldown{ };
    vec3   m_checkpoint_origin{ };
    vec3   m_checkpoint_angles{ };
    bool   m_checkpoint_valid{ };
    bool   m_checkpoint_save_held{ };
    bool   m_checkpoint_load_held{ };
    vec3   m_route_points[ 32 ]{ };
    usize  m_route_point_count{ };
    usize  m_route_index{ };
    bool   m_route_add_held{ };
    bool   m_route_delete_held{ };
    bool   m_route_clear_held{ };
    char   m_movement_lab_map[ 64 ]{ };
    bool   m_movement_lab_dirty{ };
    movement_owner_t m_movement_owner{ movement_owner_t::none };
    vec3   m_movement_debug_point{ };
    vec3   m_movement_debug_normal{ };
    bool   m_movement_debug_valid{ };
    bool   m_stats_grounded{ true };
    bool   m_stats_active{ };
    vec3   m_stats_start{ };
    f32    m_stats_start_speed{ };
    f32    m_stats_max_speed{ };
    f32    m_stats_max_height{ };
    i32    m_stats_strafes{ };
    i32    m_stats_last_side{ };
    float  m_third_fraction{ };
    double m_third_time{ };
    vec3   m_freecam_origin{ };
    vec3   m_freecam_angles{ };
    i32    m_freecam_buttons{ };
    bool   m_freecam_active{ };
    f32    m_last_action_time{ };
    f32    m_needs_action_time{ };
    bool   m_should_hit_noisemaker{ false };
    bool   m_is_taunt_exploit_taking_place{ false };
    u64    m_fullupdate_last_frame{ };
    u64    m_fullupdate_cooldown{ };
    bool   m_fullupdate_armed{ };
    u64    m_mvm_ready_time{ };
    u64    m_mvm_respawn_time{ };
    u64    m_mvm_queue_time{ };
    u64    m_mvm_buy_time{ };
    u64    m_mvm_class_time{ };
    u64    m_mvm_rent_time{ };
    i32    m_mvm_rented_definition{ };
    i32    m_mvm_buy_class{ };
    usize  m_mvm_buy_step{ };
    bool   m_mvm_buy_finished{ };
    u64    m_mvm_buy_finished_time{ };
    i32    m_mvm_respec_attempts{ };
    f32    m_mvm_station_best_distance{ FLT_MAX };
    u64    m_mvm_station_progress_time{ };
};

extern misc g_misc;
