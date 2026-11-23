template< typename value_type >
inline constexpr i64 config_enum_maximum{ json::detail::is_same< value_type, config::esp_anchor_t > ? 3 : 2 };

inline constexpr u32 CONFIG_BIND_KEY_MAXIMUM{ 255 };

template< typename visitor >
static void config_fields( color_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "r" ).get( ), value.r );
    field( ASTRAL_XORSTR( "g" ).get( ), value.g );
    field( ASTRAL_XORSTR( "b" ).get( ), value.b );
    field( ASTRAL_XORSTR( "a" ).get( ), value.a );
}

template< typename visitor >
static void config_fields( point_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "x" ).get( ), value.x );
    field( ASTRAL_XORSTR( "y" ).get( ), value.y );
}

template< typename visitor >
static void config_fields( bind_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "mode" ).get( ), value.m_mode );
    field( ASTRAL_XORSTR( "display_name" ).get( ), value.m_display_name );
}

template< typename visitor >
static void config_fields( config::cham_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "visible" ).get( ), value.m_visible );
    field( ASTRAL_XORSTR( "visible_overlay" ).get( ), value.m_visible_overlay );
    field( ASTRAL_XORSTR( "invisible" ).get( ), value.m_invisible );
    field( ASTRAL_XORSTR( "invisible_overlay" ).get( ), value.m_invisible_overlay );
    field( ASTRAL_XORSTR( "visible_color" ).get( ), value.m_visible_color );
    field( ASTRAL_XORSTR( "occluded_color" ).get( ), value.m_occluded_color );
    field( ASTRAL_XORSTR( "visible_overlay_color" ).get( ), value.m_visible_overlay_color );
    field( ASTRAL_XORSTR( "occluded_overlay_color" ).get( ), value.m_occluded_overlay_color );
    field( ASTRAL_XORSTR( "visible_custom" ).get( ), value.m_visible_custom );
    field( ASTRAL_XORSTR( "visible_overlay_custom" ).get( ), value.m_visible_overlay_custom );
    field( ASTRAL_XORSTR( "invisible_custom" ).get( ), value.m_invisible_custom );
    field( ASTRAL_XORSTR( "invisible_overlay_custom" ).get( ), value.m_invisible_overlay_custom );
}

template< typename visitor >
static void config_fields( config::esp_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "elements" ).get( ), value.m_elements );
    field( ASTRAL_XORSTR( "chams" ).get( ), value.m_chams );
    field( ASTRAL_XORSTR( "order" ).get( ), value.m_order );
    field( ASTRAL_XORSTR( "anchors" ).get( ), value.m_anchors );
    field( ASTRAL_XORSTR( "aligns" ).get( ), value.m_aligns );
    field( ASTRAL_XORSTR( "box_color" ).get( ), value.m_box_color );
    field( ASTRAL_XORSTR( "name_color" ).get( ), value.m_name_color );
    field( ASTRAL_XORSTR( "distance_color" ).get( ), value.m_distance_color );
    field( ASTRAL_XORSTR( "health_style" ).get( ), value.m_health_style );
    field( ASTRAL_XORSTR( "health_color" ).get( ), value.m_health_color );
    field( ASTRAL_XORSTR( "health_low_color" ).get( ), value.m_health_low_color );
    field( ASTRAL_XORSTR( "overheal_color" ).get( ), value.m_overheal_color );
    field( ASTRAL_XORSTR( "health_divisions" ).get( ), value.m_health_divisions );
    field( ASTRAL_XORSTR( "weapon_color" ).get( ), value.m_weapon_color );
    field( ASTRAL_XORSTR( "skeleton_color" ).get( ), value.m_skeleton_color );
    field( ASTRAL_XORSTR( "sandevistan_start_color" ).get( ), value.m_sandevistan_start_color );
    field( ASTRAL_XORSTR( "sandevistan_end_color" ).get( ), value.m_sandevistan_end_color );
}

template< typename visitor >
static void config_fields( config::esp_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "profiles" ).get( ), value.m_profiles );
    field( ASTRAL_XORSTR( "target_group" ).get( ), value.m_target_group );
    field( ASTRAL_XORSTR( "edited" ).get( ), value.m_edited );
    field( ASTRAL_XORSTR( "dormant_time" ).get( ), value.m_dormant_time );
    field( ASTRAL_XORSTR( "distance_fade" ).get( ), value.m_distance_fade );
    field( ASTRAL_XORSTR( "distance_fade_start" ).get( ), value.m_distance_fade_start );
    field( ASTRAL_XORSTR( "distance_fade_end" ).get( ), value.m_distance_fade_end );
    field( ASTRAL_XORSTR( "distance_fade_minimum" ).get( ), value.m_distance_fade_minimum );
    field( ASTRAL_XORSTR( "avatars" ).get( ), value.m_avatars );
    field( ASTRAL_XORSTR( "target_style" ).get( ), value.m_target_style );
    field( ASTRAL_XORSTR( "target_chevron" ).get( ), value.m_target_chevron );
    field( ASTRAL_XORSTR( "target_animate" ).get( ), value.m_target_animate );
    field( ASTRAL_XORSTR( "target_color" ).get( ), value.m_target_color );
    field( ASTRAL_XORSTR( "building_elements" ).get( ), value.m_building_elements );
    field( ASTRAL_XORSTR( "projectile_elements" ).get( ), value.m_projectile_elements );
    field( ASTRAL_XORSTR( "pickup_elements" ).get( ), value.m_pickup_elements );
    field( ASTRAL_XORSTR( "intel_elements" ).get( ), value.m_intel_elements );
    field( ASTRAL_XORSTR( "npc_elements" ).get( ), value.m_npc_elements );
    field( ASTRAL_XORSTR( "projectile_teams" ).get( ), value.m_projectile_teams );
    field( ASTRAL_XORSTR( "draw_teammates_conditionally" ).get( ), value.m_draw_teammates_conditionally );
    field( ASTRAL_XORSTR( "players_on_top" ).get( ), value.m_players_on_top );
    field( ASTRAL_XORSTR( "sandevistan_show_all" ).get( ), value.m_sandevistan_show_all );
}

template< typename visitor >
static void config_fields( config::hitmarker_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "show_damage" ).get( ), value.m_show_damage );
    field( ASTRAL_XORSTR( "render_mode" ).get( ), value.m_render_mode );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "edited_event" ).get( ), value.m_edited_event );
    field( ASTRAL_XORSTR( "override_color" ).get( ), value.m_override_color );
    field( ASTRAL_XORSTR( "event_color" ).get( ), value.m_event_color );
}

template< typename visitor >
static void config_fields( config::chams_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "buildings" ).get( ), value.m_buildings );
    field( ASTRAL_XORSTR( "projectiles" ).get( ), value.m_projectiles );
    field( ASTRAL_XORSTR( "pickups" ).get( ), value.m_pickups );
    field( ASTRAL_XORSTR( "intel" ).get( ), value.m_intel );
    field( ASTRAL_XORSTR( "npcs" ).get( ), value.m_npcs );
    field( ASTRAL_XORSTR( "viewmodel" ).get( ), value.m_viewmodel );
    field( ASTRAL_XORSTR( "viewmodel_overlay" ).get( ), value.m_viewmodel_overlay );
    field( ASTRAL_XORSTR( "weapon" ).get( ), value.m_weapon );
    field( ASTRAL_XORSTR( "weapon_overlay" ).get( ), value.m_weapon_overlay );
    field( ASTRAL_XORSTR( "viewmodel_color" ).get( ), value.m_viewmodel_color );
    field( ASTRAL_XORSTR( "viewmodel_overlay_color" ).get( ), value.m_viewmodel_overlay_color );
    field( ASTRAL_XORSTR( "weapon_color" ).get( ), value.m_weapon_color );
    field( ASTRAL_XORSTR( "weapon_overlay_color" ).get( ), value.m_weapon_overlay_color );
    field( ASTRAL_XORSTR( "viewmodel_custom" ).get( ), value.m_viewmodel_custom );
    field( ASTRAL_XORSTR( "viewmodel_overlay_custom" ).get( ), value.m_viewmodel_overlay_custom );
    field( ASTRAL_XORSTR( "weapon_custom" ).get( ), value.m_weapon_custom );
    field( ASTRAL_XORSTR( "weapon_overlay_custom" ).get( ), value.m_weapon_overlay_custom );
    field( ASTRAL_XORSTR( "local_model" ).get( ), value.m_local_model );
    field( ASTRAL_XORSTR( "enemy_model" ).get( ), value.m_enemy_model );
    field( ASTRAL_XORSTR( "backtrack" ).get( ), value.m_backtrack );
    field( ASTRAL_XORSTR( "backtrack_records" ).get( ), value.m_backtrack_records );
    field( ASTRAL_XORSTR( "backtrack_color" ).get( ), value.m_backtrack_color );
    field( ASTRAL_XORSTR( "backtrack_overlay_color" ).get( ), value.m_backtrack_overlay_color );
    field( ASTRAL_XORSTR( "custom_material" ).get( ), value.m_custom_material );
}

template< typename visitor >
static void config_fields( config::glow_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "stencil" ).get( ), value.m_stencil );
    field( ASTRAL_XORSTR( "blur" ).get( ), value.m_blur );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
}

template< typename visitor >
static void config_fields( config::glow_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "players" ).get( ), value.m_players );
    field( ASTRAL_XORSTR( "buildings" ).get( ), value.m_buildings );
    field( ASTRAL_XORSTR( "projectiles" ).get( ), value.m_projectiles );
    field( ASTRAL_XORSTR( "pickups" ).get( ), value.m_pickups );
    field( ASTRAL_XORSTR( "intel" ).get( ), value.m_intel );
    field( ASTRAL_XORSTR( "npcs" ).get( ), value.m_npcs );
    field( ASTRAL_XORSTR( "backtrack" ).get( ), value.m_backtrack );
    field( ASTRAL_XORSTR( "viewmodel" ).get( ), value.m_viewmodel );
    field( ASTRAL_XORSTR( "weapon" ).get( ), value.m_weapon );
}

template< typename visitor >
static void config_fields( config::weather_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "type" ).get( ), value.m_type );
    field( ASTRAL_XORSTR( "density" ).get( ), value.m_density );
    field( ASTRAL_XORSTR( "wind_speed" ).get( ), value.m_wind_speed );
    field( ASTRAL_XORSTR( "wind_direction" ).get( ), value.m_wind_direction );
}

