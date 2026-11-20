#include "config.hpp"

#include "config_storage.hpp"
#include "features/chams.hpp"
#include "gui/notifications.hpp"
#include "gui/theme.hpp"
#include "preview.hpp"
#include "runtime/thread.hpp"
#include "script/script.hpp"

#include "config_fields.inl"

ASTRAL_NO_DESTROY config g_cfg{ };

template< typename value_type >
static void write_config_value( json::writer &writer, value_type &value );

template< typename value_type, usize size >
static void write_config_value( json::writer &writer, value_type ( &value )[ size ] );

static void write_config_value(
    json::writer &writer,
    config::skins_t::advanced_item_t ( &value )[ config::ADVANCED_ITEM_COUNT ] );

template< usize size >
static void write_config_value( json::writer &writer, char ( &value )[ size ] );

template< typename value_type >
static bool read_config_value( json::value node, value_type &value );

template< typename value_type, usize size >
static bool read_config_value( json::value node, value_type ( &value )[ size ] );

template< usize size >
static bool read_config_value( json::value node, char ( &value )[ size ] );

struct config_writer
{
    json::writer &m_writer;

    template< typename value_type >
    void operator( )( const char *name, value_type &value )
    {
        this->m_writer.key( name );
        write_config_value( this->m_writer, value );
    }
};

struct config_reader
{
    json::value m_node;
    bool        m_valid{ true };

    template< typename value_type >
    void operator( )( const char *name, value_type &value )
    {
        const json::value member{ this->m_node[ name ] };
        if ( member.valid( ) && !read_config_value( member, value ) )
        {
            this->m_valid = false;
        }
    }
};

template< typename value_type >
static void write_config_value( json::writer &writer, value_type &value )
{
    if constexpr ( __is_enum( value_type ) )
    {
        writer.write( static_cast< u32 >( value ) );
    } else if constexpr ( json::detail::is_number< value_type > )
    {
        writer.write( value );
    } else
    {
        writer.begin_object( );
        config_writer field{ writer };
        config_fields( value, field );
        writer.end_object( );
    }
}

template< typename value_type, usize size >
static void write_config_value( json::writer &writer, value_type ( &value )[ size ] )
{
    writer.begin_array( );
    for ( auto &element : value )
    {
        write_config_value( writer, element );
    }

    writer.end_array( );
}

// Advanced inventory storage is fixed-capacity at runtime, but serializing all 128 empty records
// makes an otherwise ordinary config exceed the established 256 KiB transfer limit. Preserve all
// occupied slots and any holes before the last one; the loader starts from config{} and already
// accepts arrays shorter than their destination capacity.
static void write_config_value(
    json::writer &writer,
    config::skins_t::advanced_item_t ( &value )[ config::ADVANCED_ITEM_COUNT ] )
{
    usize count{ config::ADVANCED_ITEM_COUNT };
    while ( count && !value[ count - 1 ].m_id )
    {
        --count;
    }

    writer.begin_array( );
    for ( usize index{ }; index < count; ++index )
    {
        write_config_value( writer, value[ index ] );
    }
    writer.end_array( );
}

template< usize size >
static void write_config_value( json::writer &writer, char ( &value )[ size ] )
{
    writer.write( string_view{ value } );
}

template< typename value_type >
static bool read_config_value( json::value node, value_type &value )
{
    if constexpr ( json::detail::is_same< value_type, bool > )
    {
        if ( node.type( ) != json::type_t::boolean )
        {
            return false;
        }

        value = node.as< bool >( );
    } else if constexpr ( __is_enum( value_type ) )
    {
        if ( node.type( ) != json::type_t::number )
        {
            return false;
        }

        const i64 number{ node.as< i64 >( -1 ) };
        if ( number < 0 || number > config_enum_maximum< value_type > || node.as< f64 >( ) != number )
        {
            return false;
        }

        value = static_cast< value_type >( number );
    } else if constexpr ( json::detail::is_number< value_type > )
    {
        if ( node.type( ) != json::type_t::number )
        {
            return false;
        }

        const value_type number{ node.as< value_type >( ) };
        if ( static_cast< f64 >( number ) != node.as< f64 >( ) && !json::detail::is_real< value_type > )
        {
            return false;
        }

        value = number;
    } else
    {
        if ( node.type( ) != json::type_t::object )
        {
            return false;
        }

        config_reader field{ node };
        config_fields( value, field );
        if constexpr ( json::detail::is_same< value_type, bind_t > )
        {
            return field.m_valid && value.m_key <= CONFIG_BIND_KEY_MAXIMUM;
        }

        return field.m_valid;
    }

    return true;
}

