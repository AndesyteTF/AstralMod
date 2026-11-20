#include "astral_sense.hpp"

#include "config.hpp"
#include "entity_cache.hpp"
#include "global.hpp"
#include "gui/indicator.hpp"
#include "gui/gui.hpp"
#include "gui/theme.hpp"
#include "player_card_image.hpp"
#include "render/render.hpp"

namespace
{
    constexpr i32 SMALL_WIDTH{ 150 };
    constexpr i32 STATUS_WIDTH{ 286 };
    constexpr i32 ROW_HEIGHT{ 15 };
    constexpr i32 PAD{ 5 };
    constexpr usize DAMAGE_BUCKETS{ 60 };
    constexpr usize TRAIL_POINTS{ 32 };

    indicator_card_t s_player_card{ }, s_status{ }, s_damage{ }, s_speed{ }, s_health{ }, s_session{ };
    indicator_card_t s_resources{ }, s_buildings{ }, s_combat{ }, s_conditions{ }, s_compass{ };
    indicator_card_t s_objective{ }, s_events{ }, s_alerts{ };
    indicator_card_t s_arsenal{ }, s_telemetry{ }, s_readiness{ };
    indicator_card_t s_history{ }, s_performance{ };
    i32              s_damage_buckets[ DAMAGE_BUCKETS ]{ };
    usize            s_damage_head{ };
    u64              s_damage_second{ };
    i32              s_last_health{ -1 };
    bool             s_was_alive{ };
    i32              s_deaths{ };
    i32              s_peak_speed{ };
    u64              s_started{ };
    i32              s_kills{ }, s_assists{ }, s_streak{ }, s_best_streak{ };

    struct combat_line_t
    {
        char m_name[ 32 ]{ };
        char m_weapon[ 32 ]{ };
        bool m_death{ };
        bool m_headshot{ };
        bool m_backstab{ };
        bool m_crit{ };
    };
    combat_line_t s_feed[ 4 ]{ };
    usize         s_feed_count{ };

    struct event_line_t
    {
        char m_text[ 72 ]{ };
        u64  m_time{ };
        color_t m_color{ MENU_TEXT };
    };
    event_line_t s_event_feed[ 6 ]{ };
    usize        s_event_count{ };
    char         s_map_name[ 64 ]{ "unknown" };
    u64          s_map_started{ };
    i32          s_captures{ };
    i32          s_capture_team{ };
    i32          s_round_winner{ };
    u64          s_round_event_time{ };
    float        s_smoothed_fps{ 60.0f };

    struct history_entry_t
    {
        char m_map[ 32 ]{ };
        i32 m_kills{ }, m_deaths{ }, m_assists{ };
        u64 m_seconds{ };
    };
    history_entry_t s_history_entries[ 5 ]{ };
    usize s_history_count{ };

    struct performance_sample_t { i32 m_fps{ }, m_ping{ }, m_speed{ }; };
    performance_sample_t s_performance_samples[ 60 ]{ };
    usize s_performance_count{ };
    u64 s_last_performance_sample{ };

    struct trail_point_t
    {
        vec3 m_position{ };
        u64  m_time{ };
    };
    trail_point_t s_trail[ TRAIL_POINTS ]{ };
    usize         s_trail_count{ };

    color_t fade( color_t value, u8 alpha )
    {
        value.a = alpha;
        return value;
    }

    color_t health_color( i32 health, i32 maximum )
    {
        if ( maximum > 0 && health > maximum ) return color_t{ 90, 180, 255 };
        const float part{ maximum > 0 ? math::clamp( static_cast< float >( health ) / maximum, 0.0f, 1.0f ) : 0.0f };
        if ( part < 0.5f ) return gui::mix_color( CONSOLE_ERROR, STATUS_WARNING, static_cast< u8 >( part * 510.0f ) );
        return gui::mix_color( STATUS_WARNING, color_t{ 90, 220, 120 }, static_cast< u8 >( ( part - 0.5f ) * 510.0f ) );
    }

    const char *class_name( i32 value );

    i32 max_health( C_TFPlayer *player )
    {
        if ( !player ) return 1;
        if ( g_cl.m_resource ) return math::bigger( g_cl.m_resource->m_iMaxHealth( )[ player->entindex( ) ], 1 );
        return math::bigger( player->m_iHealth( ), 1 );
    }

    void text_pair( i32 left, i32 right, i32 y, const char *label, const char *value,
                    color_t value_color, u8 alpha )
    {
        g_render.text( MENU_FONT_TINY( ), point_t{ left, y }, fade( MENU_TEXT_MUTED, alpha ), label );
        g_render.text( MENU_FONT_TINY( ), point_t{ right, y }, fade( value_color, alpha ), value,
                       false, render::text_align::right );
    }

    void bool_pair( i32 left, i32 right, i32 y, const char *label, bool value, u8 alpha )
    {
        text_pair( left, right, y, label, value ? ASTRAL_XORSTR_STATIC( "ON" ) : ASTRAL_XORSTR_STATIC( "OFF" ),
                   value ? color_t{ 90, 220, 120 } : CONSOLE_ERROR, alpha );
    }

    indicator_frame_t begin_card( indicator_card_t &card, point_t &saved, i32 width, i32 height,
                                  const char *title )
    {
        card.m_at = saved;
        step_indicator( card, 0.0f, g_cl.m_screen_width, g_cl.m_screen_height, width, height, true );
        saved = card.m_at;
        return draw_indicator_frame( card, title );
    }

    void update_state( C_TFPlayer *local, u64 now )
    {
        if ( !s_started ) s_started = now;

        const u64 second{ now / 1000 };
        if ( !s_damage_second ) s_damage_second = second;
        usize advance{ static_cast< usize >( math::smaller( second - s_damage_second,
                                                            static_cast< u64 >( DAMAGE_BUCKETS ) ) ) };
        while ( advance-- )
        {
            s_damage_head = ( s_damage_head + 1 ) % DAMAGE_BUCKETS;
            s_damage_buckets[ s_damage_head ] = 0;
        }
        s_damage_second = second;

        if ( !local )
        {
            s_last_health = -1;
            s_was_alive   = false;
            s_trail_count = 0;
            return;
        }

        const bool alive{ local->IsAlive( ) };
        if ( s_was_alive && !alive ) ++s_deaths;
        s_was_alive = alive;

        const i32 health{ local->m_iHealth( ) };
        if ( alive && s_last_health >= 0 && health < s_last_health )
            s_damage_buckets[ s_damage_head ] += s_last_health - health;
        s_last_health = alive ? health : -1;

        if ( !alive ) return;
        const i32 speed{ static_cast< i32 >( local->m_vecVelocity( ).length2d( ) + 0.5f ) };
        s_peak_speed = math::bigger( s_peak_speed, speed );

        if ( !g_cfg.m_misc.m_astral_sense_trail )
        {
            s_trail_count = 0;
            return;
        }

        const vec3 origin{ local->GetAbsOrigin( ) };
        if ( !s_trail_count || s_trail[ s_trail_count - 1 ].m_position.dist_to( origin ) >= 16.0f )
        {
            if ( s_trail_count == TRAIL_POINTS )
            {
                for ( usize index{ 1 }; index < TRAIL_POINTS; ++index ) s_trail[ index - 1 ] = s_trail[ index ];
                --s_trail_count;
            }
            s_trail[ s_trail_count++ ] = trail_point_t{ origin, now };
        }
        while ( s_trail_count && now - s_trail[ 0 ].m_time > 1500 )
        {
            for ( usize index{ 1 }; index < s_trail_count; ++index ) s_trail[ index - 1 ] = s_trail[ index ];
            --s_trail_count;
        }
    }

    void draw_world( C_TFPlayer *local, u64 now )
    {
        if ( g_cfg.m_misc.m_astral_sense_trail && s_trail_count > 1 )
        {
            for ( usize index{ 1 }; index < s_trail_count; ++index )
            {
                vec2 from{ }, to{ };
                if ( g_cl.world_to_screen( s_trail[ index - 1 ].m_position, from ) &&
                     g_cl.world_to_screen( s_trail[ index ].m_position, to ) )
                {
                    const u64 age{ now - s_trail[ index ].m_time };
                    const u8 alpha{ static_cast< u8 >( 255 - math::smaller( age * 255 / 1500, 255ull ) ) };
                    g_render.line( from, to, fade( g_theme.m_accent, alpha ), 2.0f );
                }
            }
        }

        if ( !g_cfg.m_misc.m_astral_sense_diamond || !local || !local->IsAlive( ) ) return;
        const float phase{ static_cast< float >( now % 100000 ) * 0.0012f };
        const vec3 centre{ local->GetAbsOrigin( ) + vec3{ 0.0f, 0.0f, 92.0f + math::sin( phase * 1.7f ) * 2.0f } };
        vec2 top{ }, bottom{ }, ring[ 4 ]{ };
        if ( !g_cl.world_to_screen( centre + vec3{ 0.0f, 0.0f, 7.0f }, top ) ||
             !g_cl.world_to_screen( centre - vec3{ 0.0f, 0.0f, 11.0f }, bottom ) ) return;
        for ( usize index{ }; index < 4; ++index )
        {
            const float angle{ phase + static_cast< float >( index ) * PI * 0.5f };
            if ( !g_cl.world_to_screen( centre + vec3{ math::cos( angle ) * 5.0f, math::sin( angle ) * 5.0f, 0.0f }, ring[ index ] ) ) return;
        }
        for ( usize index{ }; index < 4; ++index )
        {
            const usize next{ ( index + 1 ) % 4 };
            g_render.line( ring[ index ], ring[ next ], g_theme.m_accent, 2.0f );
            g_render.line( top, ring[ index ], g_theme.m_accent, 2.0f );
            g_render.line( bottom, ring[ index ], fade( g_theme.m_accent_end, 190 ), 2.0f );
        }
    }