template< typename visitor >
static void config_fields( config::tracers_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "style" ).get( ), value.m_style );
    field( ASTRAL_XORSTR( "local_only" ).get( ), value.m_local_only );
    field( ASTRAL_XORSTR( "local_color" ).get( ), value.m_local_color );
    field( ASTRAL_XORSTR( "local_impact_color" ).get( ), value.m_local_impact_color );
    field( ASTRAL_XORSTR( "entity_color" ).get( ), value.m_entity_color );
    field( ASTRAL_XORSTR( "entity_impact_color" ).get( ), value.m_entity_impact_color );
    field( ASTRAL_XORSTR( "duration" ).get( ), value.m_duration );
    field( ASTRAL_XORSTR( "local_duration" ).get( ), value.m_local_duration );
    field( ASTRAL_XORSTR( "friend_duration" ).get( ), value.m_friend_duration );
    field( ASTRAL_XORSTR( "teammate_duration" ).get( ), value.m_teammate_duration );
    field( ASTRAL_XORSTR( "enemy_duration" ).get( ), value.m_enemy_duration );
    field( ASTRAL_XORSTR( "impact_marker" ).get( ), value.m_impact_marker );
    field( ASTRAL_XORSTR( "impact_style" ).get( ), value.m_impact_style );
}

template< typename visitor >
static void config_fields( config::arrows_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "mode" ).get( ), value.m_mode );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "spy_color" ).get( ), value.m_spy_color );
    field( ASTRAL_XORSTR( "radius" ).get( ), value.m_radius );
    field( ASTRAL_XORSTR( "size" ).get( ), value.m_size );
    field( ASTRAL_XORSTR( "distance" ).get( ), value.m_distance );
}

template< typename visitor >
static void config_fields( config::viewmodel_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "forward" ).get( ), value.m_forward );
    field( ASTRAL_XORSTR( "right" ).get( ), value.m_right );
    field( ASTRAL_XORSTR( "up" ).get( ), value.m_up );
    field( ASTRAL_XORSTR( "bob" ).get( ), value.m_bob );
    field( ASTRAL_XORSTR( "sway" ).get( ), value.m_sway );
    field( ASTRAL_XORSTR( "aim" ).get( ), value.m_aim );
    field( ASTRAL_XORSTR( "aim_swing" ).get( ), value.m_aim_swing );
}

template< typename visitor >
static void config_fields( config::skybox_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "index" ).get( ), value.m_index );
}

template< typename visitor >
static void config_fields( config::world_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "modulate" ).get( ), value.m_modulate );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "skybox" ).get( ), value.m_skybox );
    field( ASTRAL_XORSTR( "props" ).get( ), value.m_props );
}

template< typename visitor >
static void config_fields( config::fog_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "mode" ).get( ), value.m_mode );
    field( ASTRAL_XORSTR( "start" ).get( ), value.m_start );
    field( ASTRAL_XORSTR( "end" ).get( ), value.m_end );
    field( ASTRAL_XORSTR( "density" ).get( ), value.m_density );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "skybox" ).get( ), value.m_skybox );
}

template< typename visitor >
static void config_fields( config::post_process_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "exposure" ).get( ), value.m_exposure );
    field( ASTRAL_XORSTR( "contrast" ).get( ), value.m_contrast );
    field( ASTRAL_XORSTR( "saturation" ).get( ), value.m_saturation );
    field( ASTRAL_XORSTR( "vibrance" ).get( ), value.m_vibrance );
    field( ASTRAL_XORSTR( "shadows" ).get( ), value.m_shadows );
    field( ASTRAL_XORSTR( "highlights" ).get( ), value.m_highlights );
    field( ASTRAL_XORSTR( "temperature" ).get( ), value.m_temperature );
    field( ASTRAL_XORSTR( "tint" ).get( ), value.m_tint );
    field( ASTRAL_XORSTR( "vignette" ).get( ), value.m_vignette );
    field( ASTRAL_XORSTR( "bloom" ).get( ), value.m_bloom );
    field( ASTRAL_XORSTR( "bloom_threshold" ).get( ), value.m_bloom_threshold );
    field( ASTRAL_XORSTR( "motion_blur" ).get( ), value.m_motion_blur );
}

template< typename visitor >
static void config_fields( config::radar_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "position" ).get( ), value.m_position );
    field( ASTRAL_XORSTR( "size" ).get( ), value.m_size );
    field( ASTRAL_XORSTR( "range" ).get( ), value.m_range );
    field( ASTRAL_XORSTR( "icon_size" ).get( ), value.m_icon_size );
    field( ASTRAL_XORSTR( "rotate" ).get( ), value.m_rotate );
    field( ASTRAL_XORSTR( "custom_icons" ).get( ), value.m_custom_icons );
    field( ASTRAL_XORSTR( "icon_style" ).get( ), value.m_icon_style );
    field( ASTRAL_XORSTR( "show_local" ).get( ), value.m_show_local );
    field( ASTRAL_XORSTR( "show_enemies" ).get( ), value.m_show_enemies );
    field( ASTRAL_XORSTR( "show_teammates" ).get( ), value.m_show_teammates );
    field( ASTRAL_XORSTR( "show_dormant" ).get( ), value.m_show_dormant );
    field( ASTRAL_XORSTR( "show_names" ).get( ), value.m_show_names );
    field( ASTRAL_XORSTR( "show_buildings" ).get( ), value.m_show_buildings );
    field( ASTRAL_XORSTR( "show_projectiles" ).get( ), value.m_show_projectiles );
    field( ASTRAL_XORSTR( "show_pickups" ).get( ), value.m_show_pickups );
    field( ASTRAL_XORSTR( "show_objectives" ).get( ), value.m_show_objectives );
    field( ASTRAL_XORSTR( "show_npcs" ).get( ), value.m_show_npcs );
    field( ASTRAL_XORSTR( "map_mode" ).get( ), value.m_map_mode );
    field( ASTRAL_XORSTR( "map_detail" ).get( ), value.m_map_detail );
    field( ASTRAL_XORSTR( "map_color" ).get( ), value.m_map_color );
    field( ASTRAL_XORSTR( "enemy_color" ).get( ), value.m_enemy_color );
    field( ASTRAL_XORSTR( "teammate_color" ).get( ), value.m_teammate_color );
    field( ASTRAL_XORSTR( "local_color" ).get( ), value.m_local_color );
    field( ASTRAL_XORSTR( "building_color" ).get( ), value.m_building_color );
    field( ASTRAL_XORSTR( "projectile_color" ).get( ), value.m_projectile_color );
    field( ASTRAL_XORSTR( "pickup_color" ).get( ), value.m_pickup_color );
    field( ASTRAL_XORSTR( "objective_color" ).get( ), value.m_objective_color );
    field( ASTRAL_XORSTR( "npc_color" ).get( ), value.m_npc_color );
}