template< typename value_type, usize size >
static bool read_config_value( json::value node, value_type ( &value )[ size ] )
{
    if ( node.type( ) != json::type_t::array || node.size( ) > size )
    {
        return false;
    }

    for ( usize i{ }; i < node.size( ); ++i )
    {
        if ( !read_config_value( node[ i ], value[ i ] ) )
        {
            return false;
        }
    }

    return true;
}

template< usize size >
static bool read_config_value( json::value node, char ( &value )[ size ] )
{
    if ( node.type( ) != json::type_t::string )
        return false;
    const string_view text{ node.as< string_view >( ) };
    if ( text.size( ) >= size )
        return false;
    value[ 0 ] = 0;
    if ( text.empty( ) )
        return true;
    str::copy( value, size, text.data( ), text.size( ) );
    return true;
}

template< typename value_type >
static void clamp_config_vars( value_type &value, value_type minimum, value_type maximum )
{
    value = math::clamp( value, minimum, maximum );
}

bool valid_config( config &value )
{
    clamp_config_vars( value.m_misc.m_ui_scale, 100, 200 );
    clamp_config_vars( value.m_misc.m_notification_lifetime, 1000, 15000 );
    clamp_config_vars( value.m_misc.m_notification_position, static_cast< usize >( 0 ),
                       static_cast< usize >( 3 ) );
    clamp_config_vars( value.m_ragebot.m_fov, 1, 180 );
    clamp_config_vars( value.m_ragebot.m_aim_strength, 1, 100 );
    clamp_config_vars( value.m_ragebot.m_smooth_curve, static_cast< usize >( 0 ),
                       static_cast< usize >( config::ragebot_t::SMOOTH_CURVE_COUNT - 1 ) );
    clamp_config_vars( value.m_ragebot.m_smooth_curve_amount, 0, 200 );
    clamp_config_vars( value.m_ragebot.m_humanized_reaction_ms_min, 0, 1000 );
    clamp_config_vars( value.m_ragebot.m_humanized_reaction_ms_max, 0, 1000 );
    clamp_config_vars( value.m_ragebot.m_humanized_fire_delay_ms_min, 0, 1000 );
    clamp_config_vars( value.m_ragebot.m_humanized_fire_delay_ms_max, 0, 1000 );
    clamp_config_vars( value.m_ragebot.m_humanized_projectile_fire_delay_ms_min, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_projectile_fire_delay_ms_max, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_lock_ms_min, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_lock_ms_max, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_switch_ms_min, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_switch_ms_max, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_min_fire_interval_ms_min, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_min_fire_interval_ms_max, 0, 2000 );
    clamp_config_vars( value.m_ragebot.m_humanized_projectile_aim_tolerance_min, 10, 300 );
    clamp_config_vars( value.m_ragebot.m_humanized_projectile_aim_tolerance_max, 10, 300 );
    if ( value.m_ragebot.m_humanized_reaction_ms_min > value.m_ragebot.m_humanized_reaction_ms_max )
        value.m_ragebot.m_humanized_reaction_ms_min = value.m_ragebot.m_humanized_reaction_ms_max;
    if ( value.m_ragebot.m_humanized_fire_delay_ms_min > value.m_ragebot.m_humanized_fire_delay_ms_max )
        value.m_ragebot.m_humanized_fire_delay_ms_min = value.m_ragebot.m_humanized_fire_delay_ms_max;
    if ( value.m_ragebot.m_humanized_projectile_fire_delay_ms_min > value.m_ragebot.m_humanized_projectile_fire_delay_ms_max )
        value.m_ragebot.m_humanized_projectile_fire_delay_ms_min = value.m_ragebot.m_humanized_projectile_fire_delay_ms_max;
    if ( value.m_ragebot.m_humanized_lock_ms_min > value.m_ragebot.m_humanized_lock_ms_max )
        value.m_ragebot.m_humanized_lock_ms_min = value.m_ragebot.m_humanized_lock_ms_max;
    if ( value.m_ragebot.m_humanized_switch_ms_min > value.m_ragebot.m_humanized_switch_ms_max )
        value.m_ragebot.m_humanized_switch_ms_min = value.m_ragebot.m_humanized_switch_ms_max;
    if ( value.m_ragebot.m_humanized_min_fire_interval_ms_min > value.m_ragebot.m_humanized_min_fire_interval_ms_max )
        value.m_ragebot.m_humanized_min_fire_interval_ms_min = value.m_ragebot.m_humanized_min_fire_interval_ms_max;
    if ( value.m_ragebot.m_humanized_projectile_aim_tolerance_min > value.m_ragebot.m_humanized_projectile_aim_tolerance_max )
        value.m_ragebot.m_humanized_projectile_aim_tolerance_min = value.m_ragebot.m_humanized_projectile_aim_tolerance_max;
    clamp_config_vars( value.m_ragebot.m_hitscan_draw_duration, 0.2f, 5.0f );
    clamp_config_vars( value.m_ragebot.m_projectile_path_fade_time, 0.2f, 3.0f );
    clamp_config_vars( value.m_projectile_path.m_duration, 0.2f, 5.0f );
    clamp_config_vars( value.m_esp.m_dormant_time, 1, 15 );
    clamp_config_vars( value.m_fog.m_start, 0, 8000 );
    clamp_config_vars( value.m_fog.m_end, 0, 16000 );
    clamp_config_vars( value.m_fog.m_density, 0, 100 );
    clamp_config_vars( value.m_weather.m_density, 0, 100 );
    clamp_config_vars( value.m_weather.m_wind_speed, 0, 500 );
    clamp_config_vars( value.m_weather.m_wind_direction, 0, 360 );
    clamp_config_vars( value.m_viewmodel.m_forward, -30, 30 );
    clamp_config_vars( value.m_viewmodel.m_right, -30, 30 );
    clamp_config_vars( value.m_viewmodel.m_up, -30, 30 );
    clamp_config_vars( value.m_viewmodel.m_bob, 0, 200 );
    clamp_config_vars( value.m_viewmodel.m_sway, 0, 200 );
    clamp_config_vars( value.m_viewmodel.m_aim_swing, 0, 400 );
    clamp_config_vars( value.m_tracers.m_duration, 100, 3000 );
    clamp_config_vars( value.m_arrows.m_radius, 60, 500 );
    clamp_config_vars( value.m_arrows.m_size, 8, 40 );
    clamp_config_vars( value.m_misc.m_fov, 60, 140 );
    clamp_config_vars( value.m_misc.m_thirdperson_distance, 30, 300 );
    clamp_config_vars( value.m_misc.m_thirdperson_side, -100, 100 );
    clamp_config_vars( value.m_misc.m_thirdperson_height, -100, 100 );
    clamp_config_vars( value.m_misc.m_thirdperson_smooth_time, 0, 1000 );
    clamp_config_vars( value.m_misc.m_freecam_speed, 100, 3000 );
    clamp_config_vars( value.m_misc.m_projectile_camera_distance, 0, 1000 );
    clamp_config_vars( value.m_misc.m_projectile_camera_width, 160, 800 );
    clamp_config_vars( value.m_misc.m_menu_sound_volume, 0, 100 );
    clamp_config_vars( value.m_radio.m_volume, 0, 100 );
    clamp_config_vars( value.m_radio.m_source, static_cast< usize >( 0 ),
                       static_cast< usize >( config::RADIO_SOURCE_COUNT - 1 ) );
    clamp_config_vars( value.m_soundboard.m_volume, 0, 500 );
    clamp_config_vars( value.m_radio.m_duck_level, 0, 100 );
    clamp_config_vars( value.m_radio.m_crossfade, 0, 2000 );
    clamp_config_vars( value.m_auto_det.m_radius_scale, 40, 100 );
    clamp_config_vars( value.m_auto_det.m_min_targets, 1, 6 );
    clamp_config_vars( value.m_misc.m_auto_strafe_retrack, 10.0f, 100.0f );
    clamp_config_vars( value.m_misc.m_strafe_optimizer_gain, 0.0f, 100.0f );
    clamp_config_vars( value.m_misc.m_strafe_optimizer_min_speed, 0, 520 );
    clamp_config_vars( value.m_misc.m_strafe_avoid_distance, 8.0f, 200.0f );
    clamp_config_vars( value.m_misc.m_auto_crouch_ticks, 1, 32 );
    clamp_config_vars( value.m_exploits.m_fake_latency_ms, 50, 900 );
    clamp_config_vars( value.m_exploits.m_ping_reducer_cmdrate, 1, 66 );
    clamp_config_vars( value.m_radar.m_size, 180, 480 );
    clamp_config_vars( value.m_radar.m_range, 500, 8000 );
    clamp_config_vars( value.m_radar.m_icon_size, 10, 32 );
    if ( value.m_radar.m_icon_style >= config::RADAR_ICON_COUNT )
        value.m_radar.m_icon_style = config::RADAR_ICON_REGULAR;
    clamp_config_vars( value.m_radar.m_map_detail, 1, 3 );
    if ( value.m_radar.m_map_mode >= config::RADAR_MAP_COUNT )
        value.m_radar.m_map_mode = config::RADAR_MAP_DOOM;
    clamp_config_vars( value.m_exploits.m_warp_ticks, 2, 22 );
    clamp_config_vars( value.m_queue.m_max_ping, 10, 1000 );
    clamp_config_vars( value.m_queue.m_delay_join_time, 15, 1000 );
    clamp_config_vars( value.m_recorder.m_marker_distance, 500, 6000 );

    if ( value.m_esp.m_target_group >= config::CHAM_ENTITY_COUNT ||
         ( value.m_esp.m_target_group != static_cast< usize >( config::cham_entity_t::world ) &&
           value.m_esp.m_edited >= config::ESP_TARGET_COUNT ) ||
         ( value.m_esp.m_target_group == static_cast< usize >( config::cham_entity_t::world ) &&
           value.m_esp.m_edited >= config::WORLD_COUNT ) ||
         value.m_esp.m_avatars >= 3 || value.m_esp.m_target_style >= config::TARGET_VISUAL_COUNT ||
         value.m_hitmarker.m_edited_event >= config::HITMARKER_EVENT_COUNT ||
         value.m_hitmarker.m_render_mode >= config::HITMARKER_MODE_COUNT ||
         value.m_auto_vacc.m_idle_resist >= 3 ||
         value.m_auto_vacc.m_reactivity >= config::auto_vacc_t::REACTIVITY_COUNT ||
         value.m_pills.m_crit_priority >= 2 ||
         value.m_chams.m_local_model >= config::LOCAL_MODEL_COUNT || value.m_chams.m_enemy_model >= config::ENEMY_MODEL_COUNT ||
         value.m_chams.m_backtrack_records >= config::BACKTRACK_COUNT || value.m_weather.m_type >= config::WEATHER_COUNT ||
         value.m_tracers.m_style >= config::TRACER_STYLE_COUNT || value.m_fog.m_mode >= config::FOG_COUNT ||
         value.m_misc.m_menu_particles >= gui::MENU_PARTICLES_COUNT || value.m_misc.m_auto_strafe_mode >= 2 ||
         value.m_misc.m_projectile_camera_mode >= config::PROJECTILE_CAMERA_COUNT ||
         value.m_misc.m_projectile_camera_hud >= config::PROJECTILE_CAMERA_HUD_COUNT ||
         value.m_rjump.m_priority >= config::RJUMP_PRIORITY_COUNT ||
         value.m_ragebot.m_projectile_path_style >= config::PATH_STYLE_COUNT ||
         value.m_ragebot.m_projectile_path_bounds_style >= config::PATH_BOUNDS_COUNT ||
         value.m_projectile_path.m_style >= config::PATH_STYLE_COUNT ||
         value.m_trajectory.m_style >= config::TRAJECTORY_STYLE_COUNT ||
         value.m_projectile_warning.m_style >= config::PROJECTILE_WARNING_STYLE_COUNT ||
         value.m_radio.m_indicator_style >= config::RADIO_INDICATOR_COUNT ||
         value.m_recorder.m_route_style >= config::PATH_STYLE_COUNT ||
         value.m_skins.m_wearable_slot >= config::SKIN_WEARABLE_SLOTS )
    {
        return false;
    }

    for ( auto &profile : value.m_esp.m_profiles )
    {
        if ( profile.m_health_style >= config::HEALTH_STYLE_COUNT )
        {
            return false;
        }

        clamp_config_vars( profile.m_health_divisions, 0, 10 );

        u32 seen{ };
        for ( u8 element : profile.m_order )
        {
            if ( element >= config::ESP_ELEMENT_COUNT || ( seen & ( 1u << element ) ) )
            {
                return false;
            }

            seen |= 1u << element;
        }
    }

    u16 groups{ };
    for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
    {
        const config::player_group_t &group{ value.m_player_groups[ index ] };
        if ( group.m_name[ 16 ] )
        {
            return false;
        }

        if ( group.m_name[ 0 ] )
        {
            groups = static_cast< u16 >( groups | ( 1u << index ) );
        }
    }

    for ( const auto &entry : value.m_player_tags )
    {
        if ( entry.m_tag > config::PLAYER_TAG_TARGET || entry.m_alias[ 32 ] ||
             ( entry.m_marks & ~config::PLAYER_MARK_ALL ) ||
             ( entry.m_groups & ~groups ) )
        {
            return false;
        }
    }

    return true;
}