    void draw_status( )
    {
        struct status_row_t { const char *m_label; char m_value[ 32 ]; color_t m_color; } rows[ 16 ]{ };
        usize count{ };
        const auto add = [&]( usize field, const char *label, const char *value, color_t color = MENU_TEXT )
        {
            if ( !g_cfg.m_misc.m_astral_sense_status_fields[ field ] || count >= 16 ) return;
            rows[ count ].m_label = label;
            str::copy( rows[ count ].m_value, sizeof( rows[ count ].m_value ), value );
            rows[ count++ ].m_color = color;
        };
        const auto add_bool = [&]( usize field, const char *label, bool value )
        {
            add( field, label, value ? ASTRAL_XORSTR_STATIC( "ON" ) : ASTRAL_XORSTR_STATIC( "OFF" ),
                 value ? color_t{ 90, 220, 120 } : MENU_TEXT_DIM );
        };
        char value[ 32 ]{ };
        C_TFPlayer *local{ g_cl.m_local };
        C_TFWeaponBase *weapon{ g_cl.m_weapon };
        if ( local )
        {
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{} / {}" ).get( ), local->m_iHealth( ), max_health( local ) );
            add( 0, ASTRAL_XORSTR_STATIC( "Health" ), value, health_color( local->m_iHealth( ), max_health( local ) ) );
        }
        if ( weapon ) { str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), weapon->m_iClip1( ) ); add( 1, ASTRAL_XORSTR_STATIC( "Clip" ), value ); }
        add_bool( 2, ASTRAL_XORSTR_STATIC( "Aimbot" ), g_cfg.m_ragebot.m_enabled );
        add_bool( 3, ASTRAL_XORSTR_STATIC( "Melee aim" ), g_cfg.m_melee.m_enabled );
        add_bool( 4, ASTRAL_XORSTR_STATIC( "Triggerbot" ), g_cfg.m_triggerbot.m_enabled );
        static const char *const AIM_TYPES[]{ ASTRAL_XORSTR_STATIC( "Plain" ), ASTRAL_XORSTR_STATIC( "Silent" ),
                                              ASTRAL_XORSTR_STATIC( "Locking" ), ASTRAL_XORSTR_STATIC( "Humanized" ) };
        add( 5, ASTRAL_XORSTR_STATIC( "Aim type" ), g_cfg.m_ragebot.m_aim_type < 4 ? AIM_TYPES[ g_cfg.m_ragebot.m_aim_type ] : ASTRAL_XORSTR_STATIC( "?" ) );
        add_bool( 6, ASTRAL_XORSTR_STATIC( "Thirdperson" ), g_cfg.m_misc.m_thirdperson );
        add_bool( 7, ASTRAL_XORSTR_STATIC( "Bunnyhop" ), g_cfg.m_misc.m_bhop );
        add_bool( 8, ASTRAL_XORSTR_STATIC( "Anti-aim" ), g_cfg.m_anti_hack.m_anti_aim_enabled );
        add_bool( 9, ASTRAL_XORSTR_STATIC( "Seed sync" ), g_cfg.m_exploits.m_seed_prediction );
        add_bool( 10, ASTRAL_XORSTR_STATIC( "Anti-bait" ), g_cfg.m_ragebot.m_prevent_bait );
        add_bool( 11, ASTRAL_XORSTR_STATIC( "Backtrack" ), g_cfg.m_ragebot.m_hitscan_profiles[ 0 ].m_use_history );
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{} deg" ).get( ), g_cfg.m_ragebot.m_fov ); add( 12, ASTRAL_XORSTR_STATIC( "Aim FOV" ), value );
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), g_cfg.m_misc.m_fov ); add( 13, ASTRAL_XORSTR_STATIC( "Weapon FOV" ), value );
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), s_deaths ); add( 14, ASTRAL_XORSTR_STATIC( "Deaths" ), value );
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 0 };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{} ms" ).get( ), ping );
        add( 15, ASTRAL_XORSTR_STATIC( "Ping" ), value, ping < 80 ? color_t{ 90, 220, 120 } : ( ping < 150 ? STATUS_WARNING : CONSOLE_ERROR ) );

        const usize row_count{ math::bigger< usize >( ( count + 1 ) / 2, 1 ) };
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * static_cast< i32 >( row_count + 1 ) };
        const indicator_frame_t frame{ begin_card( s_status, g_cfg.m_misc.m_astral_sense_status_position,
                                                   STATUS_WIDTH, height, ASTRAL_XORSTR_STATIC( "status" ) ) };
        const i32 middle{ ( frame.m_content.left + frame.m_content.right ) / 2 };
        const i32 left{ frame.m_content.left }, right{ frame.m_content.right };
        i32 y{ frame.m_content.top + PAD };
        g_render.text( MENU_FONT( ), point_t{ left, y }, fade( MENU_TEXT, frame.m_enter ),
                       g_cfg.m_misc.m_menu_user[ 0 ] ? g_cfg.m_misc.m_menu_user : ASTRAL_XORSTR_STATIC( "User" ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ right, y }, fade( g_theme.m_accent, frame.m_enter ),
                       local ? class_name( local->m_iClass( ) ) : ASTRAL_XORSTR_STATIC( "No class" ), false, render::text_align::right );
        y += ROW_HEIGHT;
        for ( usize index{ }; index < count; ++index )
        {
            const bool right_column{ index >= row_count };
            const usize row{ right_column ? index - row_count : index };
            const i32 x0{ right_column ? middle + 8 : left };
            const i32 x1{ right_column ? right : middle - 8 };
            text_pair( x0, x1, y + static_cast< i32 >( row ) * ROW_HEIGHT, rows[ index ].m_label,
                       rows[ index ].m_value, rows[ index ].m_color, frame.m_enter );
        }
    }

    void draw_player_card( C_TFPlayer *local )
    {
        constexpr i32 WIDTH{ 300 };
        constexpr i32 HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + 62 };
        const indicator_frame_t frame{ begin_card( s_player_card,
            g_cfg.m_misc.m_astral_sense_player_card_position, WIDTH, HEIGHT,
            ASTRAL_XORSTR_STATIC( "player" ) ) };
        const i32 image_size{ 46 };
        const rect_t image{ frame.m_content.left, frame.m_content.top + 7,
                            frame.m_content.left + image_size, frame.m_content.top + 7 + image_size };
        const usize selected{ g_cfg.m_misc.m_astral_sense_player_card_image };
        const bool custom{ g_cfg.m_misc.m_astral_sense_player_card_custom_image &&
                           selected < g_player_card_image.count( ) };
        if ( custom ) g_player_card_image.paint( image, frame.m_enter, selected );
        if ( !custom || !g_player_card_image.ready( selected ) )
        {
            if ( g_gui.profile_avatar( ) )
                g_render.icon( 0, render::icons::KIND_PROFILE, point_t{ image.left, image.top },
                               fade( color_t{ 255, 255, 255 }, frame.m_enter ), 0.0f, false, image_size );
            else
                g_render.text( MENU_FONT( ), point_t{ image.left, image.top },
                               fade( color_t{ 255, 255, 255 }, frame.m_enter ), ASTRAL_XORSTR_STATIC( " " ),
                               false, render::text_align::left, 0.0f, g_gui.avatar_account( ),
                               render::AVATAR_CIRCLE, image_size );
        }
        g_render.circle( point_t{ ( image.left + image.right ) / 2, ( image.top + image.bottom ) / 2 },
                         fade( g_theme.m_accent, frame.m_enter ), image_size * 0.5f, 1.0f );

        const i32 left{ image.right + 10 };
        const i32 right{ frame.m_content.right };
        const char *user{ g_cfg.m_misc.m_menu_user[ 0 ] ? g_cfg.m_misc.m_menu_user : ASTRAL_XORSTR_STATIC( "User" ) };
        g_render.text( MENU_FONT( ), point_t{ left, frame.m_content.top + 7 },
                       fade( MENU_TEXT, frame.m_enter ), user );
        char health[ 48 ]{ };
        const i32 maximum{ local ? max_health( local ) : 0 };
        const i32 current{ local ? math::bigger( local->m_iHealth( ), 0 ) : 0 };
        if ( local )
            str::format_to( health, sizeof( health ), ASTRAL_XORSTR( "{}  {} / {}" ).get( ),
                            class_name( local->m_iClass( ) ), current, maximum );
        else
            str::copy( health, sizeof( health ), ASTRAL_XORSTR_STATIC( "Not in game" ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ left, frame.m_content.top + 25 },
                       fade( MENU_TEXT_MUTED, frame.m_enter ), health );
        const rect_t bar{ left, frame.m_content.top + 43, right, frame.m_content.top + 49 };
        g_render.box( bar, fade( CONTROL_BACKGROUND, frame.m_enter ), true );
        const float fill{ maximum > 0
            ? math::clamp( static_cast< float >( current ) / maximum, 0.0f, 1.0f )
            : 0.0f };
        const i32 fill_width{ static_cast< i32 >( fill * ( bar.right - bar.left ) ) };
        if ( fill_width > 0 )
            g_render.gradient( rect_t{ bar.left, bar.top, bar.left + fill_width, bar.bottom },
                               fade( g_theme.m_accent, frame.m_enter ),
                               fade( g_theme.m_accent_end, frame.m_enter ), false );
    }

    void draw_small_card( indicator_card_t &card, point_t &position, const char *title,
                          const char *label, const char *value, float fill, color_t color )
    {
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + 31 };
        const indicator_frame_t frame{ begin_card( card, position, SMALL_WIDTH, height, title ) };
        const i32 y{ frame.m_content.top + PAD };
        text_pair( frame.m_content.left, frame.m_content.right, y, label, value, color, frame.m_enter );
        const rect_t bar{ frame.m_content.left, y + ROW_HEIGHT + 2, frame.m_content.right, y + ROW_HEIGHT + 6 };
        g_render.box( bar, fade( CONTROL_BACKGROUND, frame.m_enter ), true );
        const i32 width{ static_cast< i32 >( math::clamp( fill, 0.0f, 1.0f ) * ( bar.right - bar.left ) ) };
        if ( width > 0 ) g_render.box( rect_t{ bar.left, bar.top, bar.left + width, bar.bottom }, fade( color, frame.m_enter ), true );
    }

    void draw_resources( C_TFPlayer *local )
    {
        constexpr i32 HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * 5 };
        const indicator_frame_t frame{ begin_card( s_resources, g_cfg.m_misc.m_astral_sense_resources_position,
                                                   180, HEIGHT, ASTRAL_XORSTR_STATIC( "class resources" ) ) };
        const i32 left{ frame.m_content.left }, right{ frame.m_content.right };
        i32 y{ frame.m_content.top + PAD };
        char value[ 48 ]{ };

        if ( !local || !local->IsAlive( ) )
        {
            text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "state" ), ASTRAL_XORSTR_STATIC( "not alive" ), MENU_TEXT_DIM, frame.m_enter );
            return;
        }

        C_TFWeaponBase *weapon{ g_cl.m_weapon };
        const i32 clip{ weapon ? weapon->m_iClip1( ) : -1 };
        const i32 ammo_type{ weapon ? weapon->m_iPrimaryAmmoType( ) : -1 };
        const i32 reserve{ ammo_type >= 0 ? ( &local->m_iAmmo( ) )[ ammo_type ] : -1 };
        str::format_to( value, sizeof( value ), clip >= 0 ? ASTRAL_XORSTR( "{} / {}" ).get( ) : ASTRAL_XORSTR( "{}" ).get( ),
                        clip >= 0 ? clip : reserve, reserve );
        text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "ammunition" ), value, MENU_TEXT, frame.m_enter );
        y += ROW_HEIGHT;

        const i32 player_class{ local->m_iClass( ) };
        if ( player_class == TF_CLASS_MEDIC )
        {
            C_TFWeaponBase *medigun{ local->Weapon_OwnsThisID( TF_WEAPON_MEDIGUN ) };
            const i32 charge{ medigun ? static_cast< i32 >( medigun->As< C_WeaponMedigun * >( )->m_flChargeLevel( ) * 100.0f + 0.5f ) : 0 };
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}%" ).get( ), charge );
            text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "ubercharge" ), value, charge >= 100 ? color_t{ 90, 220, 120 } : g_theme.m_accent, frame.m_enter );
        } else if ( player_class == TF_CLASS_ENGINEER )
        {
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), ( &local->m_iAmmo( ) )[ 3 ] );
            text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "metal" ), value, g_theme.m_accent, frame.m_enter );
        } else if ( player_class == TF_CLASS_SPY )
        {
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}%" ).get( ), static_cast< i32 >( local->m_flCloakMeter( ) + 0.5f ) );
            text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "cloak" ), value, g_theme.m_accent, frame.m_enter );
        } else if ( player_class == TF_CLASS_DEMOMAN )
        {
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}%" ).get( ), static_cast< i32 >( local->m_flChargeMeter( ) + 0.5f ) );
            text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "charge" ), value, g_theme.m_accent, frame.m_enter );
        } else
        {
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}%" ).get( ), static_cast< i32 >( local->m_flRageMeter( ) + 0.5f ) );
            text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "rage" ), value, g_theme.m_accent, frame.m_enter );
        }
        y += ROW_HEIGHT;

        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), local->m_iRevengeCrits( ) );
        text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "revenge crits" ), value, STATUS_WARNING, frame.m_enter );
        y += ROW_HEIGHT;
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), local->m_iDecapitations( ) );
        text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "heads" ), value, STATUS_WARNING, frame.m_enter );
        y += ROW_HEIGHT;
        const i32 healers{ local->GetSharedPlayerData( ) ? local->GetSharedPlayerData( )->m_nNumHealers( ) : 0 };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), healers );
        text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "healers" ), value, healers ? color_t{ 90, 220, 120 } : MENU_TEXT_DIM, frame.m_enter );
    }

    void draw_buildings( C_TFPlayer *local )
    {
        struct row_t { C_BaseObject *m_object; ETFClassID m_class; } rows[ 8 ]{ };
        usize count{ };
        if ( local )
        {
            for ( const auto *cached : g_entity_cache.buildings( ) )
            {
                C_BaseObject *object{ cached->m_entity->As< C_BaseObject * >( ) };
                if ( !object || object->IsDormant( ) || object->m_hBuilder( ).Get( ) != local || object->m_iHealth( ) <= 0 ) continue;
                if ( count < 8 ) rows[ count++ ] = row_t{ object, cached->m_class_id };
            }
        }
        const usize shown{ math::bigger< usize >( count, 1 ) };
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + static_cast< i32 >( shown ) * ROW_HEIGHT };
        const indicator_frame_t frame{ begin_card( s_buildings, g_cfg.m_misc.m_astral_sense_buildings_position,
                                                   220, height, ASTRAL_XORSTR_STATIC( "buildings" ) ) };
        i32 y{ frame.m_content.top + PAD };
        if ( !count )
        {
            text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "owned" ), ASTRAL_XORSTR_STATIC( "none" ), MENU_TEXT_DIM, frame.m_enter );
            return;
        }
        for ( usize index{ }; index < count; ++index )
        {
            C_BaseObject *object{ rows[ index ].m_object };
            const char *name{ rows[ index ].m_class == ETFClassID::CObjectSentrygun ? ASTRAL_XORSTR_STATIC( "sentry" ) :
                              rows[ index ].m_class == ETFClassID::CObjectDispenser ? ASTRAL_XORSTR_STATIC( "dispenser" ) :
                              object->m_iObjectMode( ) ? ASTRAL_XORSTR_STATIC( "teleporter out" ) : ASTRAL_XORSTR_STATIC( "teleporter in" ) };
            char value[ 64 ]{ };
            if ( rows[ index ].m_class == ETFClassID::CObjectSentrygun )
            {
                auto *sentry{ object->As< C_ObjectSentrygun * >( ) };
                str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "L{}  {}hp  {}/{}" ).get( ), object->m_iUpgradeLevel( ),
                                object->m_iHealth( ), sentry->m_iAmmoShells( ), sentry->m_iAmmoRockets( ) );
            } else
            {
                if ( object->m_bHasSapper( ) )
                    str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "SAPPED  {}hp" ).get( ), object->m_iHealth( ) );
                else
                    str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "L{}  {}hp" ).get( ),
                                    object->m_iUpgradeLevel( ), object->m_iHealth( ) );
            }
            text_pair( frame.m_content.left, frame.m_content.right, y, name, value,
                       object->m_bHasSapper( ) ? CONSOLE_ERROR : health_color( object->m_iHealth( ), object->m_iMaxHealth( ) ), frame.m_enter );
            y += ROW_HEIGHT;
        }
    }

    void draw_combat( )
    {
        constexpr i32 WIDTH{ 250 };
        constexpr i32 HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * 6 };
        const indicator_frame_t frame{ begin_card( s_combat, g_cfg.m_misc.m_astral_sense_combat_position,
                                                   WIDTH, HEIGHT, ASTRAL_XORSTR_STATIC( "combat" ) ) };
        const i32 left{ frame.m_content.left }, right{ frame.m_content.right };
        i32 y{ frame.m_content.top + PAD };
        char stats[ 64 ]{ };
        str::format_to( stats, sizeof( stats ), ASTRAL_XORSTR( "{} K   {} D   {} A" ).get( ), s_kills, s_deaths, s_assists );
        text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "score" ), stats, MENU_TEXT, frame.m_enter );
        y += ROW_HEIGHT;
        str::format_to( stats, sizeof( stats ), ASTRAL_XORSTR( "{} current   {} best" ).get( ), s_streak, s_best_streak );
        text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "streak" ), stats,
                   s_streak >= 3 ? STATUS_WARNING : g_theme.m_accent, frame.m_enter );
        y += ROW_HEIGHT;

        for ( usize index{ }; index < 4; ++index )
        {
            if ( index >= s_feed_count )
            {
                if ( !index ) text_pair( left, right, y, ASTRAL_XORSTR_STATIC( "feed" ), ASTRAL_XORSTR_STATIC( "no engagements" ), MENU_TEXT_DIM, frame.m_enter );
                y += ROW_HEIGHT;
                continue;
            }
            const combat_line_t &line{ s_feed[ index ] };
            char detail[ 80 ]{ };
            const char *tag{ line.m_backstab ? ASTRAL_XORSTR_STATIC( "BACKSTAB" ) :
                             line.m_headshot ? ASTRAL_XORSTR_STATIC( "HEADSHOT" ) :
                             line.m_crit ? ASTRAL_XORSTR_STATIC( "CRIT" ) : line.m_weapon };
            str::format_to( detail, sizeof( detail ), tag[ 0 ] ? ASTRAL_XORSTR( "{} [{}]" ).get( ) : ASTRAL_XORSTR( "{}" ).get( ),
                            line.m_name, tag );
            text_pair( left, right, y, line.m_death ? ASTRAL_XORSTR_STATIC( "downed by" ) : ASTRAL_XORSTR_STATIC( "eliminated" ),
                       detail, line.m_death ? CONSOLE_ERROR : color_t{ 90, 220, 120 }, frame.m_enter );
            y += ROW_HEIGHT;
        }
    }

    void draw_conditions( C_TFPlayer *local )
    {
        const char *active[ 14 ]{ };
        usize count{ };
        auto add{ [ & ]( bool on, const char *name ) { if ( on ) active[ count++ ] = name; } };
        if ( local && local->IsAlive( ) )
        {
            add( local->IsBurning( ), ASTRAL_XORSTR_STATIC( "BURNING" ) );
            add( local->IsBleeding( ), ASTRAL_XORSTR_STATIC( "BLEEDING" ) );
            add( local->IsJarated( ), ASTRAL_XORSTR_STATIC( "JARATE" ) );
            add( local->IsMilked( ), ASTRAL_XORSTR_STATIC( "MILK" ) );
            add( local->IsMarkedForDeath( ), ASTRAL_XORSTR_STATIC( "MARKED" ) );
            add( local->IsCritBoosted( ), ASTRAL_XORSTR_STATIC( "CRITICAL" ) );
            add( local->IsUbered( ) || local->IsQuickFixUbered( ), ASTRAL_XORSTR_STATIC( "INVULNERABLE" ) );
            add( local->IsCloaked( ), ASTRAL_XORSTR_STATIC( "CLOAKED" ) );
            add( local->IsDisguised( ), ASTRAL_XORSTR_STATIC( "DISGUISED" ) );
            add( local->IsZoomed( ), ASTRAL_XORSTR_STATIC( "ZOOMED" ) );
            add( local->IsCharging( ), ASTRAL_XORSTR_STATIC( "CHARGING" ) );
            add( local->IsStunned( ), ASTRAL_XORSTR_STATIC( "STUNNED" ) );
            add( local->IsTaunting( ), ASTRAL_XORSTR_STATIC( "TAUNTING" ) );
            add( local->IsBonked( ), ASTRAL_XORSTR_STATIC( "PHASE" ) );
        }
        const usize rows{ math::bigger< usize >( count, 1 ) };
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + static_cast< i32 >( rows ) * ROW_HEIGHT };
        const indicator_frame_t frame{ begin_card( s_conditions, g_cfg.m_misc.m_astral_sense_conditions_position,
                                                   170, height, ASTRAL_XORSTR_STATIC( "conditions" ) ) };
        i32 y{ frame.m_content.top + PAD };
        if ( !count )
        {
            text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "active" ), ASTRAL_XORSTR_STATIC( "none" ), MENU_TEXT_DIM, frame.m_enter );
            return;
        }
        for ( usize index{ }; index < count; ++index )
        {
            const color_t color{ str::equals( active[ index ], ASTRAL_XORSTR_STATIC( "BURNING" ) ) ||
                                 str::equals( active[ index ], ASTRAL_XORSTR_STATIC( "BLEEDING" ) ) ||
                                 str::equals( active[ index ], ASTRAL_XORSTR_STATIC( "MARKED" ) ) ? CONSOLE_ERROR : g_theme.m_accent };
            g_render.text( MENU_FONT_TINY( ), point_t{ frame.m_content.left, y }, fade( color, frame.m_enter ), active[ index ] );
            y += ROW_HEIGHT;
        }
    }

    void draw_compass( C_TFPlayer *local )
    {
        constexpr i32 WIDTH{ 150 }, HEIGHT{ 120 };
        const indicator_frame_t frame{ begin_card( s_compass, g_cfg.m_misc.m_astral_sense_compass_position,
                                                   WIDTH, HEIGHT, ASTRAL_XORSTR_STATIC( "velocity compass" ) ) };
        const point_t centre{ ( frame.m_content.left + frame.m_content.right ) / 2,
                              frame.m_content.top + 38 };
        const i32 radius{ 27 };
        g_render.circle( centre, fade( CONTROL_BORDER, frame.m_enter ), static_cast< float >( radius ), 1.0f );
        g_render.line( point_t{ centre.x, centre.y - radius }, point_t{ centre.x, centre.y - radius + 6 },
                       fade( MENU_TEXT, frame.m_enter ), 2.0f );

        i32 speed{ };
        if ( local && local->IsAlive( ) )
        {
            const vec3 velocity{ local->m_vecVelocity( ) };
            speed = static_cast< i32 >( velocity.length2d( ) + 0.5f );
            if ( speed > 5 )
            {
                vec3 view{ };
                g_tf2.m_engine->GetViewAngles( view );
                const float movement{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
                const float relative{ DEG2RAD( movement - view.y - 90.0f ) };
                const point_t tip{ centre.x + static_cast< i32 >( math::cos( relative ) * ( radius - 4 ) ),
                                   centre.y + static_cast< i32 >( math::sin( relative ) * ( radius - 4 ) ) };
                g_render.line( centre, tip, fade( g_theme.m_accent, frame.m_enter ), 2.5f );
                g_render.circle( tip, fade( g_theme.m_accent_end, frame.m_enter ), 2.5f, 2.0f );
            }
        }
        char value[ 32 ]{ };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{} u/s" ).get( ), speed );
        g_render.text( MENU_FONT_TINY( ), point_t{ centre.x, centre.y + radius + 8 }, fade( MENU_TEXT, frame.m_enter ),
                       value, false, render::text_align::center );
        g_render.text( MENU_FONT_TINY( ), point_t{ centre.x, centre.y + radius + 21 }, fade( MENU_TEXT_DIM, frame.m_enter ),
                       local && local->IsOnGround( ) ? ASTRAL_XORSTR_STATIC( "ground" ) : ASTRAL_XORSTR_STATIC( "air" ),
                       false, render::text_align::center );
    }

    void push_event( const char *text, color_t color )
    {
        for ( usize index{ math::smaller< usize >( s_event_count, 5 ) }; index > 0; --index )
            s_event_feed[ index ] = s_event_feed[ index - 1 ];
        s_event_count = math::smaller< usize >( s_event_count + 1, 6 );
        s_event_feed[ 0 ] = event_line_t{ };
        str::copy( s_event_feed[ 0 ].m_text, sizeof( s_event_feed[ 0 ].m_text ), text );
        s_event_feed[ 0 ].m_time = gui::clock( );
        s_event_feed[ 0 ].m_color = color;
    }

    void draw_objective( C_TFPlayer *local, u64 now )
    {
        constexpr i32 WIDTH{ 220 }, HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * 4 };
        const indicator_frame_t frame{ begin_card( s_objective, g_cfg.m_misc.m_astral_sense_objective_position,
                                                   WIDTH, HEIGHT, ASTRAL_XORSTR_STATIC( "objective" ) ) };
        i32 y{ frame.m_content.top + PAD };
        char value[ 64 ]{ };
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "map" ), s_map_name, g_theme.m_accent, frame.m_enter );
        y += ROW_HEIGHT;
        const u64 elapsed{ s_map_started ? ( now - s_map_started ) / 1000 : 0 };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}:{:02}" ).get( ), elapsed / 60, elapsed % 60 );
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "map time" ), value, MENU_TEXT, frame.m_enter );
        y += ROW_HEIGHT;
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), s_captures );
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "captures" ), value,
                   s_capture_team == 2 ? color_t{ 255, 92, 92 } : s_capture_team == 3 ? color_t{ 83, 174, 255 } : MENU_TEXT,
                   frame.m_enter );
        y += ROW_HEIGHT;
        const bool recent{ s_round_event_time && now - s_round_event_time < 10000 };
        const bool won{ recent && local && s_round_winner == local->m_iTeamNum( ) };
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "status" ),
                   recent ? ( won ? ASTRAL_XORSTR_STATIC( "VICTORY" ) : ASTRAL_XORSTR_STATIC( "DEFEAT" ) ) :
                            ( g_cl.m_is_running ? ASTRAL_XORSTR_STATIC( "LIVE" ) : ASTRAL_XORSTR_STATIC( "STANDBY" ) ),
                   recent ? ( won ? color_t{ 90, 220, 120 } : CONSOLE_ERROR ) : g_theme.m_accent, frame.m_enter );
    }

    void draw_event_stream( u64 now )
    {
        const usize rows{ math::bigger< usize >( s_event_count, 1 ) };
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + static_cast< i32 >( rows ) * ROW_HEIGHT };
        const indicator_frame_t frame{ begin_card( s_events, g_cfg.m_misc.m_astral_sense_events_position,
                                                   270, height, ASTRAL_XORSTR_STATIC( "event stream" ) ) };
        i32 y{ frame.m_content.top + PAD };
        if ( !s_event_count )
        {
            text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "status" ),
                       ASTRAL_XORSTR_STATIC( "listening" ), MENU_TEXT_DIM, frame.m_enter );
            return;
        }
        for ( usize index{ }; index < s_event_count; ++index )
        {
            char age[ 20 ]{ };
            str::format_to( age, sizeof( age ), ASTRAL_XORSTR( "{}s" ).get( ), ( now - s_event_feed[ index ].m_time ) / 1000 );
            text_pair( frame.m_content.left, frame.m_content.right, y, s_event_feed[ index ].m_text, age,
                       s_event_feed[ index ].m_color, frame.m_enter );
            y += ROW_HEIGHT;
        }
    }

    void draw_alerts( C_TFPlayer *local )
    {
        struct alert_t { const char *m_label; const char *m_value; color_t m_color; } alerts[ 10 ]{ };
        usize count{ };
        char hp_value[ 24 ]{ }, ammo_value[ 24 ]{ }, ping_value[ 24 ]{ }, speed_value[ 24 ]{ }, heal_value[ 24 ]{ }, streak_value[ 24 ]{ }, cloak_value[ 24 ]{ };
        if ( local && local->IsAlive( ) )
        {
            const i32 hp{ local->m_iHealth( ) }, maximum{ max_health( local ) };
            if ( hp * 100 <= maximum * 35 )
            {
                str::format_to( hp_value, sizeof( hp_value ), ASTRAL_XORSTR( "{} HP" ).get( ), hp );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "LOW HEALTH" ), hp_value, CONSOLE_ERROR };
            } else if ( hp > maximum )
            {
                str::format_to( hp_value, sizeof( hp_value ), ASTRAL_XORSTR( "{}%" ).get( ), hp * 100 / maximum );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "OVERHEALED" ), hp_value, color_t{ 90, 180, 255 } };
            }
            if ( g_cl.m_weapon && g_cl.m_weapon->m_iClip1( ) >= 0 && g_cl.m_weapon->m_iClip1( ) <= 1 )
            {
                str::format_to( ammo_value, sizeof( ammo_value ), ASTRAL_XORSTR( "{} LEFT" ).get( ), g_cl.m_weapon->m_iClip1( ) );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "LOW AMMO" ), ammo_value, STATUS_WARNING };
            }
            const i32 speed{ static_cast< i32 >( local->m_vecVelocity( ).length2d( ) + 0.5f ) };
            if ( !local->IsOnGround( ) )
            {
                str::format_to( speed_value, sizeof( speed_value ), ASTRAL_XORSTR( "{} u/s" ).get( ), speed );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "AIRBORNE" ), speed_value, g_theme.m_accent };
            }
            const i32 healers{ local->GetSharedPlayerData( ) ? local->GetSharedPlayerData( )->m_nNumHealers( ) : 0 };
            if ( healers > 0 )
            {
                str::format_to( heal_value, sizeof( heal_value ), ASTRAL_XORSTR( "{}" ).get( ), healers );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "HEALING" ), heal_value, color_t{ 90, 220, 120 } };
            }
            if ( s_streak >= 3 )
            {
                str::format_to( streak_value, sizeof( streak_value ), ASTRAL_XORSTR( "{}x" ).get( ), s_streak );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "KILLSTREAK" ), streak_value, STATUS_WARNING };
            }
            if ( local->IsCritBoosted( ) ) alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "CRIT BOOST" ), ASTRAL_XORSTR_STATIC( "ACTIVE" ), STATUS_WARNING };
            if ( local->IsMarkedForDeath( ) ) alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "MARKED" ), ASTRAL_XORSTR_STATIC( "FOR DEATH" ), CONSOLE_ERROR };
            if ( local->IsCloaked( ) )
            {
                str::format_to( cloak_value, sizeof( cloak_value ), ASTRAL_XORSTR( "{}%" ).get( ), static_cast< i32 >( local->m_flCloakMeter( ) + 0.5f ) );
                alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "CLOAKED" ), cloak_value, g_theme.m_accent };
            }
        }
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 0 };
        if ( ping >= 90 && count < 10 )
        {
            str::format_to( ping_value, sizeof( ping_value ), ASTRAL_XORSTR( "{} ms" ).get( ), ping );
            alerts[ count++ ] = alert_t{ ASTRAL_XORSTR_STATIC( "BAD CONNECTION" ), ping_value, CONSOLE_ERROR };
        }

        const usize rows{ math::bigger< usize >( count, 1 ) };
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + static_cast< i32 >( rows ) * ROW_HEIGHT };
        const indicator_frame_t frame{ begin_card( s_alerts, g_cfg.m_misc.m_astral_sense_alerts_position,
                                                   190, height, ASTRAL_XORSTR_STATIC( "alerts" ) ) };
        i32 y{ frame.m_content.top + PAD };
        if ( !count )
        {
            text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "status" ), ASTRAL_XORSTR_STATIC( "clear" ),
                       color_t{ 90, 220, 120 }, frame.m_enter );
            return;
        }
        for ( usize index{ }; index < count; ++index )
        {
            text_pair( frame.m_content.left, frame.m_content.right, y, alerts[ index ].m_label,
                       alerts[ index ].m_value, alerts[ index ].m_color, frame.m_enter );
            y += ROW_HEIGHT;
        }
    }

    const char *class_name( i32 value )
    {
        switch ( value )
        {
            case TF_CLASS_SCOUT: return ASTRAL_XORSTR_STATIC( "Scout" );
            case TF_CLASS_SNIPER: return ASTRAL_XORSTR_STATIC( "Sniper" );
            case TF_CLASS_SOLDIER: return ASTRAL_XORSTR_STATIC( "Soldier" );
            case TF_CLASS_DEMOMAN: return ASTRAL_XORSTR_STATIC( "Demoman" );
            case TF_CLASS_MEDIC: return ASTRAL_XORSTR_STATIC( "Medic" );
            case TF_CLASS_HEAVYWEAPONS: return ASTRAL_XORSTR_STATIC( "Heavy" );
            case TF_CLASS_PYRO: return ASTRAL_XORSTR_STATIC( "Pyro" );
            case TF_CLASS_SPY: return ASTRAL_XORSTR_STATIC( "Spy" );
            case TF_CLASS_ENGINEER: return ASTRAL_XORSTR_STATIC( "Engineer" );
            default: return ASTRAL_XORSTR_STATIC( "No class" );
        }
    }

    void draw_arsenal( C_TFPlayer *local )
    {
        constexpr i32 HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * 4 };
        const indicator_frame_t frame{ begin_card( s_arsenal, g_cfg.m_misc.m_astral_sense_arsenal_position,
                                                   250, HEIGHT, ASTRAL_XORSTR_STATIC( "arsenal" ) ) };
        i32 y{ frame.m_content.top + PAD };
        char value[ 64 ]{ };
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "class" ),
                   local ? class_name( local->m_iClass( ) ) : ASTRAL_XORSTR_STATIC( "offline" ),
                   g_theme.m_accent, frame.m_enter );
        y += ROW_HEIGHT;

        C_TFWeaponBase *weapon{ local && local->IsAlive( ) ? g_cl.m_weapon : nullptr };
        if ( !weapon )
        {
            text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "weapon" ),
                       ASTRAL_XORSTR_STATIC( "none" ), MENU_TEXT_DIM, frame.m_enter );
            return;
        }

        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "item #{}" ).get( ), weapon->m_iItemDefinitionIndex( ) );
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "weapon" ), value, MENU_TEXT, frame.m_enter );
        y += ROW_HEIGHT;
        const i32 ammo_type{ weapon->m_iPrimaryAmmoType( ) };
        const i32 reserve{ ammo_type >= 0 ? ( &local->m_iAmmo( ) )[ ammo_type ] : 0 };
        str::format_to( value, sizeof( value ), weapon->m_iClip1( ) >= 0 ? ASTRAL_XORSTR( "{} / {}" ).get( ) : ASTRAL_XORSTR( "{}" ).get( ),
                        weapon->m_iClip1( ) >= 0 ? weapon->m_iClip1( ) : reserve, reserve );
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "ammunition" ), value,
                   weapon->m_iClip1( ) == 0 ? CONSOLE_ERROR : STATUS_WARNING, frame.m_enter );
        y += ROW_HEIGHT;
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}" ).get( ), static_cast< i32 >( local->m_vecVelocity( ).length2d( ) + 0.5f ) );
        text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "movement" ), value, MENU_TEXT, frame.m_enter );
    }

    void telemetry_row( const indicator_frame_t &frame, i32 y, const char *label, i32 value, i32 maximum,
                        const char *suffix, bool inverse )
    {
        char text[ 32 ]{ };
        str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "{} {}" ).get( ), value, suffix );
        const float fraction{ math::clamp( static_cast< float >( value ) / math::bigger( maximum, 1 ), 0.0f, 1.0f ) };
        const float quality{ inverse ? 1.0f - fraction : fraction };
        const color_t color{ health_color( static_cast< i32 >( quality * 100.0f ), 100 ) };
        text_pair( frame.m_content.left, frame.m_content.right, y, label, text, color, frame.m_enter );
        const rect_t track{ frame.m_content.left, y + ROW_HEIGHT - 3, frame.m_content.right, y + ROW_HEIGHT };
        g_render.box( track, fade( CONTROL_BACKGROUND, frame.m_enter ), true );
        const i32 fill{ static_cast< i32 >( ( track.right - track.left ) * fraction ) };
        if ( fill ) g_render.box( rect_t{ track.left, track.top, track.left + fill, track.bottom }, fade( color, frame.m_enter ), true );
    }

    void draw_telemetry( )
    {
        constexpr i32 HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * 4 };
        const indicator_frame_t frame{ begin_card( s_telemetry, g_cfg.m_misc.m_astral_sense_telemetry_position,
                                                   250, HEIGHT, ASTRAL_XORSTR_STATIC( "network telemetry" ) ) };
        const float frame_time{ g_tf2.m_globals ? g_tf2.m_globals->absoluteframetime : 0.0f };
        const float instant{ frame_time > 0.0001f ? 1.0f / frame_time : s_smoothed_fps };
        s_smoothed_fps += ( instant - s_smoothed_fps ) * 0.04f;
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 0 };
        const i32 tickrate{ TICK_INTERVAL > 0.0f ? static_cast< i32 >( 1.0f / TICK_INTERVAL + 0.5f ) : 0 };
        i32 y{ frame.m_content.top + PAD };
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "FPS" ), static_cast< i32 >( s_smoothed_fps + 0.5f ), 240, ASTRAL_XORSTR_STATIC( "fps" ), false );
        y += ROW_HEIGHT + 3;
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "PING" ), ping, 150, ASTRAL_XORSTR_STATIC( "ms" ), true );
        y += ROW_HEIGHT + 3;
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "SERVER" ), tickrate, 128, ASTRAL_XORSTR_STATIC( "tick" ), false );
    }

    void draw_readiness( C_TFPlayer *local )
    {
        constexpr i32 HEIGHT{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + ROW_HEIGHT * 4 };
        const indicator_frame_t frame{ begin_card( s_readiness, g_cfg.m_misc.m_astral_sense_readiness_position,
                                                   250, HEIGHT, ASTRAL_XORSTR_STATIC( "readiness" ) ) };
        const i32 hp{ local ? local->m_iHealth( ) : 0 }, maximum{ max_health( local ) };
        C_TFWeaponBase *weapon{ local ? g_cl.m_weapon : nullptr };
        const i32 clip{ weapon && weapon->m_iClip1( ) >= 0 ? weapon->m_iClip1( ) : 0 };
        const i32 ammo_type{ weapon ? weapon->m_iPrimaryAmmoType( ) : -1 };
        const i32 reserve{ local && ammo_type >= 0 ? ( &local->m_iAmmo( ) )[ ammo_type ] : 0 };
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 150 };
        i32 y{ frame.m_content.top + PAD };
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "HEALTH" ), hp, maximum, ASTRAL_XORSTR_STATIC( "hp" ), false ); y += ROW_HEIGHT + 3;
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "AMMO" ), clip + reserve, math::bigger( clip + reserve, 24 ), ASTRAL_XORSTR_STATIC( "total" ), false ); y += ROW_HEIGHT + 3;
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "MOTION" ), local ? static_cast< i32 >( local->m_vecVelocity( ).length2d( ) ) : 0,
                       local ? math::bigger( static_cast< i32 >( local->m_flMaxspeed( ) ), 1 ) : 300, ASTRAL_XORSTR_STATIC( "u/s" ), false ); y += ROW_HEIGHT + 3;
        telemetry_row( frame, y, ASTRAL_XORSTR_STATIC( "LINK" ), ping, 150, ASTRAL_XORSTR_STATIC( "ms" ), true );
    }

    void update_performance_samples( C_TFPlayer *local, u64 now )
    {
        if ( now - s_last_performance_sample < 500 ) return;
        s_last_performance_sample = now;
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 0 };
        const performance_sample_t sample{ static_cast< i32 >( s_smoothed_fps + 0.5f ), ping,
            local && local->IsAlive( ) ? static_cast< i32 >( local->m_vecVelocity( ).length2d( ) + 0.5f ) : 0 };
        if ( s_performance_count == 60 )
        {
            for ( usize i{ 1 }; i < 60; ++i ) s_performance_samples[ i - 1 ] = s_performance_samples[ i ];
            --s_performance_count;
        }
        s_performance_samples[ s_performance_count++ ] = sample;
    }

    void draw_performance( )
    {
        constexpr i32 WIDTH{ 280 }, HEIGHT{ 116 };
        const indicator_frame_t frame{ begin_card( s_performance, g_cfg.m_misc.m_astral_sense_performance_position,
                                                   WIDTH, HEIGHT, ASTRAL_XORSTR_STATIC( "performance - 30s" ) ) };
        const rect_t graph{ frame.m_content.left, frame.m_content.top + PAD, frame.m_content.right, frame.m_content.bottom - PAD - ROW_HEIGHT };
        g_render.box( graph, fade( CONTROL_BACKGROUND, frame.m_enter ), true );
        if ( s_performance_count > 1 )
        {
            const float step{ static_cast< float >( graph.right - graph.left ) / 59.0f };
            for ( usize i{ 1 }; i < s_performance_count; ++i )
            {
                const auto point = [&]( i32 value, i32 maximum, usize at )
                {
                    return vec2{ static_cast< float >( graph.left ) + ( 60 - s_performance_count + at ) * step,
                                 static_cast< float >( graph.bottom ) - math::clamp( static_cast< float >( value ) / maximum, 0.0f, 1.0f ) * ( graph.bottom - graph.top ) };
                };
                g_render.line( point( s_performance_samples[ i - 1 ].m_fps, 240, i - 1 ), point( s_performance_samples[ i ].m_fps, 240, i ), fade( color_t{ 90, 220, 120 }, frame.m_enter ), 1.5f );
                g_render.line( point( s_performance_samples[ i - 1 ].m_ping, 150, i - 1 ), point( s_performance_samples[ i ].m_ping, 150, i ), fade( STATUS_WARNING, frame.m_enter ), 1.5f );
                g_render.line( point( s_performance_samples[ i - 1 ].m_speed, 600, i - 1 ), point( s_performance_samples[ i ].m_speed, 600, i ), fade( g_theme.m_accent, frame.m_enter ), 1.5f );
            }
        }
        g_render.text( MENU_FONT_TINY( ), point_t{ graph.left, graph.bottom + 3 }, fade( color_t{ 90, 220, 120 }, frame.m_enter ), ASTRAL_XORSTR_STATIC( "FPS" ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ graph.left + 55, graph.bottom + 3 }, fade( STATUS_WARNING, frame.m_enter ), ASTRAL_XORSTR_STATIC( "PING" ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ graph.left + 115, graph.bottom + 3 }, fade( g_theme.m_accent, frame.m_enter ), ASTRAL_XORSTR_STATIC( "SPEED" ) );
    }

    void draw_history( u64 now )
    {
        const usize rows{ math::bigger< usize >( s_history_count + ( s_map_started ? 1 : 0 ), 1 ) };
        const i32 height{ 2 + INDICATOR_TITLE_HEIGHT + 1 + INDICATOR_RULE_DEPTH + PAD * 2 + static_cast< i32 >( math::smaller< usize >( rows, 5 ) ) * ROW_HEIGHT };
        const indicator_frame_t frame{ begin_card( s_history, g_cfg.m_misc.m_astral_sense_history_position,
                                                   280, height, ASTRAL_XORSTR_STATIC( "session history" ) ) };
        i32 y{ frame.m_content.top + PAD };
        char value[ 64 ]{ };
        if ( s_map_started )
        {
            const u64 seconds{ ( now - s_map_started ) / 1000 };
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}K {}D {}A  {}:{:02}" ).get( ), s_kills, s_deaths, s_assists, seconds / 60, seconds % 60 );
            text_pair( frame.m_content.left, frame.m_content.right, y, s_map_name, value, g_theme.m_accent, frame.m_enter );
            y += ROW_HEIGHT;
        }
        for ( usize i{ }; i < s_history_count && i + ( s_map_started ? 1 : 0 ) < 5; ++i )
        {
            const history_entry_t &entry{ s_history_entries[ i ] };
            str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}K {}D {}A  {}:{:02}" ).get( ), entry.m_kills, entry.m_deaths, entry.m_assists, entry.m_seconds / 60, entry.m_seconds % 60 );
            text_pair( frame.m_content.left, frame.m_content.right, y, entry.m_map, value, MENU_TEXT, frame.m_enter );
            y += ROW_HEIGHT;
        }
        if ( !s_map_started && !s_history_count ) text_pair( frame.m_content.left, frame.m_content.right, y, ASTRAL_XORSTR_STATIC( "maps" ), ASTRAL_XORSTR_STATIC( "none yet" ), MENU_TEXT_DIM, frame.m_enter );
    }

    void draw_screen_alerts( C_TFPlayer *local )
    {
        if ( !local || !local->IsAlive( ) ) return;
        const i32 hp{ local->m_iHealth( ) }, maximum{ max_health( local ) };
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 0 };
        color_t color{ };
        u8 alpha{ };
        if ( hp * 4 <= maximum ) { color = CONSOLE_ERROR; alpha = static_cast< u8 >( 45 + ( gui::clock( ) / 7 % 35 ) ); }
        else if ( ping >= 150 ) { color = STATUS_WARNING; alpha = 38; }
        else if ( local->InCond( TF_COND_MARKEDFORDEATH ) ) { color = STATUS_WARNING; alpha = 48; }
        if ( !alpha ) return;
        color.a = alpha;
        constexpr i32 EDGE{ 12 };
        g_render.box( rect_t{ 0, 0, g_cl.m_screen_width, EDGE }, color, true );
        g_render.box( rect_t{ 0, g_cl.m_screen_height - EDGE, g_cl.m_screen_width, g_cl.m_screen_height }, color, true );
        g_render.box( rect_t{ 0, EDGE, EDGE, g_cl.m_screen_height - EDGE }, color, true );
        g_render.box( rect_t{ g_cl.m_screen_width - EDGE, EDGE, g_cl.m_screen_width, g_cl.m_screen_height - EDGE }, color, true );
    }

    bool has_conditions( C_TFPlayer *local )
    {
        return local && local->IsAlive( ) &&
            ( local->IsBurning( ) || local->IsBleeding( ) || local->IsJarated( ) || local->IsMilked( ) ||
              local->IsMarkedForDeath( ) || local->IsCritBoosted( ) || local->IsUbered( ) ||
              local->IsQuickFixUbered( ) || local->IsCloaked( ) || local->IsDisguised( ) ||
              local->IsZoomed( ) || local->IsCharging( ) || local->IsStunned( ) ||
              local->IsTaunting( ) || local->IsBonked( ) );
    }

    bool has_owned_building( C_TFPlayer *local )
    {
        if ( !local || local->m_iClass( ) != TF_CLASS_ENGINEER ) return false;
        for ( const auto *cached : g_entity_cache.buildings( ) )
        {
            C_BaseObject *object{ cached->m_entity->As< C_BaseObject * >( ) };
            if ( object && !object->IsDormant( ) && object->m_hBuilder( ).Get( ) == local && object->m_iHealth( ) > 0 )
                return true;
        }
        return false;
    }

    bool has_context_alert( C_TFPlayer *local )
    {
        if ( !local || !local->IsAlive( ) ) return false;
        const i32 hp{ local->m_iHealth( ) }, maximum{ max_health( local ) };
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        const i32 ping{ channel ? static_cast< i32 >( ( channel->GetLatency( FLOW_INCOMING ) + channel->GetLatency( FLOW_OUTGOING ) ) * 1000.0f ) : 0 };
        const i32 healers{ local->GetSharedPlayerData( ) ? local->GetSharedPlayerData( )->m_nNumHealers( ) : 0 };
        return hp * 100 <= maximum * 35 || hp > maximum ||
               ( g_cl.m_weapon && g_cl.m_weapon->m_iClip1( ) >= 0 && g_cl.m_weapon->m_iClip1( ) <= 1 ) ||
               !local->IsOnGround( ) || healers > 0 || s_streak >= 3 || local->IsCritBoosted( ) ||
               local->IsMarkedForDeath( ) || local->IsCloaked( ) || ping >= 90;
    }
}