template< typename visitor >
static void config_fields( config::misc_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "removals" ).get( ), value.m_removals );
    field( ASTRAL_XORSTR( "accent" ).get( ), value.m_accent );
    field( ASTRAL_XORSTR( "accent_end" ).get( ), value.m_accent_end );
    field( ASTRAL_XORSTR( "shadow" ).get( ), value.m_shadow );
    field( ASTRAL_XORSTR( "ui_background" ).get( ), value.m_ui_background );
    field( ASTRAL_XORSTR( "ui_surface" ).get( ), value.m_ui_surface );
    field( ASTRAL_XORSTR( "ui_raised" ).get( ), value.m_ui_raised );
    field( ASTRAL_XORSTR( "ui_border" ).get( ), value.m_ui_border );
    field( ASTRAL_XORSTR( "ui_text" ).get( ), value.m_ui_text );
    field( ASTRAL_XORSTR( "ui_muted" ).get( ), value.m_ui_muted );
    field( ASTRAL_XORSTR( "ui_dim" ).get( ), value.m_ui_dim );
    field( ASTRAL_XORSTR( "ui_hover" ).get( ), value.m_ui_hover );
    field( ASTRAL_XORSTR( "ui_selection" ).get( ), value.m_ui_selection );
    field( ASTRAL_XORSTR( "ui_warning" ).get( ), value.m_ui_warning );
    field( ASTRAL_XORSTR( "ui_error" ).get( ), value.m_ui_error );
    field( ASTRAL_XORSTR( "menu_blur" ).get( ), value.m_menu_blur );
    field( ASTRAL_XORSTR( "menu_astral_effect" ).get( ), value.m_menu_astral_effect );
    field( ASTRAL_XORSTR( "menu_style" ).get( ), value.m_menu_style );
    field( ASTRAL_XORSTR( "ui_scale" ).get( ), value.m_ui_scale );
    field( ASTRAL_XORSTR( "ui_font" ).get( ), value.m_ui_font );
    field( ASTRAL_XORSTR( "snap_indicators" ).get( ), value.m_snap_indicators );
    field( ASTRAL_XORSTR( "dock_editor" ).get( ), value.m_dock_editor );
    field( ASTRAL_XORSTR( "crit_indicator_pos" ).get( ), value.m_crit_indicator_position );
    field( ASTRAL_XORSTR( "tick_indicator_pos" ).get( ), value.m_tick_indicator_position );
    field( ASTRAL_XORSTR( "menu_particles" ).get( ), value.m_menu_particles );
    field( ASTRAL_XORSTR( "menu_sounds" ).get( ), value.m_menu_sounds );
    field( ASTRAL_XORSTR( "menu_sound_volume" ).get( ), value.m_menu_sound_volume );
    field( ASTRAL_XORSTR( "notifications" ).get( ), value.m_notifications );
    field( ASTRAL_XORSTR( "notification_log" ).get( ), value.m_notification_log );
    field( ASTRAL_XORSTR( "notification_kinds" ).get( ), value.m_notification_kinds );
    field( ASTRAL_XORSTR( "notification_channels" ).get( ), value.m_notification_channels );
    field( ASTRAL_XORSTR( "notification_lifetime" ).get( ), value.m_notification_lifetime );
    field( ASTRAL_XORSTR( "notification_position" ).get( ), value.m_notification_position );
    field( ASTRAL_XORSTR( "draw_watermark" ).get( ), value.m_draw_watermark );
    field( ASTRAL_XORSTR( "watermark_mode" ).get( ), value.m_watermark_mode );
    field( ASTRAL_XORSTR( "watermark_show_user" ).get( ), value.m_watermark_show_user );
    field( ASTRAL_XORSTR( "watermark_show_fps" ).get( ), value.m_watermark_show_fps );
    field( ASTRAL_XORSTR( "watermark_show_ping" ).get( ), value.m_watermark_show_ping );
    field( ASTRAL_XORSTR( "watermark_show_clock" ).get( ), value.m_watermark_show_clock );
    field( ASTRAL_XORSTR( "watermark_show_class" ).get( ), value.m_watermark_show_class );
    field( ASTRAL_XORSTR( "watermark_show_kd" ).get( ), value.m_watermark_show_kd );
    field( ASTRAL_XORSTR( "spectator_list" ).get( ), value.m_spectator_list );
    field( ASTRAL_XORSTR( "spectator_list_position" ).get( ), value.m_spectator_list_position );
    field( ASTRAL_XORSTR( "binds_list" ).get( ), value.m_binds_list );
    field( ASTRAL_XORSTR( "binds_list_show_always" ).get( ), value.m_binds_list_show_always );
    field( ASTRAL_XORSTR( "binds_list_name_mode" ).get( ), value.m_binds_list_name_mode );
    field( ASTRAL_XORSTR( "binds_list_position" ).get( ), value.m_binds_list_position );
    field( ASTRAL_XORSTR( "astral_sense" ).get( ), value.m_astral_sense );
    field( ASTRAL_XORSTR( "astral_sense_player_card" ).get( ), value.m_astral_sense_player_card );
    field( ASTRAL_XORSTR( "astral_sense_player_card_custom_image" ).get( ), value.m_astral_sense_player_card_custom_image );
    field( ASTRAL_XORSTR( "astral_sense_player_card_image" ).get( ), value.m_astral_sense_player_card_image );
    field( ASTRAL_XORSTR( "astral_sense_status" ).get( ), value.m_astral_sense_status );
    field( ASTRAL_XORSTR( "astral_sense_status_fields" ).get( ), value.m_astral_sense_status_fields );
    field( ASTRAL_XORSTR( "astral_sense_damage" ).get( ), value.m_astral_sense_damage );
    field( ASTRAL_XORSTR( "astral_sense_speed" ).get( ), value.m_astral_sense_speed );
    field( ASTRAL_XORSTR( "astral_sense_health" ).get( ), value.m_astral_sense_health );
    field( ASTRAL_XORSTR( "astral_sense_session" ).get( ), value.m_astral_sense_session );
    field( ASTRAL_XORSTR( "astral_sense_trail" ).get( ), value.m_astral_sense_trail );
    field( ASTRAL_XORSTR( "astral_sense_diamond" ).get( ), value.m_astral_sense_diamond );
    field( ASTRAL_XORSTR( "astral_sense_resources" ).get( ), value.m_astral_sense_resources );
    field( ASTRAL_XORSTR( "astral_sense_buildings" ).get( ), value.m_astral_sense_buildings );
    field( ASTRAL_XORSTR( "astral_sense_combat" ).get( ), value.m_astral_sense_combat );
    field( ASTRAL_XORSTR( "astral_sense_conditions" ).get( ), value.m_astral_sense_conditions );
    field( ASTRAL_XORSTR( "astral_sense_compass" ).get( ), value.m_astral_sense_compass );
    field( ASTRAL_XORSTR( "astral_sense_objective" ).get( ), value.m_astral_sense_objective );
    field( ASTRAL_XORSTR( "astral_sense_events" ).get( ), value.m_astral_sense_events );
    field( ASTRAL_XORSTR( "astral_sense_alerts" ).get( ), value.m_astral_sense_alerts );
    field( ASTRAL_XORSTR( "astral_sense_arsenal" ).get( ), value.m_astral_sense_arsenal );
    field( ASTRAL_XORSTR( "astral_sense_telemetry" ).get( ), value.m_astral_sense_telemetry );
    field( ASTRAL_XORSTR( "astral_sense_readiness" ).get( ), value.m_astral_sense_readiness );
    field( ASTRAL_XORSTR( "astral_sense_history" ).get( ), value.m_astral_sense_history );
    field( ASTRAL_XORSTR( "astral_sense_performance_graphs" ).get( ), value.m_astral_sense_performance_graphs );
    field( ASTRAL_XORSTR( "astral_sense_screen_alerts" ).get( ), value.m_astral_sense_screen_alerts );
    field( ASTRAL_XORSTR( "astral_sense_status_position" ).get( ), value.m_astral_sense_status_position );
    field( ASTRAL_XORSTR( "astral_sense_player_card_position" ).get( ), value.m_astral_sense_player_card_position );
    field( ASTRAL_XORSTR( "astral_sense_damage_position" ).get( ), value.m_astral_sense_damage_position );
    field( ASTRAL_XORSTR( "astral_sense_speed_position" ).get( ), value.m_astral_sense_speed_position );
    field( ASTRAL_XORSTR( "astral_sense_health_position" ).get( ), value.m_astral_sense_health_position );
    field( ASTRAL_XORSTR( "astral_sense_session_position" ).get( ), value.m_astral_sense_session_position );
    field( ASTRAL_XORSTR( "astral_sense_resources_position" ).get( ), value.m_astral_sense_resources_position );
    field( ASTRAL_XORSTR( "astral_sense_buildings_position" ).get( ), value.m_astral_sense_buildings_position );
    field( ASTRAL_XORSTR( "astral_sense_combat_position" ).get( ), value.m_astral_sense_combat_position );
    field( ASTRAL_XORSTR( "astral_sense_conditions_position" ).get( ), value.m_astral_sense_conditions_position );
    field( ASTRAL_XORSTR( "astral_sense_compass_position" ).get( ), value.m_astral_sense_compass_position );
    field( ASTRAL_XORSTR( "astral_sense_objective_position" ).get( ), value.m_astral_sense_objective_position );
    field( ASTRAL_XORSTR( "astral_sense_events_position" ).get( ), value.m_astral_sense_events_position );
    field( ASTRAL_XORSTR( "astral_sense_alerts_position" ).get( ), value.m_astral_sense_alerts_position );
    field( ASTRAL_XORSTR( "astral_sense_arsenal_position" ).get( ), value.m_astral_sense_arsenal_position );
    field( ASTRAL_XORSTR( "astral_sense_telemetry_position" ).get( ), value.m_astral_sense_telemetry_position );
    field( ASTRAL_XORSTR( "astral_sense_readiness_position" ).get( ), value.m_astral_sense_readiness_position );
    field( ASTRAL_XORSTR( "astral_sense_history_position" ).get( ), value.m_astral_sense_history_position );
    field( ASTRAL_XORSTR( "astral_sense_performance_position" ).get( ), value.m_astral_sense_performance_position );
    field( ASTRAL_XORSTR( "seed_indicator_pos" ).get( ), value.m_seed_indicator_position );
    field( ASTRAL_XORSTR( "party_network" ).get( ), value.m_party_network );
    field( ASTRAL_XORSTR( "watchdog" ).get( ), value.m_watchdog );
    field( ASTRAL_XORSTR( "watchdog_quality" ).get( ), value.m_watchdog_quality );
    field( ASTRAL_XORSTR( "watchdog_post_to_party" ).get( ), value.m_watchdog_notify_party );
    field( ASTRAL_XORSTR( "behavior_scanner" ).get( ), value.m_behavior_scanner );
    field( ASTRAL_XORSTR( "crash_reports" ).get( ), value.m_crash_reports );
    field( ASTRAL_XORSTR( "anticheat_profile" ).get( ), value.m_anticheat_profile );
    field( ASTRAL_XORSTR( "discord_rpc" ).get( ), value.m_discord_rpc );
    field( ASTRAL_XORSTR( "discord_rpc_map" ).get( ), value.m_discord_rpc_map );
    field( ASTRAL_XORSTR( "discord_rpc_class" ).get( ), value.m_discord_rpc_class );
    field( ASTRAL_XORSTR( "menu_mascot" ).get( ), value.m_menu_mascot );
    field( ASTRAL_XORSTR( "menu_mascot_file" ).get( ), value.m_menu_mascot_file );
    field( ASTRAL_XORSTR( "menu_mascot_side" ).get( ), value.m_menu_mascot_side );
    field( ASTRAL_XORSTR( "menu_mascot_size" ).get( ), value.m_menu_mascot_size );
    field( ASTRAL_XORSTR( "menu_mascot_offset" ).get( ), value.m_menu_mascot_offset );
    field( ASTRAL_XORSTR( "menu_mascot_spacing" ).get( ), value.m_menu_mascot_spacing );
    field( ASTRAL_XORSTR( "unload_key" ).get( ), value.m_unload_key );
    field( ASTRAL_XORSTR( "watermark_pos" ).get( ), value.m_watermark_position );
    field( ASTRAL_XORSTR( "bhop" ).get( ), value.m_bhop );
    field( ASTRAL_XORSTR( "null_movement" ).get( ), value.m_null_movement );
    field( ASTRAL_XORSTR( "fast_stop" ).get( ), value.m_fast_stop );
    field( ASTRAL_XORSTR( "fast_accel" ).get( ), value.m_fast_accel );
    field( ASTRAL_XORSTR( "air_stop" ).get( ), value.m_air_stop );
    field( ASTRAL_XORSTR( "auto_retry" ).get( ), value.m_auto_retry );
    field( ASTRAL_XORSTR( "auto_join" ).get( ), value.m_auto_join );
    field( ASTRAL_XORSTR( "auto_join_class" ).get( ), value.m_auto_join_class );
    field( ASTRAL_XORSTR( "auto_vote" ).get( ), value.m_auto_vote );
    field( ASTRAL_XORSTR( "auto_vote_protect_friends" ).get( ), value.m_auto_vote_protect_friends );
    field( ASTRAL_XORSTR( "auto_vote_marked_players" ).get( ), value.m_auto_vote_marked_players );
    field( ASTRAL_XORSTR( "enhanced_spectate_thirdperson" ).get( ), value.m_enhanced_spectate_thirdperson );
    field( ASTRAL_XORSTR( "strafe_optimizer" ).get( ), value.m_strafe_optimizer );
    field( ASTRAL_XORSTR( "strafe_optimizer_gain" ).get( ), value.m_strafe_optimizer_gain );
    field( ASTRAL_XORSTR( "strafe_optimizer_min_speed" ).get( ), value.m_strafe_optimizer_min_speed );
    field( ASTRAL_XORSTR( "strafe_avoid_walls" ).get( ), value.m_strafe_avoid_walls );
    field( ASTRAL_XORSTR( "strafe_avoid_distance" ).get( ), value.m_strafe_avoid_distance );
    field( ASTRAL_XORSTR( "half_sideways" ).get( ), value.m_half_sideways );
    field( ASTRAL_XORSTR( "half_sideways_key" ).get( ), value.m_half_sideways_key );
    field( ASTRAL_XORSTR( "double_jump_strafe" ).get( ), value.m_double_jump_strafe );
    field( ASTRAL_XORSTR( "double_jump_strafe_key" ).get( ), value.m_double_jump_strafe_key );
    field( ASTRAL_XORSTR( "delayed_hop" ).get( ), value.m_delayed_hop );
    field( ASTRAL_XORSTR( "delayed_hop_key" ).get( ), value.m_delayed_hop_key );
    field( ASTRAL_XORSTR( "delayed_hop_ticks" ).get( ), value.m_delayed_hop_ticks );
    field( ASTRAL_XORSTR( "long_jump" ).get( ), value.m_long_jump );
    field( ASTRAL_XORSTR( "long_jump_key" ).get( ), value.m_long_jump_key );
    field( ASTRAL_XORSTR( "long_jump_adaptive" ).get( ), value.m_long_jump_adaptive );
    field( ASTRAL_XORSTR( "long_jump_release_ticks" ).get( ), value.m_long_jump_release_ticks );
    field( ASTRAL_XORSTR( "mini_jump" ).get( ), value.m_mini_jump );
    field( ASTRAL_XORSTR( "mini_jump_key" ).get( ), value.m_mini_jump_key );
    field( ASTRAL_XORSTR( "mini_jump_hold_duck" ).get( ), value.m_mini_jump_hold_duck );
    field( ASTRAL_XORSTR( "mini_jump_edge" ).get( ), value.m_mini_jump_edge );
    field( ASTRAL_XORSTR( "auto_crouch" ).get( ), value.m_auto_crouch );
    field( ASTRAL_XORSTR( "auto_crouch_key" ).get( ), value.m_auto_crouch_key );
    field( ASTRAL_XORSTR( "auto_crouch_ticks" ).get( ), value.m_auto_crouch_ticks );
    field( ASTRAL_XORSTR( "jump_stats" ).get( ), value.m_jump_stats );
    field( ASTRAL_XORSTR( "edge_jump" ).get( ), value.m_edge_jump );
    field( ASTRAL_XORSTR( "edge_jump_key" ).get( ), value.m_edge_jump_key );
    field( ASTRAL_XORSTR( "jump_bug" ).get( ), value.m_jump_bug );
    field( ASTRAL_XORSTR( "jump_bug_key" ).get( ), value.m_jump_bug_key );
    field( ASTRAL_XORSTR( "jump_bug_indicator" ).get( ), value.m_jump_bug_indicator );
    field( ASTRAL_XORSTR( "edge_bug" ).get( ), value.m_edge_bug );
    field( ASTRAL_XORSTR( "edge_bug_key" ).get( ), value.m_edge_bug_key );
    field( ASTRAL_XORSTR( "edge_bug_predict_ticks" ).get( ), value.m_edge_bug_predict_ticks );
    field( ASTRAL_XORSTR( "edge_bug_stance" ).get( ), value.m_edge_bug_stance );
    field( ASTRAL_XORSTR( "edge_bug_steer" ).get( ), value.m_edge_bug_steer );
    field( ASTRAL_XORSTR( "edge_bug_indicator" ).get( ), value.m_edge_bug_indicator );
    field( ASTRAL_XORSTR( "edge_skip" ).get( ), value.m_edge_skip );
    field( ASTRAL_XORSTR( "edge_skip_key" ).get( ), value.m_edge_skip_key );
    field( ASTRAL_XORSTR( "blockbot" ).get( ), value.m_blockbot );
    field( ASTRAL_XORSTR( "blockbot_key" ).get( ), value.m_blockbot_key );
    field( ASTRAL_XORSTR( "blockbot_teammates" ).get( ), value.m_blockbot_teammates );
    field( ASTRAL_XORSTR( "auto_bounce" ).get( ), value.m_auto_bounce );
    field( ASTRAL_XORSTR( "auto_bounce_key" ).get( ), value.m_auto_bounce_key );
    field( ASTRAL_XORSTR( "auto_bounce_mode" ).get( ), value.m_auto_bounce_mode );
    field( ASTRAL_XORSTR( "auto_bounce_reach" ).get( ), value.m_auto_bounce_reach );
    field( ASTRAL_XORSTR( "auto_bounce_min_gain" ).get( ), value.m_auto_bounce_min_gain );
    field( ASTRAL_XORSTR( "auto_bounce_jump" ).get( ), value.m_auto_bounce_jump );
    field( ASTRAL_XORSTR( "auto_bounce_crouch" ).get( ), value.m_auto_bounce_crouch );
    field( ASTRAL_XORSTR( "goomba" ).get( ), value.m_goomba );
    field( ASTRAL_XORSTR( "goomba_key" ).get( ), value.m_goomba_key );
    field( ASTRAL_XORSTR( "goomba_teammates" ).get( ), value.m_goomba_teammates );
    field( ASTRAL_XORSTR( "goomba_reach" ).get( ), value.m_goomba_reach );
    field( ASTRAL_XORSTR( "goomba_lead_ticks" ).get( ), value.m_goomba_lead_ticks );
    field( ASTRAL_XORSTR( "wall_climb" ).get( ), value.m_wall_climb );
    field( ASTRAL_XORSTR( "wall_climb_key" ).get( ), value.m_wall_climb_key );
    field( ASTRAL_XORSTR( "wall_climb_reach" ).get( ), value.m_wall_climb_reach );
    field( ASTRAL_XORSTR( "wall_climb_duck" ).get( ), value.m_wall_climb_duck );
    field( ASTRAL_XORSTR( "wall_climb_prevent_slow" ).get( ), value.m_wall_climb_prevent_slow );
    field( ASTRAL_XORSTR( "wall_climb_seam_steer" ).get( ), value.m_wall_climb_seam_steer );
    field( ASTRAL_XORSTR( "wall_climb_steer_scale" ).get( ), value.m_wall_climb_steer_scale );
    field( ASTRAL_XORSTR( "pixel_surf" ).get( ), value.m_pixel_surf );
    field( ASTRAL_XORSTR( "pixel_surf_key" ).get( ), value.m_pixel_surf_key );
    field( ASTRAL_XORSTR( "pixel_surf_predict_ticks" ).get( ), value.m_pixel_surf_predict_ticks );
    field( ASTRAL_XORSTR( "pixel_surf_wall_strafe" ).get( ), value.m_pixel_surf_wall_strafe );
    field( ASTRAL_XORSTR( "pixel_surf_park" ).get( ), value.m_pixel_surf_park );
    field( ASTRAL_XORSTR( "texture_bug" ).get( ), value.m_texture_bug );
    field( ASTRAL_XORSTR( "texture_bug_key" ).get( ), value.m_texture_bug_key );
    field( ASTRAL_XORSTR( "texture_bug_reach" ).get( ), value.m_texture_bug_reach );
    field( ASTRAL_XORSTR( "texture_bug_stance" ).get( ), value.m_texture_bug_stance );
    field( ASTRAL_XORSTR( "texture_bug_auto_duck" ).get( ), value.m_texture_bug_auto_duck );
    field( ASTRAL_XORSTR( "texture_bug_wall_strafe" ).get( ), value.m_texture_bug_wall_strafe );
    field( ASTRAL_XORSTR( "pixel_jump" ).get( ), value.m_pixel_jump );
    field( ASTRAL_XORSTR( "pixel_jump_key" ).get( ), value.m_pixel_jump_key );
    field( ASTRAL_XORSTR( "pixel_jump_reach" ).get( ), value.m_pixel_jump_reach );
    field( ASTRAL_XORSTR( "pixel_jump_crouch" ).get( ), value.m_pixel_jump_crouch );
    field( ASTRAL_XORSTR( "pixel_jump_hop" ).get( ), value.m_pixel_jump_hop );
    field( ASTRAL_XORSTR( "movement_debug" ).get( ), value.m_movement_debug );
    field( ASTRAL_XORSTR( "air_stuck" ).get( ), value.m_air_stuck );
    field( ASTRAL_XORSTR( "air_stuck_key" ).get( ), value.m_air_stuck_key );
    field( ASTRAL_XORSTR( "air_stuck_reach" ).get( ), value.m_air_stuck_reach );
    field( ASTRAL_XORSTR( "air_stuck_crouch" ).get( ), value.m_air_stuck_crouch );
    field( ASTRAL_XORSTR( "movement_finder" ).get( ), value.m_movement_finder );
    field( ASTRAL_XORSTR( "movement_finder_radius" ).get( ), value.m_movement_finder_radius );
    field( ASTRAL_XORSTR( "movement_finder_draw_distance" ).get( ), value.m_movement_finder_draw_distance );
    field( ASTRAL_XORSTR( "checkpoint_save_key" ).get( ), value.m_checkpoint_save_key );
    field( ASTRAL_XORSTR( "checkpoint_load_key" ).get( ), value.m_checkpoint_load_key );
    field( ASTRAL_XORSTR( "route_add_key" ).get( ), value.m_route_add_key );
    field( ASTRAL_XORSTR( "route_delete_key" ).get( ), value.m_route_delete_key );
    field( ASTRAL_XORSTR( "route_clear_key" ).get( ), value.m_route_clear_key );
    field( ASTRAL_XORSTR( "route_follow" ).get( ), value.m_route_follow );
    field( ASTRAL_XORSTR( "route_follow_key" ).get( ), value.m_route_follow_key );
    field( ASTRAL_XORSTR( "route_reach" ).get( ), value.m_route_reach );
    field( ASTRAL_XORSTR( "fan_jump" ).get( ), value.m_fan_jump );
    field( ASTRAL_XORSTR( "fan_jump_key" ).get( ), value.m_fan_jump_key );
    field( ASTRAL_XORSTR( "taunt_spin" ).get( ), value.m_taunt_spin );
    field( ASTRAL_XORSTR( "taunt_spin_speed" ).get( ), value.m_taunt_spin_speed );
    field( ASTRAL_XORSTR( "giant_weapon_sounds" ).get( ), value.m_giant_weapon_sounds );
    field( ASTRAL_XORSTR( "apply_to_alternative" ).get( ), value.m_apply_to_alternate_weapons );
    field( ASTRAL_XORSTR( "weapon_pitch" ).get( ), value.m_weapon_pitch );
    field( ASTRAL_XORSTR( "auto_strafe" ).get( ), value.m_auto_strafe );
    field( ASTRAL_XORSTR( "auto_strafe_mode" ).get( ), value.m_auto_strafe_mode );
    field( ASTRAL_XORSTR( "auto_strafe_retrack" ).get( ), value.m_auto_strafe_retrack );
    field( ASTRAL_XORSTR( "fov_override" ).get( ), value.m_fov_override );
    field( ASTRAL_XORSTR( "fov" ).get( ), value.m_fov );
    field( ASTRAL_XORSTR( "thirdperson" ).get( ), value.m_thirdperson );
    field( ASTRAL_XORSTR( "thirdperson_key" ).get( ), value.m_thirdperson_key );
    field( ASTRAL_XORSTR( "thirdperson_distance" ).get( ), value.m_thirdperson_distance );
    field( ASTRAL_XORSTR( "thirdperson_side" ).get( ), value.m_thirdperson_side );
    field( ASTRAL_XORSTR( "thirdperson_height" ).get( ), value.m_thirdperson_height );
    field( ASTRAL_XORSTR( "thirdperson_collision" ).get( ), value.m_thirdperson_collision );
    field( ASTRAL_XORSTR( "thirdperson_smooth" ).get( ), value.m_thirdperson_smooth );
    field( ASTRAL_XORSTR( "thirdperson_smooth_time" ).get( ), value.m_thirdperson_smooth_time );
    field( ASTRAL_XORSTR( "freecam" ).get( ), value.m_freecam );
    field( ASTRAL_XORSTR( "freecam_key" ).get( ), value.m_freecam_key );
    field( ASTRAL_XORSTR( "freecam_speed" ).get( ), value.m_freecam_speed );
    field( ASTRAL_XORSTR( "projectile_camera" ).get( ), value.m_projectile_camera );
    field( ASTRAL_XORSTR( "projectile_camera_mode" ).get( ), value.m_projectile_camera_mode );
    field( ASTRAL_XORSTR( "projectile_camera_hud" ).get( ), value.m_projectile_camera_hud );
    field( ASTRAL_XORSTR( "projectile_camera_distance" ).get( ), value.m_projectile_camera_distance );
    field( ASTRAL_XORSTR( "projectile_camera_width" ).get( ), value.m_projectile_camera_width );
    field( ASTRAL_XORSTR( "projectile_camera_pos" ).get( ), value.m_projectile_camera_position );
    field( ASTRAL_XORSTR( "spy_camera" ).get( ), value.m_spy_camera );
    field( ASTRAL_XORSTR( "spy_camera_range" ).get( ), value.m_spy_camera_range );
    field( ASTRAL_XORSTR( "aspect_ratio_changer" ).get( ), value.m_aspect_ratio_changer );
    field( ASTRAL_XORSTR( "aspect_ratio_amt" ).get( ), value.m_aspect_ratio_amt );
    field( ASTRAL_XORSTR( "noisemaker_spam" ).get( ), value.m_noisemaker_spam );
    field( ASTRAL_XORSTR( "noisemaker_spam_mode" ).get( ), value.m_noisemaker_spam_mode );
    field( ASTRAL_XORSTR( "clean_screenshots" ).get( ), value.m_clean_screenshots );
    field( ASTRAL_XORSTR( "clean_demos" ).get( ), value.m_clean_demos );
    field( ASTRAL_XORSTR( "telemetry_blocker" ).get( ), value.m_telemetry_blocker );
}