bool config::load( const char *name )
{
    atl::vector< char > content{ };
    json::document      document{ };
    if ( !configs::transfer( configs::action::load, name, { }, content ) ||
         !document.parse( string_view{ content.data( ), content.size( ) } ) ||
         document.root( )[ ASTRAL_XORSTR_STATIC( "version" ) ].as< i32 >( ) != 1 )
    {
        return false;
    }

    config candidate{ };
    if ( !read_config_value( document.root( )[ ASTRAL_XORSTR_STATIC( "settings" ) ], candidate ) )
    {
        return false;
    }
    const string_view steam_key{ document.root( )[ ASTRAL_XORSTR_STATIC( "steam_api_key" ) ].as< string_view >( ) };
    const string_view history_key{ document.root( )[ ASTRAL_XORSTR_STATIC( "steam_history_api_key" ) ].as< string_view >( ) };
    const string_view menu_user{ document.root( )[ ASTRAL_XORSTR_STATIC( "menu_user" ) ].as< string_view >( ) };
    const string_view streamer_alias{ document.root( )[ ASTRAL_XORSTR_STATIC( "streamer_local_alias" ) ].as< string_view >( ) };
    if ( steam_key.size( ) >= sizeof( candidate.m_misc.m_steam_api_key ) ||
         history_key.size( ) >= sizeof( candidate.m_misc.m_steam_history_api_key ) ||
         menu_user.size( ) >= sizeof( candidate.m_misc.m_menu_user ) ||
         streamer_alias.size( ) >= sizeof( candidate.m_streamer_mode.m_local_alias ) )
    {
        return false;
    }
    str::copy( candidate.m_misc.m_steam_api_key, sizeof( candidate.m_misc.m_steam_api_key ),
               steam_key.data( ), steam_key.size( ) );
    str::copy( candidate.m_misc.m_steam_history_api_key, sizeof( candidate.m_misc.m_steam_history_api_key ),
               history_key.data( ), history_key.size( ) );
    if ( !menu_user.empty( ) )
    {
        str::copy( candidate.m_misc.m_menu_user, sizeof( candidate.m_misc.m_menu_user ),
                   menu_user.data( ), menu_user.size( ) );
    }
    if ( !streamer_alias.empty( ) )
    {
        str::copy( candidate.m_streamer_mode.m_local_alias,
                   sizeof( candidate.m_streamer_mode.m_local_alias ),
                   streamer_alias.data( ), streamer_alias.size( ) );
    }

    if ( candidate.m_esp.m_target_style == 3 )
    {
        candidate.m_esp.m_target_style = TARGET_VISUAL_BRACKETS;
    }

    if ( !valid_config( candidate ) )
    {
        return false;
    }

    if ( !configs::transfer( configs::action::activate, name, { }, content ) )
    {
        return false;
    }

    *this = candidate;

    const json::value scripts{ document.root( )[ ASTRAL_XORSTR_STATIC( "scripts" ) ] };
    if ( scripts.type( ) == json::type_t::array )
    {
        g_scripts.request_unload_all( );

        for ( usize index{ }; index < scripts.size( ); ++index )
        {
            const string_view   script_name{ scripts[ index ].as< string_view >( ) };
            atl::vector< char > source{ };

            if ( !script::valid_name( script_name ) )
            {
                continue;
            }

            if ( !script::fetch( script_name, source ) )
            {
                g_scripts.log( script::log_kind_t::error, ASTRAL_XORSTR( "could not read the script file" ).get( ) );
                continue;
            }

            g_scripts.request_run( script_name, string_view{ source.data( ), source.size( ) } );
        }
    }

    return true;
}