void draw_astral_sense( )
{
    if ( !g_cfg.m_misc.m_astral_sense )
    {
        hide_indicator( s_player_card );
        hide_indicator( s_status ); hide_indicator( s_damage ); hide_indicator( s_speed );
        hide_indicator( s_health ); hide_indicator( s_session );
        hide_indicator( s_resources ); hide_indicator( s_buildings ); hide_indicator( s_combat );
        hide_indicator( s_conditions ); hide_indicator( s_compass );
        hide_indicator( s_objective ); hide_indicator( s_events ); hide_indicator( s_alerts );
        hide_indicator( s_arsenal ); hide_indicator( s_telemetry ); hide_indicator( s_readiness );
        hide_indicator( s_history ); hide_indicator( s_performance );
        return;
    }

    const u64 now{ gui::clock( ) };
    update_state( g_cl.m_local, now );
    update_performance_samples( g_cl.m_local, now );
    draw_world( g_cl.m_local, now );
    const bool live{ g_tf2.m_engine->IsInGame( ) && g_cl.m_local };
    const bool alive{ live && g_cl.m_local->IsAlive( ) };
    if ( live && g_cfg.m_misc.m_astral_sense_screen_alerts ) draw_screen_alerts( g_cl.m_local );

    if ( live && g_cfg.m_misc.m_astral_sense_status ) draw_status( ); else hide_indicator( s_status );
    // Player Card is intentionally the dashboard exception: it remains present whenever
    // enabled, including while dead, disconnected, or sitting on the main menu.
    if ( g_cfg.m_misc.m_astral_sense_player_card ) draw_player_card( g_cl.m_local );
    else hide_indicator( s_player_card );

    if ( live && g_cfg.m_misc.m_astral_sense_damage )
    {
        constexpr i32 WIDTH{ 210 };
        constexpr i32 HEIGHT{ 91 };
        const indicator_frame_t frame{ begin_card( s_damage, g_cfg.m_misc.m_astral_sense_damage_position,
                                                   WIDTH, HEIGHT, ASTRAL_XORSTR_STATIC( "damage taken - 60s" ) ) };
        i32 peak{ 1 }, total{ };
        for ( i32 value : s_damage_buckets ) { peak = math::bigger( peak, value ); total += value; }
        char amount[ 24 ]{ }; str::format_to( amount, sizeof( amount ), ASTRAL_XORSTR( "{} total" ).get( ), total );
        g_render.text( MENU_FONT_TINY( ), point_t{ frame.m_content.right, frame.m_content.top + PAD },
                       fade( MENU_TEXT, frame.m_enter ), amount, false, render::text_align::right );
        const rect_t graph{ frame.m_content.left, frame.m_content.top + PAD + ROW_HEIGHT,
                            frame.m_content.right, frame.m_content.bottom - PAD };
        g_render.box( graph, fade( CONTROL_BACKGROUND, frame.m_enter ), true );
        const float column{ static_cast< float >( graph.right - graph.left ) / DAMAGE_BUCKETS };
        for ( usize index{ }; index < DAMAGE_BUCKETS; ++index )
        {
            const i32 value{ s_damage_buckets[ ( s_damage_head + index + 1 ) % DAMAGE_BUCKETS ] };
            if ( !value ) continue;
            const i32 high{ math::bigger( 1, value * ( graph.bottom - graph.top ) / peak ) };
            const i32 x{ graph.left + static_cast< i32 >( index * column ) };
            g_render.box( rect_t{ x, graph.bottom - high, x + math::bigger( 1, static_cast< i32 >( column ) - 1 ), graph.bottom },
                          fade( health_color( peak - value, peak ), frame.m_enter ), true );
        }
    } else hide_indicator( s_damage );

    char value[ 48 ]{ };
    if ( alive && g_cfg.m_misc.m_astral_sense_speed )
    {
        const i32 speed{ g_cl.m_local && g_cl.m_local->IsAlive( ) ? static_cast< i32 >( g_cl.m_local->m_vecVelocity( ).length2d( ) + 0.5f ) : 0 };
        const i32 maximum{ g_cl.m_local ? math::bigger( static_cast< i32 >( g_cl.m_local->m_flMaxspeed( ) ), 1 ) : 300 };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{} / {} u/s" ).get( ), speed, maximum );
        draw_small_card( s_speed, g_cfg.m_misc.m_astral_sense_speed_position, ASTRAL_XORSTR_STATIC( "speed" ),
                         ASTRAL_XORSTR_STATIC( "current" ), value, static_cast< float >( speed ) / maximum,
                         speed > maximum ? color_t{ 90, 180, 255 } : g_theme.m_accent );
    } else hide_indicator( s_speed );

    if ( alive && g_cfg.m_misc.m_astral_sense_health )
    {
        const i32 hp{ g_cl.m_local ? g_cl.m_local->m_iHealth( ) : 0 };
        const i32 maximum{ max_health( g_cl.m_local ) };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{} / {} hp" ).get( ), hp, maximum );
        draw_small_card( s_health, g_cfg.m_misc.m_astral_sense_health_position, ASTRAL_XORSTR_STATIC( "health" ),
                         ASTRAL_XORSTR_STATIC( "current" ), value, static_cast< float >( hp ) / maximum,
                         health_color( hp, maximum ) );
    } else hide_indicator( s_health );

    if ( live && g_cfg.m_misc.m_astral_sense_session )
    {
        const u64 seconds{ ( now - s_started ) / 1000 };
        str::format_to( value, sizeof( value ), ASTRAL_XORSTR( "{}:{:02}:{:02}" ).get( ), seconds / 3600,
                        seconds % 3600 / 60, seconds % 60 );
        draw_small_card( s_session, g_cfg.m_misc.m_astral_sense_session_position, ASTRAL_XORSTR_STATIC( "session" ),
                         ASTRAL_XORSTR_STATIC( "uptime" ), value, 0.0f, MENU_TEXT );
    } else hide_indicator( s_session );

    if ( alive && g_cfg.m_misc.m_astral_sense_resources ) draw_resources( g_cl.m_local ); else hide_indicator( s_resources );
    if ( has_owned_building( g_cl.m_local ) && g_cfg.m_misc.m_astral_sense_buildings ) draw_buildings( g_cl.m_local ); else hide_indicator( s_buildings );
    if ( live && s_feed_count && g_cfg.m_misc.m_astral_sense_combat ) draw_combat( ); else hide_indicator( s_combat );
    if ( has_conditions( g_cl.m_local ) && g_cfg.m_misc.m_astral_sense_conditions ) draw_conditions( g_cl.m_local ); else hide_indicator( s_conditions );
    if ( alive && g_cl.m_local->m_vecVelocity( ).length2d( ) > 5.0f && g_cfg.m_misc.m_astral_sense_compass ) draw_compass( g_cl.m_local ); else hide_indicator( s_compass );
    if ( live && g_cfg.m_misc.m_astral_sense_objective ) draw_objective( g_cl.m_local, now ); else hide_indicator( s_objective );
    if ( live && s_event_count && now - s_event_feed[ 0 ].m_time < 10000 && g_cfg.m_misc.m_astral_sense_events ) draw_event_stream( now ); else hide_indicator( s_events );
    if ( has_context_alert( g_cl.m_local ) && g_cfg.m_misc.m_astral_sense_alerts ) draw_alerts( g_cl.m_local ); else hide_indicator( s_alerts );
    if ( alive && g_cfg.m_misc.m_astral_sense_arsenal ) draw_arsenal( g_cl.m_local ); else hide_indicator( s_arsenal );
    if ( live && g_cfg.m_misc.m_astral_sense_telemetry ) draw_telemetry( ); else hide_indicator( s_telemetry );
    if ( alive && g_cfg.m_misc.m_astral_sense_readiness ) draw_readiness( g_cl.m_local ); else hide_indicator( s_readiness );
    if ( ( live || s_history_count ) && g_cfg.m_misc.m_astral_sense_history ) draw_history( now ); else hide_indicator( s_history );
    if ( live && s_performance_count > 1 && g_cfg.m_misc.m_astral_sense_performance_graphs ) draw_performance( ); else hide_indicator( s_performance );
}