template< typename visitor >
static void config_fields( config::queue_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "max_ping" ).get( ), value.m_max_ping );
    field( ASTRAL_XORSTR( "desired" ).get( ), value.m_desired );
    field( ASTRAL_XORSTR( "delay_join" ).get( ), value.m_delay_join );
    field( ASTRAL_XORSTR( "delay_join_time" ).get( ), value.m_delay_join_time );
}

template< typename visitor >
static void config_fields( config::mvm_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "instant_respawn" ).get( ), value.m_instant_respawn );
    field( ASTRAL_XORSTR( "auto_ready" ).get( ), value.m_auto_ready );
    field( ASTRAL_XORSTR( "auto_mann_up_queue" ).get( ), value.m_auto_mann_up_queue );
    field( ASTRAL_XORSTR( "auto_boot_camp_queue" ).get( ), value.m_auto_boot_camp_queue );
    field( ASTRAL_XORSTR( "mann_up_tour" ).get( ), value.m_mann_up_tour );
    field( ASTRAL_XORSTR( "mann_up_uncompleted" ).get( ), value.m_mann_up_uncompleted );
    field( ASTRAL_XORSTR( "boot_camp_missions" ).get( ), value.m_boot_camp_missions );
    field( ASTRAL_XORSTR( "buybot" ).get( ), value.m_buybot );
    field( ASTRAL_XORSTR( "buybot_auto_class" ).get( ), value.m_buybot_auto_class );
    field( ASTRAL_XORSTR( "rent_items" ).get( ), value.m_rent_items );
    field( ASTRAL_XORSTR( "respec_recovery" ).get( ), value.m_respec_recovery );
    field( ASTRAL_XORSTR( "buybot_class" ).get( ), value.m_buybot_class );
    field( ASTRAL_XORSTR( "buybot_max_cash" ).get( ), value.m_buybot_max_cash );
}