bool config::save( const char *name )
{
    json::writer writer{ };
    writer.begin_object( );
    writer.member( ASTRAL_XORSTR_STATIC( "version" ), 1 );
    writer.member( ASTRAL_XORSTR_STATIC( "steam_api_key" ), string_view{ this->m_misc.m_steam_api_key } );
    writer.member( ASTRAL_XORSTR_STATIC( "steam_history_api_key" ), string_view{ this->m_misc.m_steam_history_api_key } );
    writer.member( ASTRAL_XORSTR_STATIC( "menu_user" ), string_view{ this->m_misc.m_menu_user } );
    writer.member( ASTRAL_XORSTR_STATIC( "streamer_local_alias" ), string_view{ this->m_streamer_mode.m_local_alias } );
    writer.key( ASTRAL_XORSTR_STATIC( "settings" ) );
    write_config_value( writer, *this );

    script::name_t scripts[ script::SCRIPT_CAPACITY ]{ };
    const usize    script_count{ g_scripts.loaded_names( scripts, script::SCRIPT_CAPACITY ) };

    writer.key( ASTRAL_XORSTR_STATIC( "scripts" ) );
    writer.begin_array( );
    for ( usize index{ }; index < script_count; ++index )
    {
        writer.write( scripts[ index ].view( ) );
    }

    writer.end_array( );

    writer.end_object( );
    atl::vector< char > output{ };
    return writer.ok( ) && configs::transfer( configs::action::save, name, writer.text( ), output );
}