void astral_sense_player_death( IGameEvent *event )
{
    if ( !event || !g_cl.m_local ) return;
    if ( event->GetInt( ASTRAL_XORSTR( "death_flags" ).get( ) ) & TF_DEATH_FEIGN_DEATH ) return;

    const i32 local{ g_cl.m_local->entindex( ) };
    const i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "userid" ).get( ) ) ) };
    const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "attacker" ).get( ) ) ) };
    const i32 assister{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "assister" ).get( ) ) ) };

    bool death{ };
    i32 other{ };
    if ( attacker == local && victim != local )
    {
        ++s_kills;
        ++s_streak;
        s_best_streak = math::bigger( s_best_streak, s_streak );
        other = victim;
    } else if ( victim == local )
    {
        s_streak = 0;
        death = true;
        other = attacker;
    } else if ( assister == local )
    {
        ++s_assists;
        return;
    } else
    {
        return;
    }

    for ( usize index{ math::smaller< usize >( s_feed_count, 3 ) }; index > 0; --index ) s_feed[ index ] = s_feed[ index - 1 ];
    s_feed_count = math::smaller< usize >( s_feed_count + 1, 4 );
    combat_line_t &line{ s_feed[ 0 ] };
    line = combat_line_t{ };
    line.m_death = death;
    player_info_t info{ };
    if ( other > 0 && g_tf2.m_engine->GetPlayerInfo( other, &info ) )
        str::copy( line.m_name, sizeof( line.m_name ), info.name );
    else
        str::copy( line.m_name, sizeof( line.m_name ), death ? ASTRAL_XORSTR_STATIC( "world" ) : ASTRAL_XORSTR_STATIC( "enemy" ) );
    str::copy( line.m_weapon, sizeof( line.m_weapon ), event->GetString( ASTRAL_XORSTR( "weapon" ).get( ) ) );
    const i32 custom{ event->GetInt( ASTRAL_XORSTR( "customkill" ).get( ) ) };
    line.m_headshot = custom == TF_DMG_CUSTOM_HEADSHOT;
    line.m_backstab = custom == TF_DMG_CUSTOM_BACKSTAB;
    line.m_crit = event->GetInt( ASTRAL_XORSTR( "crit_type" ).get( ) ) == 2;
}