template< typename visitor >
static void config_fields( config::exploits_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "seed_prediction" ).get( ), value.m_seed_prediction );
    field( ASTRAL_XORSTR( "seed_indicator" ).get( ), value.m_seed_indicator );
    field( ASTRAL_XORSTR( "fake_latency" ).get( ), value.m_fake_latency );
    field( ASTRAL_XORSTR( "fake_latency_key" ).get( ), value.m_fake_latency_key );
    field( ASTRAL_XORSTR( "fake_latency_ms" ).get( ), value.m_fake_latency_ms );
    field( ASTRAL_XORSTR( "extended_backtrack" ).get( ), value.m_extended_backtrack );
    field( ASTRAL_XORSTR( "ping_reducer" ).get( ), value.m_ping_reducer );
    field( ASTRAL_XORSTR( "ping_reducer_cmdrate" ).get( ), value.m_ping_reducer_cmdrate );
    field( ASTRAL_XORSTR( "rapid_fire" ).get( ), value.m_rapid_fire );
    field( ASTRAL_XORSTR( "rapid_fire_key" ).get( ), value.m_rapid_fire_key );
    field( ASTRAL_XORSTR( "anti_warp" ).get( ), value.m_anti_warp );
    field( ASTRAL_XORSTR( "indicator" ).get( ), value.m_indicator );
    field( ASTRAL_XORSTR( "warp" ).get( ), value.m_warp );
    field( ASTRAL_XORSTR( "warp_key" ).get( ), value.m_warp_key );
    field( ASTRAL_XORSTR( "warp_ticks" ).get( ), value.m_warp_ticks );
    field( ASTRAL_XORSTR( "recharge_key" ).get( ), value.m_recharge_key );
    field( ASTRAL_XORSTR( "passive_recharge" ).get( ), value.m_passive_recharge );
    field( ASTRAL_XORSTR( "fake_duck" ).get( ), value.m_fake_duck );
    field( ASTRAL_XORSTR( "fake_duck_key" ).get( ), value.m_fake_duck_key );
}

template< typename visitor >
static void config_fields( config::projectile_path_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "style" ).get( ), value.m_style );
    field( ASTRAL_XORSTR( "outline" ).get( ), value.m_outline );
    field( ASTRAL_XORSTR( "duration" ).get( ), value.m_duration );
    field( ASTRAL_XORSTR( "animate" ).get( ), value.m_animate );
    field( ASTRAL_XORSTR( "bounce_prediction" ).get( ), value.m_bounce_prediction );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "impact_box" ).get( ), value.m_impact_box );
    field( ASTRAL_XORSTR( "impact_color" ).get( ), value.m_impact_color );
}

template< typename visitor >
static void config_fields( config::trajectory_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "style" ).get( ), value.m_style );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "outline" ).get( ), value.m_outline );
    field( ASTRAL_XORSTR( "gradient" ).get( ), value.m_gradient );
    field( ASTRAL_XORSTR( "gradient_color" ).get( ), value.m_gradient_color );
    field( ASTRAL_XORSTR( "dim_occluded" ).get( ), value.m_dim_occluded );
    field( ASTRAL_XORSTR( "marker_color" ).get( ), value.m_marker_color );
    field( ASTRAL_XORSTR( "splash" ).get( ), value.m_splash );
    field( ASTRAL_XORSTR( "impact_box" ).get( ), value.m_impact_box );
    field( ASTRAL_XORSTR( "flight_time" ).get( ), value.m_flight_time );
    field( ASTRAL_XORSTR( "bounce_prediction" ).get( ), value.m_bounce_prediction );
}

template< typename visitor >
static void config_fields( config::proj_warning_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "style" ).get( ), value.m_style );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
    field( ASTRAL_XORSTR( "danger_color" ).get( ), value.m_danger_color );
    field( ASTRAL_XORSTR( "only_threats" ).get( ), value.m_only_threats );
    field( ASTRAL_XORSTR( "resting_stickies" ).get( ), value.m_resting_stickies );
}

template< typename visitor >
static void config_fields( config::ragebot_hitscan_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "hitscan_aimpoint_priority" ).get( ), value.m_aimpoint_priority );
    field( ASTRAL_XORSTR( "hitscan_boxes" ).get( ), value.m_hitscan_boxes );
    field( ASTRAL_XORSTR( "hitscan_multipoint" ).get( ), value.m_hitscan_multipoint );
    field( ASTRAL_XORSTR( "hitscan_multipoint_scale" ).get( ), value.m_hitscan_multipoint_scale );
    field( ASTRAL_XORSTR( "hitscan_hitchance" ).get( ), value.m_hitscan_hitchance );
    field( ASTRAL_XORSTR( "hitscan_roll_angle" ).get( ), value.m_hitscan_roll_angles );
    field( ASTRAL_XORSTR( "use_history" ).get( ), value.m_use_history );
}

template< typename visitor >
static void config_fields( config::ragebot_projectile_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "projectile_aimpoint_priority" ).get( ), value.m_aimpoint_priority );
    field( ASTRAL_XORSTR( "projectile_splashbot" ).get( ), value.m_projectile_splashbot );
    field( ASTRAL_XORSTR( "projectile_splashbot_mode" ).get( ), value.m_projectile_splashbot_mode );
    field( ASTRAL_XORSTR( "projectile_aimpoints" ).get( ), value.m_projectile_aimpoints );
    field( ASTRAL_XORSTR( "projectile_multipoint" ).get( ), value.m_projectile_multipoint );
    field( ASTRAL_XORSTR( "projectile_multipoint_scale" ).get( ), value.m_projectile_multipoint_scale );
    field( ASTRAL_XORSTR( "projectile_max_targets" ).get( ), value.m_projectile_max_targets );
    field( ASTRAL_XORSTR( "projectile_auto_charge" ).get( ), value.m_projectile_auto_charge );
    field( ASTRAL_XORSTR( "smark_donk" ).get( ), value.m_smark_donk );
    field( ASTRAL_XORSTR( "projectile_roll_angle" ).get( ), value.m_projectile_roll_angles );
    field( ASTRAL_XORSTR( "projectile_max_sim_time" ).get( ), value.m_projectile_max_sim_time );
    field( ASTRAL_XORSTR( "projectile_hitchance" ).get( ), value.m_projectile_hitchance );
    field( ASTRAL_XORSTR( "projectile_position_mode" ).get( ), value.m_projectile_position_mode );
}