static long                                 s_config_state{ };
static configs::action                      s_config_action{ };
static char                                 s_config_name[ 33 ]{ };
static char                                 s_config_names[ configs::max_configs ][ 33 ]{ };
static const char                          *s_config_labels[ configs::max_configs ]{ };
static usize                                s_config_count{ };
ASTRAL_NO_DESTROY static config              s_config_pending{ };
ASTRAL_NO_DESTROY static atl::vector< char > s_config_list{ };
static bool                                 s_config_success{ };
static bool                                 s_config_listed{ };

static bool read_config_list( )
{
    json::document document{ };
    if ( !document.parse( string_view{ s_config_list.data( ), s_config_list.size( ) } ) )
    {
        return false;
    }

    const json::value items{ document.root( )[ ASTRAL_XORSTR_STATIC( "configs" ) ] };
    if ( items.type( ) != json::type_t::array || items.size( ) > configs::max_configs )
    {
        return false;
    }

    for ( usize i{ }; i < items.size( ); ++i )
    {
        if ( !configs::valid_name( items[ i ][ ASTRAL_XORSTR_STATIC( "name" ) ].as< string_view >( ) ) )
        {
            return false;
        }
    }

    s_config_count = items.size( );
    for ( usize i{ }; i < configs::max_configs; ++i )
    {
        const string_view name{ i < s_config_count ? items[ i ][ ASTRAL_XORSTR_STATIC( "name" ) ].as< string_view >( ) : string_view{ } };
        str::copy( s_config_names[ i ], sizeof( s_config_names[ i ] ), name.data( ), name.size( ) );
        s_config_labels[ i ] = s_config_names[ i ];
    }

    return true;
}