void astral_sense_game_event( IGameEvent *event )
{
    if ( !event ) return;
    const hash_t name{ HASH_RT( event->GetName( ) ) };
    char text[ 72 ]{ };

    if ( name == HASH_CT( "player_death" ) && g_cl.m_local )
    {
        if ( event->GetInt( ASTRAL_XORSTR( "death_flags" ).get( ) ) & TF_DEATH_FEIGN_DEATH ) return;
        const i32 local{ g_cl.m_local->entindex( ) };
        const i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "userid" ).get( ) ) ) };
        const i32 attacker{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "attacker" ).get( ) ) ) };
        i32 other{ };
        const char *action{ };
        color_t color{ MENU_TEXT };
        if ( attacker == local && victim != local )
        {
            action = ASTRAL_XORSTR_STATIC( "Eliminated" ); other = victim; color = color_t{ 90, 220, 120 };
        } else if ( victim == local )
        {
            action = ASTRAL_XORSTR_STATIC( "Downed by" ); other = attacker; color = CONSOLE_ERROR;
        } else return;
        player_info_t info{ };
        const char *who{ other > 0 && g_tf2.m_engine->GetPlayerInfo( other, &info ) ? info.name : ASTRAL_XORSTR_STATIC( "world" ) };
        str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "{} {}" ).get( ), action, who );
        push_event( text, color );
    } else if ( name == HASH_CT( "teamplay_round_win" ) )
    {
        s_round_winner = event->GetInt( ASTRAL_XORSTR( "team" ).get( ) );
        s_round_event_time = gui::clock( );
        push_event( s_round_winner == ( g_cl.m_local ? g_cl.m_local->m_iTeamNum( ) : 0 ) ?
                        ASTRAL_XORSTR_STATIC( "Round won" ) : ASTRAL_XORSTR_STATIC( "Round lost" ),
                    s_round_winner == ( g_cl.m_local ? g_cl.m_local->m_iTeamNum( ) : 0 ) ? color_t{ 90, 220, 120 } : CONSOLE_ERROR );
    } else if ( name == HASH_CT( "teamplay_point_captured" ) )
    {
        ++s_captures;
        s_capture_team = event->GetInt( ASTRAL_XORSTR( "team" ).get( ) );
        const char *point{ event->GetString( ASTRAL_XORSTR( "cpname" ).get( ) ) };
        str::format_to( text, sizeof( text ), point && *point ? ASTRAL_XORSTR( "Captured {}" ).get( ) : ASTRAL_XORSTR( "Objective captured" ).get( ), point );
        push_event( text, g_theme.m_accent );
    } else if ( name == HASH_CT( "player_chargedeployed" ) && g_cl.m_local )
    {
        const i32 user{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( ASTRAL_XORSTR( "userid" ).get( ) ) ) };
        if ( user == g_cl.m_local->entindex( ) ) push_event( ASTRAL_XORSTR_STATIC( "UberCharge deployed" ), STATUS_WARNING );
    }
}