template< typename visitor >
static void config_fields( config::ragebot_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "activate_key" ).get( ), value.m_activate_key );
    field( ASTRAL_XORSTR( "aim_type" ).get( ), value.m_aim_type );
    field( ASTRAL_XORSTR( "aim_rings" ).get( ), value.m_aim_rings );
    field( ASTRAL_XORSTR( "aim_ring_inner_fov" ).get( ), value.m_aim_ring_inner_fov );
    field( ASTRAL_XORSTR( "aim_ring_outer_fov" ).get( ), value.m_aim_ring_outer_fov );
    field( ASTRAL_XORSTR( "aim_ring_inner_type" ).get( ), value.m_aim_ring_inner_type );
    field( ASTRAL_XORSTR( "aim_ring_inner_thirdperson_type" ).get( ), value.m_aim_ring_inner_thirdperson_type );
    field( ASTRAL_XORSTR( "aim_ring_inner_firstperson_type" ).get( ), value.m_aim_ring_inner_firstperson_type );
    field( ASTRAL_XORSTR( "aim_ring_outer_type" ).get( ), value.m_aim_ring_outer_type );
    field( ASTRAL_XORSTR( "aim_ring_outer_thirdperson_type" ).get( ), value.m_aim_ring_outer_thirdperson_type );
    field( ASTRAL_XORSTR( "aim_ring_outer_firstperson_type" ).get( ), value.m_aim_ring_outer_firstperson_type );
    field( ASTRAL_XORSTR( "spectator_aim_types" ).get( ), value.m_spectator_aim_types );
    field( ASTRAL_XORSTR( "spectator_thirdperson_type" ).get( ), value.m_spectator_thirdperson_type );
    field( ASTRAL_XORSTR( "spectator_firstperson_type" ).get( ), value.m_spectator_firstperson_type );
    field( ASTRAL_XORSTR( "aim_strength" ).get( ), value.m_aim_strength );
    field( ASTRAL_XORSTR( "smooth_curve" ).get( ), value.m_smooth_curve );
    field( ASTRAL_XORSTR( "smooth_curve_amount" ).get( ), value.m_smooth_curve_amount );
    field( ASTRAL_XORSTR( "fov" ).get( ), value.m_fov );
    field( ASTRAL_XORSTR( "draw_fov" ).get( ), value.m_draw_fov );
    field( ASTRAL_XORSTR( "fov_color" ).get( ), value.m_fov_color );
    field( ASTRAL_XORSTR( "draw_crosshair" ).get( ), value.m_draw_crosshair );
    field( ASTRAL_XORSTR( "auto_shoot" ).get( ), value.m_auto_shoot );
    field( ASTRAL_XORSTR( "delay_shot" ).get( ), value.m_delay_shot );
    field( ASTRAL_XORSTR( "allow_aim_in_ui" ).get( ), value.m_allow_aim_in_ui );
    field( ASTRAL_XORSTR( "manual_backtrack" ).get( ), value.m_manual_backtrack );
    field( ASTRAL_XORSTR( "backtrack_crosshair" ).get( ), value.m_backtrack_crosshair );
    field( ASTRAL_XORSTR( "humanized_reaction_ms_min" ).get( ), value.m_humanized_reaction_ms_min );
    field( ASTRAL_XORSTR( "humanized_reaction_ms_max" ).get( ), value.m_humanized_reaction_ms_max );
    field( ASTRAL_XORSTR( "humanized_fire_delay_ms_min" ).get( ), value.m_humanized_fire_delay_ms_min );
    field( ASTRAL_XORSTR( "humanized_fire_delay_ms_max" ).get( ), value.m_humanized_fire_delay_ms_max );
    field( ASTRAL_XORSTR( "humanized_projectile_fire_delay_ms_min" ).get( ), value.m_humanized_projectile_fire_delay_ms_min );
    field( ASTRAL_XORSTR( "humanized_projectile_fire_delay_ms_max" ).get( ), value.m_humanized_projectile_fire_delay_ms_max );
    field( ASTRAL_XORSTR( "humanized_lock_ms_min" ).get( ), value.m_humanized_lock_ms_min );
    field( ASTRAL_XORSTR( "humanized_lock_ms_max" ).get( ), value.m_humanized_lock_ms_max );
    field( ASTRAL_XORSTR( "humanized_switch_ms_min" ).get( ), value.m_humanized_switch_ms_min );
    field( ASTRAL_XORSTR( "humanized_switch_ms_max" ).get( ), value.m_humanized_switch_ms_max );
    field( ASTRAL_XORSTR( "humanized_min_fire_interval_ms_min" ).get( ), value.m_humanized_min_fire_interval_ms_min );
    field( ASTRAL_XORSTR( "humanized_min_fire_interval_ms_max" ).get( ), value.m_humanized_min_fire_interval_ms_max );
    field( ASTRAL_XORSTR( "humanized_projectile_aim_tolerance_min" ).get( ), value.m_humanized_projectile_aim_tolerance_min );
    field( ASTRAL_XORSTR( "humanized_projectile_aim_tolerance_max" ).get( ), value.m_humanized_projectile_aim_tolerance_max );
    field( ASTRAL_XORSTR( "projectile_profiler" ).get( ), value.m_projectile_profiler );
    field( ASTRAL_XORSTR( "projectile_profiler_fps" ).get( ), value.m_projectile_profiler_fps );
    field( ASTRAL_XORSTR( "ragebot_targets" ).get( ), value.m_targets );
    field( ASTRAL_XORSTR( "ragebot_ignores" ).get( ), value.m_ignores );
    field( ASTRAL_XORSTR( "ragebot_preferences" ).get( ), value.m_preferences );
    field( ASTRAL_XORSTR( "prevent_bait" ).get( ), value.m_prevent_bait );
    field( ASTRAL_XORSTR( "bait_override_key" ).get( ), value.m_bait_override_key );
    field( ASTRAL_XORSTR( "splash_quality" ).get( ), value.m_splash_quality );
    field( ASTRAL_XORSTR( "target_classes" ).get( ), value.m_target_classes );

    field( ASTRAL_XORSTR( "projectile_draw_path" ).get( ), value.m_projectile_draw_path );
    field( ASTRAL_XORSTR( "projectile_path_color" ).get( ), value.m_projectile_path_color );
    field( ASTRAL_XORSTR( "projectile_path_low_confidence_color" ).get( ), value.m_projectile_path_low_confidence_color );
    field( ASTRAL_XORSTR( "projectile_path_style" ).get( ), value.m_projectile_path_style );
    field( ASTRAL_XORSTR( "projectile_path_outline" ).get( ), value.m_projectile_path_outline );
    field( ASTRAL_XORSTR( "projectile_path_fade" ).get( ), value.m_projectile_path_fade );
    field( ASTRAL_XORSTR( "projectile_path_fade_time" ).get( ), value.m_projectile_path_fade_time );
    field( ASTRAL_XORSTR( "projectile_path_animate" ).get( ), value.m_projectile_path_animate );
    field( ASTRAL_XORSTR( "projectile_path_gradient" ).get( ), value.m_projectile_path_gradient );
    field( ASTRAL_XORSTR( "projectile_path_color_end" ).get( ), value.m_projectile_path_color_end );
    field( ASTRAL_XORSTR( "projectile_path_bounds" ).get( ), value.m_projectile_path_bounds );
    field( ASTRAL_XORSTR( "projectile_path_bounds_style" ).get( ), value.m_projectile_path_bounds_style );
    field( ASTRAL_XORSTR( "projectile_path_bounds_color" ).get( ), value.m_projectile_path_bounds_color );
    field( ASTRAL_XORSTR( "hitscan_draw_hitboxes" ).get( ), value.m_hitscan_draw_hitboxes );
    field( ASTRAL_XORSTR( "hitscan_hitbox_color" ).get( ), value.m_hitscan_hitbox_color );
    field( ASTRAL_XORSTR( "hitscan_target_color" ).get( ), value.m_hitscan_target_color );
    field( ASTRAL_XORSTR( "hitscan_draw_shot" ).get( ), value.m_hitscan_draw_shot );
    field( ASTRAL_XORSTR( "hitscan_shot_color" ).get( ), value.m_hitscan_shot_color );
    field( ASTRAL_XORSTR( "hitscan_draw_duration" ).get( ), value.m_hitscan_draw_duration );

    field( ASTRAL_XORSTR( "auto_charge_sticky_targets" ).get( ), value.m_auto_charge_sticky_targets );
    field( ASTRAL_XORSTR( "huntsman_wait_for_kill" ).get( ), value.m_huntsman_wait_for_kill );

    field( ASTRAL_XORSTR( "hitscan_profiles" ).get( ), value.m_hitscan_profiles );
    field( ASTRAL_XORSTR( "projectile_profiles" ).get( ), value.m_projectile_profiles );
}

template< typename visitor >
static void config_fields( config::melee_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "activate_key" ).get( ), value.m_activate_key );
    field( ASTRAL_XORSTR( "aim_type" ).get( ), value.m_aim_type );
    field( ASTRAL_XORSTR( "auto_swing" ).get( ), value.m_auto_swing );
    field( ASTRAL_XORSTR( "auto_backstab" ).get( ), value.m_auto_backstab );
    field( ASTRAL_XORSTR( "swing_prediction" ).get( ), value.m_swing_prediction );
    field( ASTRAL_XORSTR( "always_crit" ).get( ), value.m_always_crit );
}

template< typename visitor >
static void config_fields( config::rjump_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "priority" ).get( ), value.m_priority );
    field( ASTRAL_XORSTR( "land_shot" ).get( ), value.m_land_shot );
    field( ASTRAL_XORSTR( "pogo" ).get( ), value.m_pogo );
    field( ASTRAL_XORSTR( "pogo_crouch" ).get( ), value.m_pogo_crouch );
}

template< typename visitor >
static void config_fields( config::lagcomp_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "draw_records" ).get( ), value.m_draw_records );
    field( ASTRAL_XORSTR( "extrapolate" ).get( ), value.m_extrapolate );
}

template< typename visitor >
static void config_fields( config::resolver_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
}

template< typename visitor >
static void config_fields( config::crits_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "force" ).get( ), value.m_force );
    field( ASTRAL_XORSTR( "avoid" ).get( ), value.m_avoid );
    field( ASTRAL_XORSTR( "indicator" ).get( ), value.m_indicator );
}

template< typename visitor >
static void config_fields( config::pills_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "seed_spin" ).get( ), value.m_seed_spin );
    field( ASTRAL_XORSTR( "crit_priority" ).get( ), value.m_crit_priority );
}

template< typename visitor >
static void config_fields( config::auto_det_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "radius_scale" ).get( ), value.m_radius_scale );
    field( ASTRAL_XORSTR( "min_targets" ).get( ), value.m_min_targets );
    field( ASTRAL_XORSTR( "avoid_self" ).get( ), value.m_avoid_self );
    field( ASTRAL_XORSTR( "target_buildings" ).get( ), value.m_target_buildings );
    field( ASTRAL_XORSTR( "dormant" ).get( ), value.m_dormant );
}