static void ASTRAL_STDCALL config_worker( void * )
{
    atl::vector< char > output{ };
    switch ( s_config_action )
    {
        case configs::action::save: s_config_success = s_config_pending.save( s_config_name ); break;
        case configs::action::load: s_config_success = s_config_pending.load( s_config_name ); break;
        case configs::action::remove: s_config_success = configs::transfer( s_config_action, s_config_name, { }, output ); break;
        default: s_config_success = true; break;
    }

    s_config_listed = configs::transfer( configs::action::list, "", { }, s_config_list );
    _InterlockedExchange( &s_config_state, 2 );
}

static void notify_config_result( )
{
    const notify_kind_t kind{ s_config_success ? notify_kind_t::success : notify_kind_t::error };
    switch ( s_config_action )
    {
        case configs::action::save: push_notification( kind, ICON_SAVE, s_config_success ? ASTRAL_XORSTR_STATIC( "Config saved" ) : ASTRAL_XORSTR_STATIC( "Could not save config" ), s_config_name ); break;
        case configs::action::load: push_notification( kind, ICON_SAVE, s_config_success ? ASTRAL_XORSTR_STATIC( "Config loaded" ) : ASTRAL_XORSTR_STATIC( "Could not load config" ), s_config_name ); break;
        case configs::action::remove: push_notification( kind, ICON_SAVE, s_config_success ? ASTRAL_XORSTR_STATIC( "Config deleted" ) : ASTRAL_XORSTR_STATIC( "Could not delete config" ), s_config_name ); break;
        default: break;
    }

    if ( !s_config_listed )
    {
        push_notification( notify_kind_t::error, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "Could not read the config list" ) );
    } else if ( s_config_action == configs::action::list )
    {
        push_notification( notify_kind_t::information, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "Config list refreshed" ) );
    }
}