void astral_sense_level_init( const char *map_name )
{
    const u64 now{ gui::clock( ) };
    if ( s_map_started && str::length( s_map_name ) )
    {
        for ( usize index{ math::smaller< usize >( s_history_count, 4 ) }; index > 0; --index )
            s_history_entries[ index ] = s_history_entries[ index - 1 ];
        s_history_count = math::smaller< usize >( s_history_count + 1, 5 );
        history_entry_t &entry{ s_history_entries[ 0 ] };
        entry = history_entry_t{ };
        str::copy( entry.m_map, sizeof( entry.m_map ), s_map_name );
        entry.m_kills = s_kills; entry.m_deaths = s_deaths; entry.m_assists = s_assists;
        entry.m_seconds = ( now - s_map_started ) / 1000;
    }
    const char *clean{ map_name ? map_name : ASTRAL_XORSTR_STATIC( "unknown" ) };
    if ( str::starts_with( clean, str::length( clean ), ASTRAL_XORSTR_STATIC( "maps/" ), 5 ) ) clean += 5;
    str::copy( s_map_name, sizeof( s_map_name ), clean );
    const usize extension{ str::find( s_map_name, str::length( s_map_name ), ASTRAL_XORSTR_STATIC( ".bsp" ), 4 ) };
    if ( extension != static_cast< usize >( -1 ) ) s_map_name[ extension ] = '\0';
    s_map_started = now;
    s_captures = s_capture_team = s_round_winner = 0;
    s_round_event_time = 0;
    s_kills = s_assists = s_streak = s_best_streak = s_deaths = s_peak_speed = 0;
    s_feed_count = s_event_count = s_trail_count = 0;
    s_last_health = -1;
    s_was_alive = false;
    for ( i32 &bucket : s_damage_buckets ) bucket = 0;
    s_damage_head = 0;
    s_damage_second = gui::clock( ) / 1000;
}