template< typename visitor >
static void config_fields( config::air_blast_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "aim" ).get( ), value.m_aim );
    field( ASTRAL_XORSTR( "redirect" ).get( ), value.m_redirect );
    field( ASTRAL_XORSTR( "prefer_crits" ).get( ), value.m_prefer_crits );
    field( ASTRAL_XORSTR( "extinguish" ).get( ), value.m_extinguish );
    field( ASTRAL_XORSTR( "push" ).get( ), value.m_push );
}

template< typename visitor >
static void config_fields( config::triggerbot_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "triggerbot_targets" ).get( ), value.m_targets );
    field( ASTRAL_XORSTR( "triggerbot_ignores" ).get( ), value.m_ignores );
    field( ASTRAL_XORSTR( "triggerbot_hitboxes" ).get( ), value.m_hitboxes );
}

template< typename visitor >
static void config_fields( config::auto_vacc_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "auto_pop" ).get( ), value.m_auto_pop );
    field( ASTRAL_XORSTR( "idle_resist" ).get( ), value.m_idle_resist );
    field( ASTRAL_XORSTR( "reactivity" ).get( ), value.m_reactivity );
    field( ASTRAL_XORSTR( "request_bullet_key" ).get( ), value.m_request_bullet_key );
    field( ASTRAL_XORSTR( "request_blast_key" ).get( ), value.m_request_blast_key );
    field( ASTRAL_XORSTR( "request_fire_key" ).get( ), value.m_request_fire_key );
}

template< typename visitor >
static void config_fields( config::skin_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "model" ).get( ), value.m_model );
    field( ASTRAL_XORSTR( "paintkit" ).get( ), value.m_paintkit );
    field( ASTRAL_XORSTR( "wear" ).get( ), value.m_wear );
    field( ASTRAL_XORSTR( "seed" ).get( ), value.m_seed );
    field( ASTRAL_XORSTR( "australium" ).get( ), value.m_australium );
    field( ASTRAL_XORSTR( "festivized" ).get( ), value.m_festivized );
    field( ASTRAL_XORSTR( "unusual" ).get( ), value.m_unusual );
    field( ASTRAL_XORSTR( "killstreak" ).get( ), value.m_killstreak );
    field( ASTRAL_XORSTR( "sheen" ).get( ), value.m_sheen );
    field( ASTRAL_XORSTR( "killstreaker" ).get( ), value.m_killstreaker );
}

template< typename visitor >
static void config_fields( config::skin_entry_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "definition" ).get( ), value.m_definition );
    field( ASTRAL_XORSTR( "skin" ).get( ), value.m_skin );
}

template< typename visitor >
static void config_fields( config::skins_t::advanced_item_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "id" ).get( ), value.m_id );
    field( ASTRAL_XORSTR( "definition" ).get( ), value.m_definition );
    field( ASTRAL_XORSTR( "class" ).get( ), value.m_player_class );
    field( ASTRAL_XORSTR( "slot" ).get( ), value.m_slot );
    field( ASTRAL_XORSTR( "quality" ).get( ), value.m_quality );
    field( ASTRAL_XORSTR( "level" ).get( ), value.m_level );
    field( ASTRAL_XORSTR( "cosmetic_unusual" ).get( ), value.m_cosmetic_unusual );
    field( ASTRAL_XORSTR( "skin" ).get( ), value.m_skin );
}

template< typename visitor >
static void config_fields( config::wearable_skin_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "unusual" ).get( ), value.m_unusual );
}

template< typename visitor >
static void config_fields( config::wearable_entry_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "definition" ).get( ), value.m_definition );
    field( ASTRAL_XORSTR( "skin" ).get( ), value.m_skin );
}

template< typename visitor >
static void config_fields( config::spells_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "pumpkin_bombs" ).get( ), value.m_pumpkin_bombs );
    field( ASTRAL_XORSTR( "voices_from_below" ).get( ), value.m_voices_from_below );
    field( ASTRAL_XORSTR( "green_flames" ).get( ), value.m_green_flames );
    field( ASTRAL_XORSTR( "footprints" ).get( ), value.m_footprints );
    field( ASTRAL_XORSTR( "footprint_custom" ).get( ), value.m_footprint_custom );
    field( ASTRAL_XORSTR( "footprint_color" ).get( ), value.m_footprint_color );
    field( ASTRAL_XORSTR( "jingle" ).get( ), value.m_jingle );
    field( ASTRAL_XORSTR( "replace_step" ).get( ), value.m_replace_step );
    field( ASTRAL_XORSTR( "full_moon" ).get( ), value.m_full_moon );
}

template< typename visitor >
static void config_fields( config::ragdolls_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "targets" ).get( ), value.m_targets );
    field( ASTRAL_XORSTR( "gold" ).get( ), value.m_gold );
    field( ASTRAL_XORSTR( "ice" ).get( ), value.m_ice );
    field( ASTRAL_XORSTR( "dissolve" ).get( ), value.m_dissolve );
    field( ASTRAL_XORSTR( "burning" ).get( ), value.m_burning );
    field( ASTRAL_XORSTR( "electrocuted" ).get( ), value.m_electrocuted );
    field( ASTRAL_XORSTR( "ash" ).get( ), value.m_ash );
}

template< typename visitor >
static void config_fields( config::skins_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "mode" ).get( ), value.m_mode );
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "shared" ).get( ), value.m_shared );
    field( ASTRAL_XORSTR( "advanced_next_id" ).get( ), value.m_advanced_next_id );
    field( ASTRAL_XORSTR( "advanced_enabled" ).get( ), value.m_advanced_enabled );
    field( ASTRAL_XORSTR( "advanced_edit_quality" ).get( ), value.m_advanced_edit_quality );
    field( ASTRAL_XORSTR( "advanced_edit_level" ).get( ), value.m_advanced_edit_level );
    field( ASTRAL_XORSTR( "advanced_edit_cosmetic_unusual" ).get( ), value.m_advanced_edit_cosmetic_unusual );
    field( ASTRAL_XORSTR( "advanced_items" ).get( ), value.m_advanced_items );
    field( ASTRAL_XORSTR( "advanced_loadouts" ).get( ), value.m_advanced_loadouts );
    field( ASTRAL_XORSTR( "weapons" ).get( ), value.m_weapons );
    field( ASTRAL_XORSTR( "sheen_custom" ).get( ), value.m_sheen_custom );
    field( ASTRAL_XORSTR( "sheen_color" ).get( ), value.m_sheen_color );
    field( ASTRAL_XORSTR( "wearable_slot" ).get( ), value.m_wearable_slot );
    field( ASTRAL_XORSTR( "wearables" ).get( ), value.m_wearables );
    field( ASTRAL_XORSTR( "spells" ).get( ), value.m_spells );
    field( ASTRAL_XORSTR( "ragdolls" ).get( ), value.m_ragdolls );
}

template< typename visitor >
static void config_fields( config::radio_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "source" ).get( ), value.m_source );
    field( ASTRAL_XORSTR( "station" ).get( ), value.m_station );
    field( ASTRAL_XORSTR( "volume" ).get( ), value.m_volume );
    field( ASTRAL_XORSTR( "scale_to_game_volume" ).get( ), value.m_scale_to_game_volume );
    field( ASTRAL_XORSTR( "indicator" ).get( ), value.m_indicator );
    field( ASTRAL_XORSTR( "indicator_style" ).get( ), value.m_indicator_style );
    field( ASTRAL_XORSTR( "indicator_position" ).get( ), value.m_indicator_position );
    field( ASTRAL_XORSTR( "duck_on_voice" ).get( ), value.m_duck_on_voice );
    field( ASTRAL_XORSTR( "duck_on_self" ).get( ), value.m_duck_on_self );
    field( ASTRAL_XORSTR( "duck_on_others" ).get( ), value.m_duck_on_others );
    field( ASTRAL_XORSTR( "duck_level" ).get( ), value.m_duck_level );
    field( ASTRAL_XORSTR( "mute_unfocused" ).get( ), value.m_mute_unfocused );
    field( ASTRAL_XORSTR( "crossfade" ).get( ), value.m_crossfade );
    field( ASTRAL_XORSTR( "next_key" ).get( ), value.m_next_key );
    field( ASTRAL_XORSTR( "previous_key" ).get( ), value.m_previous_key );
    field( ASTRAL_XORSTR( "louder_key" ).get( ), value.m_louder_key );
    field( ASTRAL_XORSTR( "quieter_key" ).get( ), value.m_quieter_key );
}

template< typename visitor >
static void config_fields( config::soundboard_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "key" ).get( ), value.m_key );
    field( ASTRAL_XORSTR( "loop" ).get( ), value.m_loop );
    field( ASTRAL_XORSTR( "volume" ).get( ), value.m_volume );
}

template< typename visitor >
static void config_fields( config::recorder_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "record_key" ).get( ), value.m_record_key );
    field( ASTRAL_XORSTR( "replay_key" ).get( ), value.m_replay_key );
    field( ASTRAL_XORSTR( "replay_attacks" ).get( ), value.m_replay_attacks );
    field( ASTRAL_XORSTR( "camera_follows" ).get( ), value.m_camera_follows );
    field( ASTRAL_XORSTR( "markers" ).get( ), value.m_markers );
    field( ASTRAL_XORSTR( "marker_color" ).get( ), value.m_marker_color );
    field( ASTRAL_XORSTR( "marker_distance" ).get( ), value.m_marker_distance );
    field( ASTRAL_XORSTR( "route" ).get( ), value.m_route );
    field( ASTRAL_XORSTR( "route_color" ).get( ), value.m_route_color );
    field( ASTRAL_XORSTR( "traveled_color" ).get( ), value.m_traveled_color );
    field( ASTRAL_XORSTR( "route_style" ).get( ), value.m_route_style );
    field( ASTRAL_XORSTR( "key_overlay" ).get( ), value.m_key_overlay );
    field( ASTRAL_XORSTR( "key_overlay_position" ).get( ), value.m_key_overlay_position );
}

template< typename visitor >
static void config_fields( config::streamer_mode_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "hide_targets" ).get( ), value.m_hide_targets );
    field( ASTRAL_XORSTR( "stream_proof" ).get( ), value.m_stream_proof );
}

template< typename visitor >
static void config_fields( config::debug_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "server_hitboxes" ).get( ), value.m_server_hitboxes );
    field( ASTRAL_XORSTR( "server_hitbox_color" ).get( ), value.m_server_hitbox_color );
    field( ASTRAL_XORSTR( "client_hitboxes" ).get( ), value.m_client_hitboxes );
    field( ASTRAL_XORSTR( "client_hitbox_color" ).get( ), value.m_client_hitbox_color );
    field( ASTRAL_XORSTR( "triggers" ).get( ), value.m_triggers );
    field( ASTRAL_XORSTR( "trigger_colors" ).get( ), value.m_trigger_colors );
    field( ASTRAL_XORSTR( "trigger_distance" ).get( ), value.m_trigger_distance );
}