void configs::startup( )
{
    for ( usize i{ }; i < max_configs; ++i )
    {
        s_config_labels[ i ] = s_config_names[ i ];
    }

    if ( !transfer( action::list, "", { }, s_config_list ) || !read_config_list( ) )
    {
        push_notification( notify_kind_t::error, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "Could not read the config list" ) );
        return;
    }

    json::document document{ };
    if ( !document.parse( string_view{ s_config_list.data( ), s_config_list.size( ) } ) )
    {
        return;
    }

    // the menu is built before this runs, so the result goes through poll( ) like a worker result
    // and the render thread applies it together with the picker, accent and skin editor refresh
    s_config_action  = action::activate; // nothing to load; poll( ) only publishes the list silently
    s_config_success = false;
    s_config_listed  = true;

    const string_view last{ document.root( )[ ASTRAL_XORSTR_STATIC( "last_used" ) ].as< string_view >( ) };
    if ( valid_name( last ) )
    {
        str::copy( s_config_name, sizeof( s_config_name ), last.data( ), last.size( ) );
        s_config_action  = action::load;
        s_config_success = s_config_pending.load( s_config_name );
    }

    _InterlockedExchange( &s_config_state, 2 );
}

bool configs::request( action operation, string_view name )
{
    if ( operation != action::list && !valid_name( name ) )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "Enter a name or select a config" ) );
        return false;
    }

    if ( _InterlockedCompareExchange( &s_config_state, 1, 0 ) != 0 )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "A config operation is still running" ) );
        return false;
    }

    s_config_action = operation;
    str::copy( s_config_name, sizeof( s_config_name ), name.data( ), name.size( ) );

    s_config_pending = g_cfg;
    s_config_success = false;

    if ( !atl::create_hidden_thread( config_worker, nullptr ) )
    {
        _InterlockedExchange( &s_config_state, 0 );
        push_notification( notify_kind_t::error, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "Could not start the config worker" ) );
        return false;
    }

    return true;
}

bool configs::poll( bool &loaded )
{
    loaded = false;
    if ( _InterlockedCompareExchange( &s_config_state, 3, 2 ) != 2 )
    {
        return false;
    }
    loaded = s_config_success && s_config_action == action::load;
    if ( loaded )
    {
        g_cfg = s_config_pending;
        reset_esp_preview( );
    }
    s_config_listed = s_config_listed && read_config_list( );
    notify_config_result( );
    _InterlockedExchange( &s_config_state, 0 );
    return true;
}

const char *const *configs::names( )
{
    return s_config_labels;
}

bool configs::has_name( usize index )
{
    return index < s_config_count;
}

usize configs::loaded_index( )
{
    for ( usize i{ }; i < s_config_count; ++i )
    {
        if ( str::equals( s_config_names[ i ], s_config_name ) )
        {
            return i;
        }
    }

    return 0;
}

const char *configs::loaded_name( )
{
    return valid_name( s_config_name ) ? s_config_name : nullptr;
}

usize configs::count( )
{
    return s_config_count;
}