template< typename visitor >
static void config_fields( config::events_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "event_targets" ).get( ), value.m_notify_targets );
    field( ASTRAL_XORSTR( "vote_start_notify_party" ).get( ), value.m_vote_start_notify_party );
}

template< typename visitor >
static void config_fields( config::player_tag_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "account" ).get( ), value.m_account );
    field( ASTRAL_XORSTR( "tag" ).get( ), value.m_tag );
    field( ASTRAL_XORSTR( "alias" ).get( ), value.m_alias );
    field( ASTRAL_XORSTR( "groups" ).get( ), value.m_groups );
    field( ASTRAL_XORSTR( "marks" ).get( ), value.m_marks );
}

template< typename visitor >
static void config_fields( config::player_group_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "name" ).get( ), value.m_name );
    field( ASTRAL_XORSTR( "color" ).get( ), value.m_color );
}

template< typename visitor >
static void config_fields( config::medibot_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "enabled" ).get( ), value.m_enabled );
    field( ASTRAL_XORSTR( "activate_key" ).get( ), value.m_activate_key );
    field( ASTRAL_XORSTR( "prefer_mode" ).get( ), value.m_prefer_mode );
    field( ASTRAL_XORSTR( "auto_uber" ).get( ), value.m_auto_uber );
    field( ASTRAL_XORSTR( "auto_kritz" ).get( ), value.m_auto_kritz );
}

template< typename visitor >
static void config_fields( config::anti_aim_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "yaw_base" ).get( ), value.m_yaw_base );
    field( ASTRAL_XORSTR( "real_yaw_mode" ).get( ), value.m_real_yaw_mode );
    field( ASTRAL_XORSTR( "fake_yaw_mode" ).get( ), value.m_fake_yaw_mode );
    field( ASTRAL_XORSTR( "real_pitch_mode" ).get( ), value.m_real_pitch_mode );
    field( ASTRAL_XORSTR( "fake_pitch_mode" ).get( ), value.m_fake_pitch_mode );
    field( ASTRAL_XORSTR( "static_real_angle" ).get( ), value.m_static_real_angle );
    field( ASTRAL_XORSTR( "static_fake_angle" ).get( ), value.m_static_fake_angle );
    field( ASTRAL_XORSTR( "cylical_pos_1_real" ).get( ), value.m_cylical_pos_1_real );
    field( ASTRAL_XORSTR( "cylical_pos_2_real" ).get( ), value.m_cylical_pos_2_real );
    field( ASTRAL_XORSTR( "cylical_pos_1_fake" ).get( ), value.m_cylical_pos_1_fake );
    field( ASTRAL_XORSTR( "cylical_pos_2_fake" ).get( ), value.m_cylical_pos_2_fake );
    field( ASTRAL_XORSTR( "real_cycle_time" ).get( ), value.m_real_cycle_time );
    field( ASTRAL_XORSTR( "fake_cycle_time" ).get( ), value.m_fake_cycle_time );
    field( ASTRAL_XORSTR( "apply_jitter_real" ).get( ), value.m_apply_jitter_real );
    field( ASTRAL_XORSTR( "apply_jitter_fake" ).get( ), value.m_apply_jitter_fake );
    field( ASTRAL_XORSTR( "jitter_amount" ).get( ), value.m_jitter_amount );
    field( ASTRAL_XORSTR( "spin_amount" ).get( ), value.m_spin_amount );
    field( ASTRAL_XORSTR( "break_pitch" ).get( ), value.m_break_pitch );
}

template< typename visitor >
static void config_fields( config::fakelag_profile_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "fakelag_mode" ).get( ), value.m_fakelag_mode );
    field( ASTRAL_XORSTR( "static_amount" ).get( ), value.m_static_amount );
    field( ASTRAL_XORSTR( "random_min" ).get( ), value.m_random_min );
    field( ASTRAL_XORSTR( "random_max" ).get( ), value.m_random_max );
    field( ASTRAL_XORSTR( "adaptive_randomized" ).get( ), value.m_adaptive_randomized_choke );
}

template< typename visitor >
static void config_fields( config::anti_hack_t &value, visitor &field )
{
    field( ASTRAL_XORSTR( "aa_enabled" ).get( ), value.m_anti_aim_enabled );
    field( ASTRAL_XORSTR( "fl_enabled" ).get( ), value.m_fakelag_enabled );
    field( ASTRAL_XORSTR( "retain_blastjump" ).get( ), value.m_retain_blastjump );
    field( ASTRAL_XORSTR( "retain_blastjump_soldier_only" ).get( ), value.m_retain_blastjump_soldier_only );
    field( ASTRAL_XORSTR( "aa_key" ).get( ), value.m_anti_aim_key );
    field( ASTRAL_XORSTR( "fl_key" ).get( ), value.m_fakelag_key );
    field( ASTRAL_XORSTR( "flip_key" ).get( ), value.m_flip_key );
    field( ASTRAL_XORSTR( "aa_mode" ).get( ), value.m_aa_mode );
    field( ASTRAL_XORSTR( "hs_threat_only" ).get( ), value.m_headshot_threats_only );
    field( ASTRAL_XORSTR( "anti_backstab" ).get( ), value.m_anti_backstab );
    field( ASTRAL_XORSTR( "aa_profiles" ).get( ), value.m_aa_profiles );
    field( ASTRAL_XORSTR( "fl_profiles" ).get( ), value.m_fl_profiles );
    field( ASTRAL_XORSTR( "draw_real" ).get( ), value.m_draw_real_hitboxes );
    field( ASTRAL_XORSTR( "draw_fake" ).get( ), value.m_draw_fake_hitboxes );
    field( ASTRAL_XORSTR( "real_draw_mode" ).get( ), value.m_real_draw_mode );
    field( ASTRAL_XORSTR( "fake_draw_mode" ).get( ), value.m_fake_draw_mode );
    field( ASTRAL_XORSTR( "real_base_color" ).get( ), value.m_real_base_color );
    field( ASTRAL_XORSTR( "fake_base_color" ).get( ), value.m_fake_base_color );
    field( ASTRAL_XORSTR( "real_highlight_color" ).get( ), value.m_real_highlight_color );
    field( ASTRAL_XORSTR( "fake_highlight_color" ).get( ), value.m_fake_highlight_color );
    field( ASTRAL_XORSTR( "fake_chams" ).get( ), value.m_fake_chams );
    field( ASTRAL_XORSTR( "fake_chams_color" ).get( ), value.m_fake_chams_color );
    field( ASTRAL_XORSTR( "fake_chams_custom" ).get( ), value.m_fake_chams_custom );
    field( ASTRAL_XORSTR( "fake_glow" ).get( ), value.m_fake_glow );
}

template< typename visitor >
static void config_fields( config &value, visitor &field )
{
    field( ASTRAL_XORSTR( "esp" ).get( ), value.m_esp );
    field( ASTRAL_XORSTR( "hitmarker" ).get( ), value.m_hitmarker );
    field( ASTRAL_XORSTR( "chams" ).get( ), value.m_chams );
    field( ASTRAL_XORSTR( "glow" ).get( ), value.m_glow );
    field( ASTRAL_XORSTR( "weather" ).get( ), value.m_weather );
    field( ASTRAL_XORSTR( "tracers" ).get( ), value.m_tracers );
    field( ASTRAL_XORSTR( "arrows" ).get( ), value.m_arrows );
    field( ASTRAL_XORSTR( "skybox" ).get( ), value.m_skybox );
    field( ASTRAL_XORSTR( "world" ).get( ), value.m_world );
    field( ASTRAL_XORSTR( "fog" ).get( ), value.m_fog );
    field( ASTRAL_XORSTR( "post_process" ).get( ), value.m_post_process );
    field( ASTRAL_XORSTR( "radar" ).get( ), value.m_radar );
    field( ASTRAL_XORSTR( "viewmodel" ).get( ), value.m_viewmodel );
    field( ASTRAL_XORSTR( "misc" ).get( ), value.m_misc );
    field( ASTRAL_XORSTR( "queue" ).get( ), value.m_queue );
    field( ASTRAL_XORSTR( "mvm" ).get( ), value.m_mvm );
    field( ASTRAL_XORSTR( "exploits" ).get( ), value.m_exploits );
    field( ASTRAL_XORSTR( "ragebot" ).get( ), value.m_ragebot );
    field( ASTRAL_XORSTR( "melee" ).get( ), value.m_melee );
    field( ASTRAL_XORSTR( "rjump" ).get( ), value.m_rjump );
    field( ASTRAL_XORSTR( "lagcomp" ).get( ), value.m_lagcomp );
    field( ASTRAL_XORSTR( "resolver" ).get( ), value.m_resolver );
    field( ASTRAL_XORSTR( "crits" ).get( ), value.m_crits );
    field( ASTRAL_XORSTR( "pills" ).get( ), value.m_pills );
    field( ASTRAL_XORSTR( "auto_vacc" ).get( ), value.m_auto_vacc );
    field( ASTRAL_XORSTR( "auto_det" ).get( ), value.m_auto_det );
    field( ASTRAL_XORSTR( "air_blast" ).get( ), value.m_air_blast );
    field( ASTRAL_XORSTR( "triggerbot" ).get( ), value.m_triggerbot );
    field( ASTRAL_XORSTR( "skins" ).get( ), value.m_skins );
    field( ASTRAL_XORSTR( "radio" ).get( ), value.m_radio );
    field( ASTRAL_XORSTR( "soundboard" ).get( ), value.m_soundboard );
    field( ASTRAL_XORSTR( "projectile_path" ).get( ), value.m_projectile_path );
    field( ASTRAL_XORSTR( "trajectory" ).get( ), value.m_trajectory );
    field( ASTRAL_XORSTR( "projectile_warning" ).get( ), value.m_projectile_warning );
    field( ASTRAL_XORSTR( "streamer_mode" ).get( ), value.m_streamer_mode );
    field( ASTRAL_XORSTR( "debug" ).get( ), value.m_debug );
    field( ASTRAL_XORSTR( "medibot" ).get( ), value.m_medibot );
    field( ASTRAL_XORSTR( "events" ).get( ), value.m_events );
    field( ASTRAL_XORSTR( "player_tags" ).get( ), value.m_player_tags );
    field( ASTRAL_XORSTR( "player_groups" ).get( ), value.m_player_groups );
    field( ASTRAL_XORSTR( "recorder" ).get( ), value.m_recorder );
    field( ASTRAL_XORSTR( "anti_hack" ).get( ), value.m_anti_hack );
}
