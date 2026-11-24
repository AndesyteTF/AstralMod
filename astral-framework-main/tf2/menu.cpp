#include "base.hpp"
#include "gui/gui.hpp"
#include "gui/particles.hpp"
#include "gui/theme.hpp"
#include "steam.hpp"

#include "config.hpp"
#include "config_storage.hpp"
#include "features/crash_reporter.hpp"
#include "features/features.hpp"
#include "menu.hpp"
#include "preview.hpp"
#include "script/ide.hpp"
#include "script_api.hpp"

//
// The big standing TODO list for this file:
// 1: [X] Remove all the nullchecks (they're useless).
// 2. [X] Either remove all the static bool functions used in set_render_on calls or convert everything to a static bool func (lambda/function mismashing is gonna make code harder to read).
// 3. [X] Move all the item arrays into the scope of the functions.
// 4. [X] Either remove the static assertions or do them cleaner somewhere in the bottom of this file.
// 5. [X] Switch Ultra to BEAST MODE in splashbot in developer builds (super necessary).
//
// Fill the [ ] with an X like [X] when you've finished this.
//

// Should make this easier if we add more class profile support in the future.
#define MAKE_PREDICATES( func, name )                            \
    static bool ( *const name[ config::CFG_PROFILE_COUNT ] )( ){ \
        &func< config::CFG_PROFILE_SCOUT >,                      \
        &func< config::CFG_PROFILE_SOLDIER >,                    \
        &func< config::CFG_PROFILE_PYRO >,                       \
        &func< config::CFG_PROFILE_DEMOMAN >,                    \
        &func< config::CFG_PROFILE_HEAVY >,                      \
        &func< config::CFG_PROFILE_ENGINEER >,                   \
        &func< config::CFG_PROFILE_MEDIC >,                      \
        &func< config::CFG_PROFILE_SNIPER >,                     \
        &func< config::CFG_PROFILE_SPY >,                        \
    };

enum : usize
{
    TARGET_GROUP_PLAYERS   = 0,
    TARGET_GROUP_BUILDINGS = 1,
    TARGET_GROUP_WORLD     = 2,
    TARGET_GROUP_COUNT     = 3,
};

template< usize target >
static bool editing_target( )
{
    return g_cfg.m_esp.m_target_group == TARGET_GROUP_PLAYERS &&
           g_cfg.m_esp.m_edited == target;
}

template< usize target >
static bool editing_blended_health( )
{
    return editing_target< target >( ) &&
           g_cfg.m_esp.m_profiles[ target ].m_health_style != config::HEALTH_STYLE_SOLID;
}

template< usize event >
static bool editing_hitmarker_event( )
{
    return g_cfg.m_hitmarker.m_enabled && g_cfg.m_hitmarker.m_edited_event == event;
}

template< usize event >
static bool editing_hitmarker_color( )
{
    return editing_hitmarker_event< event >( ) && g_cfg.m_hitmarker.m_override_color[ event ];
}

static bool editing_local_model( usize model )
{
    return editing_target< 2 >( ) && g_cfg.m_chams.m_local_model == model;
}

static bool editing_local_player( )
{
    return editing_local_model( config::LOCAL_MODEL_PLAYER );
}

static bool editing_local_viewmodel( )
{
    return editing_local_model( config::LOCAL_MODEL_VIEWMODEL );
}

static bool editing_local_weapon( )
{
    return editing_local_model( config::LOCAL_MODEL_WEAPON );
}

static bool editing_enemy_model( usize model )
{
    return editing_target< 0 >( ) && g_cfg.m_chams.m_enemy_model == model;
}

static bool editing_enemy_player( )
{
    return editing_enemy_model( config::ENEMY_MODEL_PLAYER );
}

static bool editing_enemy_backtrack( )
{
    return editing_enemy_model( config::ENEMY_MODEL_LAGCOMP );
}

static bool editing_buildings( )
{
    return g_cfg.m_esp.m_target_group == TARGET_GROUP_BUILDINGS;
}

template< usize item >
static bool editing_world( )
{
    return g_cfg.m_esp.m_target_group == TARGET_GROUP_WORLD && g_cfg.m_esp.m_edited == item;
}

template< usize count >
static bool world_marked( const bool ( &elements )[ count ], const config::cham_profile_t &chams,
                          const config::glow_profile_t &glow )
{
    for ( bool element : elements )
    {
        if ( element )
        {
            return true;
        }
    }

    return !chams.off( ) || !glow.off( );
}

static bool target_marked( usize group, usize item )
{
    if ( group == TARGET_GROUP_PLAYERS )
    {
        if ( item >= config::ESP_TARGET_COUNT )
        {
            return false;
        }

        const config::esp_profile_t &profile{ g_cfg.m_esp.m_profiles[ item ] };

        for ( bool element : profile.m_elements )
        {
            if ( element )
            {
                return true;
            }
        }

        return !profile.m_chams.off( ) || !g_cfg.m_glow.m_players[ item ].off( );
    }

    if ( group == TARGET_GROUP_BUILDINGS )
    {
        for ( bool element : g_cfg.m_esp.m_building_elements )
        {
            if ( element )
            {
                return true;
            }
        }

        return !g_cfg.m_chams.m_buildings.off( ) || !g_cfg.m_glow.m_buildings.off( );
    }

    switch ( item )
    {
        case config::WORLD_PICKUPS:
            return world_marked( g_cfg.m_esp.m_pickup_elements, g_cfg.m_chams.m_pickups, g_cfg.m_glow.m_pickups );
        case config::WORLD_INTEL:
            return world_marked( g_cfg.m_esp.m_intel_elements, g_cfg.m_chams.m_intel, g_cfg.m_glow.m_intel );
        case config::WORLD_PROJECTILES:
            return world_marked( g_cfg.m_esp.m_projectile_elements, g_cfg.m_chams.m_projectiles,
                                 g_cfg.m_glow.m_projectiles );
        case config::WORLD_NPCS:
            return world_marked( g_cfg.m_esp.m_npc_elements, g_cfg.m_chams.m_npcs, g_cfg.m_glow.m_npcs );
        default: return false;
    }
}

static bool weather_enabled( )
{
    return g_cfg.m_weather.m_type != config::WEATHER_OFF;
}

static bool world_modulated( )
{
    return g_cfg.m_world.m_modulate;
}

static bool post_processed( )
{
    return g_cfg.m_post_process.m_enabled;
}

static bool bloom_enabled( )
{
    return g_cfg.m_post_process.m_enabled && g_cfg.m_post_process.m_bloom > 0;
}

static bool footsteps_audible( )
{
    return !g_cfg.m_misc.m_removals[ config::REMOVAL_FOOTSTEPS ];
}

static bool fog_shaped( )
{
    return g_cfg.m_fog.m_mode == config::FOG_CUSTOM;
}

static bool fog_driven( )
{
    return g_cfg.m_fog.m_mode != config::FOG_OFF;
}

static bool tracers_drawn_by_us( )
{
    return g_cfg.m_tracers.drawn_by_us( );
}

static bool tracer_lines( )
{
    return g_cfg.m_tracers.m_style == config::TRACER_LINES;
}

static bool impact_condition( )
{
    return g_cfg.m_tracers.m_style == config::TRACER_LINES && g_cfg.m_tracers.m_impact_marker;
}

static bool tracers_colour_entities( )
{
    return g_cfg.m_tracers.drawn_by_us( ) && !g_cfg.m_tracers.m_local_only;
}

static bool tracer_impact_entities( )
{
    return g_cfg.m_tracers.drawn_by_us( ) && !g_cfg.m_tracers.m_local_only && g_cfg.m_tracers.m_impact_marker;
}

static bool strafing_with_wasd( )
{
    return g_cfg.m_misc.m_auto_strafe && g_cfg.m_misc.m_auto_strafe_mode == 1;
}

static bool pogo_shown( )
{
    return g_cfg.m_rjump.m_enabled && g_cfg.m_rjump.m_pogo;
}

static bool pills_fighting_crits( )
{
    return g_cfg.m_pills.m_seed_spin && g_cfg.m_crits.m_enabled;
}

static bool is_scopezoom_removal_enabled( )
{
    return g_cfg.m_misc.m_removals[ config::REMOVAL_SCOPE_ZOOM ] == true && g_cfg.m_misc.m_removals[ config::REMOVAL_SCOPE_OVERLAY ] == true;
}

static usize s_applied_menu_style{ static_cast< usize >( -1 ) };

static void apply_accent( )
{
    const bool        nextgen{ g_cfg.m_misc.m_menu_style == gui::MENU_STYLE_NEXTGEN };
    const color_t     background{ nextgen ? color_t{ 42, 46, 59, 255 } : g_cfg.m_misc.m_ui_background };
    const color_t     surface{ nextgen ? color_t{ 37, 41, 53, 255 } : g_cfg.m_misc.m_ui_surface };
    const color_t     raised{ nextgen ? color_t{ 47, 51, 65, 255 } : g_cfg.m_misc.m_ui_raised };
    const color_t     border{ nextgen ? color_t{ 58, 62, 76, 255 } : g_cfg.m_misc.m_ui_border };
    const color_t     text{ nextgen ? color_t{ 255, 255, 255, 255 } : g_cfg.m_misc.m_ui_text };
    const color_t     muted{ nextgen ? color_t{ 201, 204, 212, 255 } : g_cfg.m_misc.m_ui_muted };
    const color_t     dim{ nextgen ? color_t{ 142, 147, 161, 255 } : g_cfg.m_misc.m_ui_dim };
    const color_t     hover{ nextgen ? color_t{ 53, 57, 70, 255 } : g_cfg.m_misc.m_ui_hover };
    const color_t     selection{ nextgen ? color_t{ 58, 62, 75, 255 } : g_cfg.m_misc.m_ui_selection };
    const color_t     warning{ g_cfg.m_misc.m_ui_warning };
    const color_t     error{ g_cfg.m_misc.m_ui_error };
    constexpr color_t white{ 255, 255, 255 };
    constexpr color_t black{ 0, 0, 0 };
    const auto        alpha = []( color_t value, u8 opacity )
    {
        value.a = opacity;
        return value;
    };

    MENU_BACKGROUND        = background;
    MENU_HEADER_BACKGROUND = alpha( raised, background.a );
    MENU_RAIL_BACKGROUND   = alpha( gui::mix_color( background, black, 22 ), background.a );
    MENU_SUBNAV_BACKGROUND = alpha( gui::mix_color( background, raised, 28 ), background.a );
    MENU_OUTER_BORDER      = border;
    MENU_INNER_BORDER      = gui::mix_color( border, background, 55 );
    MENU_SECTION_SEPARATOR = gui::mix_color( border, background, 35 );
    MENU_SUBNAV_SEPARATOR  = gui::mix_color( border, background, 48 );
    MENU_FOOTER_TEXT       = muted;
    MENU_FOOTER_TEXT_DIM   = dim;
    MENU_TEXT              = text;
    MENU_TEXT_MUTED        = muted;
    MENU_TEXT_DIM          = dim;
    MENU_TEXT_FAINT        = gui::mix_color( dim, background, 25 );
    MENU_LOGO_GLYPH        = g_cfg.m_misc.m_accent;
    MENU_WORDMARK          = gui::mix_color( text, white, 15 );

    RAIL_IDLE_COLOR          = dim;
    RAIL_HOVER_COLOR         = text;
    RAIL_HOVER_BACKGROUND    = hover;
    RAIL_ACTIVE_BACKGROUND   = selection;
    RAIL_FOOT_COLOR          = gui::mix_color( dim, background, 20 );
    SUBNAV_IDLE_COLOR        = muted;
    SUBNAV_HOVER_COLOR       = text;
    SUBNAV_ACTIVE_COLOR      = gui::mix_color( text, white, 12 );
    SUBNAV_HOVER_BACKGROUND  = hover;
    SUBNAV_ACTIVE_BACKGROUND = selection;

    CARD_BACKGROUND          = surface;
    CARD_TITLE_BACKGROUND    = raised;
    CARD_BORDER              = border;
    CARD_RULE                = gui::mix_color( border, g_cfg.m_misc.m_accent, 22 );
    CARD_TITLE_TEXT          = text;
    CONTROL_BACKGROUND       = gui::mix_color( surface, background, 48 );
    CONTROL_BACKGROUND_HOVER = gui::mix_color( surface, hover, 45 );
    CONTROL_BORDER           = border;
    CONTROL_BORDER_HOVER     = gui::mix_color( border, g_cfg.m_misc.m_accent, 42 );
    CONTROL_LABEL            = muted;
    CONTROL_VALUE            = text;
    STATUS_WARNING           = warning;

    CHECKBOX_BOX_IDLE    = CONTROL_BACKGROUND;
    CHECKBOX_BORDER_IDLE = border;
    CHECKBOX_TEXT_IDLE   = muted;
    CHECKBOX_TEXT_HOVER  = text;
    CHECKBOX_TEXT_ACTIVE = gui::mix_color( text, white, 12 );
    CHECKBOX_MARK_COLOR  = text;
    CHECKBOX_KEY_COLOR   = dim;
    SWATCH_BORDER        = border;
    SLIDER_BORDER        = border;
    SLIDER_VALUE_COLOR   = text;
    SLIDER_NOTCH_COLOR   = text;
    ROW_HOVER_BACKGROUND = alpha( hover, 255 );

    LIST_ROW_BACKGROUND     = gui::mix_color( surface, background, 32 );
    LIST_META_COLOR         = dim;
    LIST_TEXT_IDLE          = muted;
    LIST_TEXT_ACTIVE        = text;
    POPUP_BACKGROUND        = alpha( background, 255 );
    POPUP_ROW_HOVER         = alpha( hover, 255 );
    POPUP_ROW_SELECTED      = selection;
    BUTTON_BACKGROUND       = gui::mix_color( surface, selection, 55 );
    BUTTON_BACKGROUND_HOVER = alpha( hover, 255 );
    BUTTON_BACKGROUND_PRESS = gui::mix_color( selection, black, 25 );
    BUTTON_BORDER           = border;
    BUTTON_TEXT             = muted;
    BUTTON_TEXT_HOVER       = text;
    BUTTON_TEXT_PRESS       = text;
    PANEL_BACKGROUND        = alpha( surface, 232 );
    PANEL_HEADER_BACKGROUND = raised;
    PANEL_INNER_BORDER      = border;
    CHIP_IDLE_BACKGROUND    = CONTROL_BACKGROUND;
    CHIP_HOVER_BACKGROUND   = CONTROL_BACKGROUND_HOVER;
    CHIP_IDLE_TEXT          = muted;
    CHIP_ACTIVE_TEXT        = text;
    SEARCH_BACKGROUND       = CONTROL_BACKGROUND;
    SEARCH_BORDER           = border;

    PICKER_POPUP_BACKGROUND = alpha( background, 252 );
    PICKER_HEX_BACKGROUND   = CONTROL_BACKGROUND;
    PICKER_CHECKER_LIGHT    = gui::mix_color( surface, white, 20 );
    PICKER_CHECKER_DARK     = gui::mix_color( surface, black, 20 );
    PICKER_CURSOR_COLOR     = text;
    PICKER_CURSOR_SHADOW    = alpha( black, 220 );
    SCROLLBAR_TRACK         = CONTROL_BACKGROUND;
    SCROLLBAR_THUMB         = gui::mix_color( border, g_cfg.m_misc.m_accent, 35 );
    SCROLL_SHADOW           = alpha( black, 205 );
    TOOLTIP_BACKGROUND      = alpha( background, 250 );
    TOOLTIP_BORDER          = border;
    TOOLTIP_TEXT            = text;
    INPUT_BACKGROUND_FOCUS  = gui::mix_color( CONTROL_BACKGROUND, selection, 45 );
    INPUT_TEXT              = text;

    EDITOR_BACKGROUND   = background;
    EDITOR_BORDER       = border;
    EDITOR_GUTTER       = surface;
    EDITOR_GUTTER_TEXT  = dim;
    EDITOR_CURRENT_LINE = raised;
    EDITOR_SELECTION    = selection;
    EDITOR_ERROR_LINE   = gui::mix_color( error, background, 40 );
    EDITOR_CARET        = g_cfg.m_misc.m_accent;
    EDITOR_TEXT         = text;
    EDITOR_KEYWORD      = g_cfg.m_misc.m_accent;
    EDITOR_COMMENT      = dim;
    CONSOLE_INFO        = muted;
    CONSOLE_ERROR       = error;

    g_theme.set_accent( g_cfg.m_misc.m_accent );
    g_theme.m_nextgen_style = nextgen;
    g_theme.m_accent_end    = nextgen ? g_cfg.m_misc.m_accent_end : g_theme.m_accent_alt;
    s_applied_menu_style    = g_cfg.m_misc.m_menu_style;
}

static void generate_palette_from_accent( )
{
    const float hue{ gui::hue_of( g_cfg.m_misc.m_accent ) };
    color_t    *colors[]{ &g_cfg.m_misc.m_ui_background, &g_cfg.m_misc.m_ui_surface,
                          &g_cfg.m_misc.m_ui_raised, &g_cfg.m_misc.m_ui_border,
                          &g_cfg.m_misc.m_ui_text, &g_cfg.m_misc.m_ui_muted,
                          &g_cfg.m_misc.m_ui_dim, &g_cfg.m_misc.m_ui_hover,
                          &g_cfg.m_misc.m_ui_selection, &g_cfg.m_misc.m_ui_warning,
                          &g_cfg.m_misc.m_ui_error };
    for ( color_t *color : colors )
    {
        *color = gui::replace_hue( *color, hue );
    }
    apply_accent( );
}

static const char *const *chams_modes( )
{
    static const char *const MODES[]{
        ASTRAL_XORSTR_STATIC( "Off" ), ASTRAL_XORSTR_STATIC( "Flat" ),
        ASTRAL_XORSTR_STATIC( "Shaded" ), ASTRAL_XORSTR_STATIC( "Fresnel" ),
        ASTRAL_XORSTR_STATIC( "Wireframe" ), ASTRAL_XORSTR_STATIC( "Lava" ),
        ASTRAL_XORSTR_STATIC( "Pearl" ), ASTRAL_XORSTR_STATIC( "Bubble" ),
        ASTRAL_XORSTR_STATIC( "LSD" ), ASTRAL_XORSTR_STATIC( "Network" ),
        ASTRAL_XORSTR_STATIC( "Soap" ), ASTRAL_XORSTR_STATIC( "Aurora" ),
        ASTRAL_XORSTR_STATIC( "Aquarium" ), ASTRAL_XORSTR_STATIC( "Sky" ),
        ASTRAL_XORSTR_STATIC( "Textured" ), ASTRAL_XORSTR_STATIC( "Custom" )
    };

    return MODES;
}

struct custom_cham_slot_t
{
    gui::combo_box *m_combo{ };
    usize          *m_mode{ };
    char           *m_name{ };
    usize           m_name_capacity{ };
    usize           m_selection{ };
};

static custom_cham_slot_t s_custom_cham_slots[ 64 ]{ };
static usize              s_custom_cham_slot_count{ };

static gui::combo_box *add_custom_cham_slot( gui::group_box *group, usize *mode,
                                              char *name, usize name_capacity,
                                              bool ( *when )( ) )
{
    if ( s_custom_cham_slot_count >= 64 ) return nullptr;
    custom_cham_slot_t &slot{ s_custom_cham_slots[ s_custom_cham_slot_count++ ] };
    slot.m_mode          = mode;
    slot.m_name          = name;
    slot.m_name_capacity = name_capacity;
    slot.m_selection     = g_chams.custom_material_index( name );
    if ( slot.m_selection >= g_chams.custom_material_count( ) )
        slot.m_selection = g_chams.custom_material_index( );
    if ( slot.m_selection >= g_chams.custom_material_count( ) ) slot.m_selection = 0;
    slot.m_combo = group->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Custom material" ), g_chams.custom_material_labels( ),
        g_chams.custom_material_count( ), &slot.m_selection );
    slot.m_combo->set_render_on_index( mode, chams::CHAM_CUSTOM )->set_render_on( when )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Selects the custom VMT used only by the material slot directly above." ) );
    return slot.m_combo;
}

static void refresh_custom_cham_slots( )
{
    for ( usize index{ }; index < s_custom_cham_slot_count; ++index )
    {
        custom_cham_slot_t &slot{ s_custom_cham_slots[ index ] };
        if ( slot.m_combo )
            slot.m_combo->set_items( g_chams.custom_material_labels( ),
                                     g_chams.custom_material_count( ) );
        slot.m_selection = g_chams.custom_material_index( slot.m_name );
        if ( slot.m_selection >= g_chams.custom_material_count( ) )
            slot.m_selection = g_chams.custom_material_index( );
        if ( slot.m_selection >= g_chams.custom_material_count( ) ) slot.m_selection = 0;
    }
}

static void sync_custom_cham_slots( )
{
    if ( !g_chams.custom_material_count( ) ) return;
    for ( usize index{ }; index < s_custom_cham_slot_count; ++index )
    {
        custom_cham_slot_t &slot{ s_custom_cham_slots[ index ] };
        if ( !slot.m_mode || *slot.m_mode != chams::CHAM_CUSTOM ) continue;
        const char *selected{ g_chams.custom_material_name( slot.m_selection ) };
        if ( selected && string_view{ slot.m_name } != string_view{ selected } )
            str::copy( slot.m_name, slot.m_name_capacity, selected );
    }
}

static void add_cham_layers( gui::group_box *group, config::cham_profile_t &profile,
                             bool ( *when )( ) )
{
    const char *const *modes{ chams_modes( ) };

    group->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Visible" ), modes,
                                  chams::CHAM_COUNT, &profile.m_visible )->set_render_on( when );
    add_custom_cham_slot( group, &profile.m_visible, profile.m_visible_custom,
                          sizeof( profile.m_visible_custom ), when );
    group->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Visible overlay" ), modes,
                                  chams::CHAM_OVERLAY_COUNT, &profile.m_visible_overlay )->set_render_on( when );
    add_custom_cham_slot( group, &profile.m_visible_overlay, profile.m_visible_overlay_custom,
                          sizeof( profile.m_visible_overlay_custom ), when );
    group->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Invisible" ), modes,
                                  chams::CHAM_COUNT, &profile.m_invisible )->set_render_on( when );
    add_custom_cham_slot( group, &profile.m_invisible, profile.m_invisible_custom,
                          sizeof( profile.m_invisible_custom ), when );
    group->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Invisible overlay" ), modes,
                                  chams::CHAM_OVERLAY_COUNT, &profile.m_invisible_overlay )->set_render_on( when );
    add_custom_cham_slot( group, &profile.m_invisible_overlay, profile.m_invisible_overlay_custom,
                          sizeof( profile.m_invisible_overlay_custom ), when );
}

static void add_cham_tints( gui::group_box *group, config::cham_profile_t &profile,
                            bool ( *when )( ) )
{
    gui::color_picker *tints[]{
        group->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Visible tint" ),
                                         &profile.m_visible_color ),
        group->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Visible overlay tint" ),
                                         &profile.m_visible_overlay_color ),
        group->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Invisible tint" ),
                                         &profile.m_occluded_color ),
        group->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Invisible overlay tint" ),
                                         &profile.m_occluded_overlay_color )
    };

    for ( gui::color_picker *tint : tints )
    {
        tint->set_keywords( ASTRAL_XORSTR_STATIC( "chams colour color material" ) )
            ->set_render_on( when );
    }
}

static void add_glow_layers( gui::group_box *group, config::glow_profile_t &profile,
                             bool ( *when )( ) )
{
    gui::slider_int   *outline{ group->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Glow" ),
                                                               &profile.m_stencil, 0, 10 ) };
    gui::slider_int   *blur{ group->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Glow blur" ),
                                                            &profile.m_blur, 0, 10 ) };
    gui::color_picker *tint{ group->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Glow tint" ),
                                                              &profile.m_color ) };

    outline->set_keywords( ASTRAL_XORSTR_STATIC( "glow outline stencil halo" ) );
    blur->set_keywords( ASTRAL_XORSTR_STATIC( "glow blur bloom" ) );
    tint->set_keywords( ASTRAL_XORSTR_STATIC( "glow colour color tint" ) );

    outline->set_render_on( when );
    blur->set_render_on( when );
    tint->set_render_on( when );
}

static const char *path_low_confidence_text( )
{
    return ASTRAL_XORSTR_STATIC( "Low confidence" );
}

static bool has_been_in_game_once( )
{
    return g_cl.m_has_been_in_game_and_alive_once;
}

// Zero follows the live class profile. The remaining values map to profile + 1 and are deliberately
// menu-only: closing the menu restores Auto so this can never look like a runtime class override.
static usize s_ragebot_class_selector{ };

static bool ragebot_manual_class( )
{
    return s_ragebot_class_selector > 0 && s_ragebot_class_selector <= config::CFG_PROFILE_COUNT;
}

static usize ragebot_edited_profile( )
{
    return ragebot_manual_class( ) ? s_ragebot_class_selector - 1
                                   : static_cast< usize >( g_cl.m_target_config_profile );
}

static bool ragebot_context_available( )
{
    return ragebot_manual_class( ) || has_been_in_game_once( );
}

static bool is_able_to_configure_ragebot_option( )
{
    return ragebot_context_available( ) && g_cfg.m_ragebot.m_enabled;
}

static bool is_able_to_configure_melee( )
{
    return ragebot_context_available( ) && g_cfg.m_melee.m_enabled;
}

static bool ragebot_context_missing( )
{
    return !ragebot_context_available( );
}

static bool is_not_in_game( )
{
    return !g_cl.m_has_been_in_game_and_alive_once;
}

#define SET_ELEMENT( obj ) \
    obj->set_render_on( &is_able_to_configure_ragebot_option )

static bool is_class_being_edited( usize tgt )
{
    return ragebot_edited_profile( ) == tgt;
}

inline bool can_create_hitscan_profile( usize i )
{
    switch ( i )
    {
        case config::CFG_PROFILE_SCOUT:
        case config::CFG_PROFILE_SOLDIER:
        case config::CFG_PROFILE_PYRO:
        case config::CFG_PROFILE_HEAVY:
        case config::CFG_PROFILE_ENGINEER:
        case config::CFG_PROFILE_SNIPER:
        case config::CFG_PROFILE_SPY: return true;
        default: return false;
    }
}

inline bool can_create_proj_profile( usize i )
{
    switch ( i )
    {
        case config::CFG_PROFILE_SOLDIER:
        case config::CFG_PROFILE_PYRO:
        case config::CFG_PROFILE_DEMOMAN:
        case config::CFG_PROFILE_ENGINEER:
        case config::CFG_PROFILE_MEDIC:
        case config::CFG_PROFILE_SNIPER: return true;
        default: return false;
    }
}

template< usize tgt >
static bool is_hitscan_eligible( )
{
    if ( !is_able_to_configure_ragebot_option( ) )
    {
        return false;
    }

    if ( !can_create_hitscan_profile( tgt ) )
    {
        return false;
    }

    return is_class_being_edited( tgt );
}

template< usize tgt >
static bool is_projectile_eligible( )
{
    if ( !is_able_to_configure_ragebot_option( ) )
    {
        return false;
    }

    if ( !can_create_proj_profile( tgt ) )
    {
        return false;
    }

    return is_class_being_edited( tgt );
}

MAKE_PREDICATES( is_projectile_eligible, RAGEBOT_PROJECTILE_PREDICATES );
MAKE_PREDICATES( is_hitscan_eligible, RAGEBOT_HITSCAN_PREDICATES );

template< usize tgt >
static bool is_class_override_being_edited( )
{
    return is_able_to_configure_ragebot_option( ) && is_class_being_edited( tgt );
}

MAKE_PREDICATES( is_class_override_being_edited, RAGEBOT_CLASS_OVERRIDE_PREDICATES );

template< usize tgt >
static bool is_class_override_enabled( )
{
    return is_class_override_being_edited< tgt >( ) &&
           g_cfg.m_ragebot.m_class_aim_overrides[ tgt ].m_enabled;
}

MAKE_PREDICATES( is_class_override_enabled, RAGEBOT_CLASS_OVERRIDE_ENABLED_PREDICATES );

template< usize tgt >
static bool can_mp( )
{
    return is_hitscan_eligible< tgt >( ) && g_cfg.m_ragebot.m_hitscan_profiles[ tgt ].m_hitscan_multipoint;
}

template< usize tgt >
static bool can_splash( )
{
    return is_projectile_eligible< tgt >( ) && g_cfg.m_ragebot.m_projectile_profiles[ tgt ].m_projectile_splashbot;
}

template< usize tgt >
static bool can_projectile_mp( )
{
    return is_projectile_eligible< tgt >( ) && g_cfg.m_ragebot.m_projectile_profiles[ tgt ].m_projectile_multipoint;
}

MAKE_PREDICATES( can_mp, RAGEBOT_MULTIPOINT_PREDICATES );
MAKE_PREDICATES( can_splash, RAGEBOT_SPLASH_PREDICATES );
MAKE_PREDICATES( can_projectile_mp, RAGEBOT_PROJECTILE_MULTIPOINT_PREDICATES );

static bool hitscan_visuals_shown( )
{
    return ragebot_context_available( ) && can_create_hitscan_profile( ragebot_edited_profile( ) );
}

static bool projectile_visuals_shown( )
{
    return ragebot_context_available( ) && can_create_proj_profile( ragebot_edited_profile( ) );
}

static bool hitscan_duration_shown( )
{
    return hitscan_visuals_shown( ) && ( g_cfg.m_ragebot.m_hitscan_draw_hitboxes || g_cfg.m_ragebot.m_hitscan_draw_shot );
}

static bool target_path_shown( )
{
    return projectile_visuals_shown( ) && g_cfg.m_ragebot.m_projectile_draw_path;
}

static bool path_bounds_style_shown( )
{
    return target_path_shown( ) && g_cfg.m_ragebot.m_projectile_path_bounds;
}

static bool path_fade_time_shown( )
{
    return target_path_shown( ) && g_cfg.m_ragebot.m_projectile_path_fade && !g_cfg.m_ragebot.m_projectile_path_animate;
}

static bool shot_path_shown( )
{
    return projectile_visuals_shown( ) && g_cfg.m_projectile_path.m_enabled;
}

static bool shot_duration_shown( )
{
    return shot_path_shown( ) && !g_cfg.m_projectile_path.m_animate;
}

static bool trajectory_shown( )
{
    return projectile_visuals_shown( ) && g_cfg.m_trajectory.m_enabled;
}

#define SET_HITSCAN( obj, i ) \
    obj->set_render_on( RAGEBOT_HITSCAN_PREDICATES[ i ] )

#define SET_PROJECTILE( obj, i ) \
    obj->set_render_on( RAGEBOT_PROJECTILE_PREDICATES[ i ] )

static const char *in_game_warning( )
{
    return ASTRAL_XORSTR_STATIC( "You must be in game\nand alive to configure!" );
}

static const char *not_eligible_warning( )
{
    return ASTRAL_XORSTR_STATIC( "Nothing to configure for\nthis class!" );
}

static const char *ragebot_hitscan_label( usize idx )
{
    switch ( idx )
    {
        case config::CFG_PROFILE_SCOUT: return ASTRAL_XORSTR_STATIC( "Hitscan (Scout)" );
        case config::CFG_PROFILE_SOLDIER: return ASTRAL_XORSTR_STATIC( "Hitscan (Soldier)" );
        case config::CFG_PROFILE_PYRO: return ASTRAL_XORSTR_STATIC( "Hitscan (Pyro)" );
        case config::CFG_PROFILE_DEMOMAN: return ASTRAL_XORSTR_STATIC( "Hitscan (Demoman)" );
        case config::CFG_PROFILE_HEAVY: return ASTRAL_XORSTR_STATIC( "Hitscan (Heavy)" );
        case config::CFG_PROFILE_ENGINEER: return ASTRAL_XORSTR_STATIC( "Hitscan (Engineer)" );
        case config::CFG_PROFILE_MEDIC: return ASTRAL_XORSTR_STATIC( "Hitscan (Medic)" );
        case config::CFG_PROFILE_SNIPER: return ASTRAL_XORSTR_STATIC( "Hitscan (Sniper)" );
        case config::CFG_PROFILE_SPY: return ASTRAL_XORSTR_STATIC( "Hitscan (Spy)" );
        default: return ASTRAL_XORSTR_STATIC( "Should never see this..." );
    }
}

static const char *ragebot_projectile_label( usize idx )
{
    switch ( idx )
    {
        case config::CFG_PROFILE_SCOUT: return ASTRAL_XORSTR_STATIC( "Projectile (Scout)" );
        case config::CFG_PROFILE_SOLDIER: return ASTRAL_XORSTR_STATIC( "Projectile (Soldier)" );
        case config::CFG_PROFILE_PYRO: return ASTRAL_XORSTR_STATIC( "Projectile (Pyro)" );
        case config::CFG_PROFILE_DEMOMAN: return ASTRAL_XORSTR_STATIC( "Projectile (Demoman)" );
        case config::CFG_PROFILE_HEAVY: return ASTRAL_XORSTR_STATIC( "Projectile (Heavy)" );
        case config::CFG_PROFILE_ENGINEER: return ASTRAL_XORSTR_STATIC( "Projectile (Engineer)" );
        case config::CFG_PROFILE_MEDIC: return ASTRAL_XORSTR_STATIC( "Projectile (Medic)" );
        case config::CFG_PROFILE_SNIPER: return ASTRAL_XORSTR_STATIC( "Projectile (Sniper)" );
        case config::CFG_PROFILE_SPY: return ASTRAL_XORSTR_STATIC( "Projectile (Spy)" );
        default: return ASTRAL_XORSTR_STATIC( "Should never see this..." );
    }
}

static void build_ragebot_visuals( gui::sub_tab *visuals )
{
    static const char *const PATH_STYLES[]{ ASTRAL_XORSTR_STATIC( "Line" ), ASTRAL_XORSTR_STATIC( "Separators" ),
                                            ASTRAL_XORSTR_STATIC( "Outlined" ), ASTRAL_XORSTR_STATIC( "Timeline" ),
                                            ASTRAL_XORSTR_STATIC( "Comet" ), ASTRAL_XORSTR_STATIC( "Flow" ) };

    static const char *const TRAJECTORY_STYLES[]{ ASTRAL_XORSTR_STATIC( "Line" ), ASTRAL_XORSTR_STATIC( "Separators" ),
                                                  ASTRAL_XORSTR_STATIC( "Timeline" ), ASTRAL_XORSTR_STATIC( "Comet" ),
                                                  ASTRAL_XORSTR_STATIC( "Flow" ) };

    static const char *const PATH_BOUNDS_STYLES[]{ ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Corners" ) };

    gui::group_box *ragebot{ visuals->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Ragebot" ), gui::column_t::left ) };
    gui::group_box *hitscan{ visuals->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Hitscan" ), gui::column_t::left ) };
    gui::group_box *trajectory{ visuals->add_group( ICON_RULER, ASTRAL_XORSTR_STATIC( "Trajectory" ), gui::column_t::left ) };
    gui::group_box *target_path{ visuals->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Target path" ), gui::column_t::right ) };
    gui::group_box *shot_path{ visuals->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Shot path" ), gui::column_t::right ) };

    config::ragebot_t         &rage{ g_cfg.m_ragebot };
    config::projectile_path_t &shot{ g_cfg.m_projectile_path };
    config::trajectory_t      &path{ g_cfg.m_trajectory };

    gui::label    *notice{ ragebot->add< gui::label >( ASTRAL_XORSTR_STATIC( "notice" ), &in_game_warning, 2 ) };
    gui::checkbox *draw_fov{ ragebot->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Draw field of view" ), &rage.m_draw_fov ) };
    gui::checkbox *draw_crosshair{ ragebot->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Crosshair follows aim" ), &rage.m_draw_crosshair ) };

    notice->set_render_on( &ragebot_context_missing );

    draw_fov->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the ragebot's field of view as a circle on screen." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fov cone circle radius" ) )
        ->set_render_on( &ragebot_context_available )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Cone" ), &rage.m_fov_color );

    draw_crosshair->set_tooltip( ASTRAL_XORSTR_STATIC( "Moves the game's own crosshair onto the point the ragebot is shooting at, "
                                                      "instead of leaving it on the camera." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "aim crosshair silent psilent marker hud" ) )
        ->set_render_on( &ragebot_context_available );

    gui::checkbox     *draw_hitboxes{ hitscan->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Hitboxes" ), &rage.m_hitscan_draw_hitboxes ) };
    gui::checkbox     *draw_shot{ hitscan->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Shoot position" ), &rage.m_hitscan_draw_shot ) };
    gui::slider_float *draw_duration{ hitscan->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Duration" ), &rage.m_hitscan_draw_duration,
                                                                         0.2f, 5.0f, 1, ASTRAL_XORSTR_STATIC( "s" ) ) };

    draw_hitboxes->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the hitboxes of the pose the last shot was placed against, the aimed one "
                                                     "in its own color." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "hitboxes draw shot pose debug" ) )
        ->set_render_on( &hitscan_visuals_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Hitboxes" ), &rage.m_hitscan_hitbox_color );
    draw_hitboxes->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Target" ), &rage.m_hitscan_target_color );

    draw_shot->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the line from the shoot position to the point the last shot was sent at." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shoot position line aim point debug" ) )
        ->set_render_on( &hitscan_visuals_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Shot" ), &rage.m_hitscan_shot_color );

    draw_duration->set_tooltip( ASTRAL_XORSTR_STATIC( "Seconds the last shot's overlay takes to fade out." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "duration fade overlay" ) )
        ->set_render_on( &hitscan_duration_shown );

    gui::checkbox     *preview{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Trajectory preview" ), &path.m_enabled ) };
    gui::combo_box    *preview_style{ trajectory->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Line style" ), TRAJECTORY_STYLES,
                                                                         config::TRAJECTORY_STYLE_COUNT, &path.m_style ) };
    gui::checkbox     *preview_outline{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Outline" ), &path.m_outline ) };
    gui::checkbox     *preview_gradient{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Gradient" ), &path.m_gradient ) };
    gui::checkbox     *preview_occluded{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Dim behind walls" ), &path.m_dim_occluded ) };
    gui::color_picker *preview_markers{ trajectory->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Markers" ), &path.m_marker_color, false ) };
    gui::checkbox     *preview_splash{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Splash radius" ), &path.m_splash ) };
    gui::checkbox     *preview_box{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Impact box" ), &path.m_impact_box ) };
    gui::checkbox     *preview_time{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Flight time" ), &path.m_flight_time ) };
    gui::checkbox     *preview_bounces{ trajectory->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Bounce prediction" ), &path.m_bounce_prediction ) };

    preview->set_tooltip( ASTRAL_XORSTR_STATIC( "While you hold a projectile weapon, draws where a shot fired now would go and "
                                               "land, so the aim can be set before shooting. Works without the ragebot." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "trajectory preview path projectile arc landing" ) )
        ->set_render_on( &projectile_visuals_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Path" ), &path.m_color, false );

    preview_style->set_render_on( &trajectory_shown );
    preview_outline->set_render_on( &trajectory_shown );
    preview_markers->set_render_on( &trajectory_shown );

    preview_gradient->set_tooltip( ASTRAL_XORSTR_STATIC( "Blends the path from its colour at the muzzle to the gradient colour at the landing." ) )
        ->set_render_on( &trajectory_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Gradient end" ), &path.m_gradient_color, false );

    preview_occluded->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the stretches your view cannot see, behind walls, faded." ) )
        ->set_render_on( &trajectory_shown );

    preview_splash->set_tooltip( ASTRAL_XORSTR_STATIC( "The weapon's blast radius on the surface it lands on, cut short by walls "
                                                      "the explosion cannot see past." ) )
        ->set_render_on( &trajectory_shown );

    preview_box->set_tooltip( ASTRAL_XORSTR_STATIC( "The projectile's collision box where it lands, at its real size." ) )
        ->set_render_on( &trajectory_shown );

    preview_time->set_tooltip( ASTRAL_XORSTR_STATIC( "Seconds from the shot to the landing, next to it." ) )
        ->set_render_on( &trajectory_shown );

    preview_bounces->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts one post-bounce segment for projectiles that retain velocity. Iron Bomber pills stop at contact and Loch-n-Load pills detonate. The continuation is only truly accurate when the pill has no rotation." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "bounce ricochet grenade cannon fuse trajectory" ) )
        ->set_render_on( &trajectory_shown );

    gui::checkbox     *proj_path{ target_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Target path" ), &rage.m_projectile_draw_path ) };
    gui::label        *path_low_confidence{ target_path->add< gui::label >( ASTRAL_XORSTR_STATIC( "Low confidence" ), &path_low_confidence_text ) };
    gui::combo_box    *path_style{ target_path->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Style" ), PATH_STYLES, config::PATH_STYLE_COUNT,
                                                                       &rage.m_projectile_path_style ) };
    gui::checkbox     *path_outline{ target_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Outline" ), &rage.m_projectile_path_outline ) };
    gui::checkbox     *path_gradient{ target_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Gradient" ), &rage.m_projectile_path_gradient ) };
    gui::checkbox     *path_bounds{ target_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Bounds" ), &rage.m_projectile_path_bounds ) };
    gui::combo_box    *bounds_style{ target_path->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Bounds style" ), PATH_BOUNDS_STYLES,
                                                                         config::PATH_BOUNDS_COUNT, &rage.m_projectile_path_bounds_style ) };
    gui::checkbox     *path_fade{ target_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Fade" ), &rage.m_projectile_path_fade ) };
    gui::slider_float *path_fade_time{ target_path->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Fade time" ), &rage.m_projectile_path_fade_time,
                                                                              0.2f, 3.0f, 1, ASTRAL_XORSTR_STATIC( "s" ) ) };
    gui::checkbox     *path_animate{ target_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Travel animation" ), &rage.m_projectile_path_animate ) };

    proj_path->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the line the movement simulation walks the target along for the shot that "
                                                 "was taken." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "target path prediction line movement player" ) )
        ->set_render_on( &projectile_visuals_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Path" ), &rage.m_projectile_path_color );

    path_low_confidence->set_tooltip( ASTRAL_XORSTR_STATIC( "Color and opacity of the uncertain part of the target path and its endpoint bounds." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path low confidence uncertainty color opacity" ) )
        ->set_render_on( &target_path_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Low confidence" ), &rage.m_projectile_path_low_confidence_color );

    path_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Separators extend to one side of the line every few ticks, showing the target's pace. "
                                                  "Timeline marks every quarter second on both sides. Comet and Flow animate along the line." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path style separators ties rails timeline ruler comet flow arrows" ) )
        ->set_render_on( &target_path_shown );

    path_outline->set_tooltip( ASTRAL_XORSTR_STATIC( "Borders the path and its style markers with a dark line so it stays readable "
                                                    "against bright surfaces." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path outline outlined border shadow" ) )
        ->set_render_on( &target_path_shown );

    path_gradient->set_tooltip( ASTRAL_XORSTR_STATIC( "Blends the line from the path color to the end color along its length." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path gradient color blend" ) )
        ->set_render_on( &target_path_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "End" ), &rage.m_projectile_path_color_end );

    path_bounds->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the target's predicted bounding box where the line ends, the box the shot "
                                                   "was actually solved against." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "bounds box hitbox predicted end" ) )
        ->set_render_on( &target_path_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Bounds" ), &rage.m_projectile_path_bounds_color );

    bounds_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Box draws all twelve edges; corners draws only the stubs, which covers less of "
                                                    "the target." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "bounds style box corners" ) )
        ->set_render_on( &path_bounds_style_shown );

    path_fade->set_tooltip( ASTRAL_XORSTR_STATIC( "Eats the line behind the target as their simulation time passes each point, and "
                                                 "fades the whole line out once the bot stops solving instead of leaving it painted "
                                                 "on the world." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path fade clear simulation time" ) )
        ->set_render_on( &target_path_shown );

    path_fade_time->set_tooltip( ASTRAL_XORSTR_STATIC( "Seconds a walked-past or stale point takes to vanish." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path fade time duration" ) )
        ->set_render_on( &path_fade_time_shown );

    path_animate->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the full target path, then erases it from the start toward the endpoint as "
                                                    "travel time advances." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "path travel animate fade movement time" ) )
        ->set_render_on( &target_path_shown );

    gui::checkbox     *shot_enabled{ shot_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &shot.m_enabled ) };
    gui::combo_box    *shot_style{ shot_path->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Style" ), PATH_STYLES, config::PATH_STYLE_COUNT,
                                                                     &shot.m_style ) };
    gui::checkbox     *shot_outline{ shot_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Outline" ), &shot.m_outline ) };
    gui::checkbox     *shot_impact{ shot_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Impact box" ), &shot.m_impact_box ) };
    gui::slider_float *shot_duration{ shot_path->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Duration" ), &shot.m_duration, 0.2f, 5.0f, 1,
                                                                           ASTRAL_XORSTR_STATIC( "s" ) ) };
    gui::checkbox     *shot_animate{ shot_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Travel animation" ), &shot.m_animate ) };
    gui::checkbox     *shot_bounces{ shot_path->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Bounce prediction" ), &shot.m_bounce_prediction ) };

    shot_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows the predicted flight of your latest projectile shot until it hits the world, "
                                                    "meets the target an ragebot shot was solved against, or its lifetime runs out. Works "
                                                    "without the ragebot." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shot projectile trajectory path flight" ) )
        ->set_render_on( &projectile_visuals_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Path" ), &shot.m_color );

    shot_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Separators extend to one side every four ticks of flight. Timeline marks every quarter "
                                                  "second on both sides. Comet and Flow animate along the flight." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shot path style separators timeline ruler comet flow arrows" ) )
        ->set_render_on( &shot_path_shown );

    shot_outline->set_tooltip( ASTRAL_XORSTR_STATIC( "Borders the flight path and its style markers with a dark line so it stays readable "
                                                    "against bright surfaces." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shot path outline outlined border shadow" ) )
        ->set_render_on( &shot_path_shown );

    shot_impact->set_tooltip( ASTRAL_XORSTR_STATIC( "Marks where the path ends on a hit with a small cube, square on the struck surface "
                                                   "or facing the shot on a target." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shot impact box hit end marker" ) )
        ->set_render_on( &shot_path_shown )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Impact" ), &shot.m_impact_color );

    shot_duration->set_tooltip( ASTRAL_XORSTR_STATIC( "Seconds the last shot's path takes to fade out." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shot path duration fade" ) )
        ->set_render_on( &shot_duration_shown );

    shot_animate->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the full projectile path, then erases it from the start toward the endpoint as "
                                                    "the projectile travels." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shot projectile path travel animate fade time" ) )
        ->set_render_on( &shot_path_shown );

    shot_bounces->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts one post-bounce segment without resetting the fuse. Weapon-specific responses are respected. The continuation is only truly accurate when the pill has no rotation." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "bounce ricochet grenade cannon fuse shot path" ) )
        ->set_render_on( &shot_path_shown );
}

static void build_hitscan_trigger( gui::group_box *trigger )
{
    static const char *const TARGETS[]{
        ASTRAL_XORSTR_STATIC( "Players" ), ASTRAL_XORSTR_STATIC( "Sentry guns" ),
        ASTRAL_XORSTR_STATIC( "Dispensers" ), ASTRAL_XORSTR_STATIC( "Teleporters" ),
    };
    static const char *const IGNORES[]{
        ASTRAL_XORSTR_STATIC( "Cloaked" ), ASTRAL_XORSTR_STATIC( "Disguised" ),
        ASTRAL_XORSTR_STATIC( "Feigned" ), ASTRAL_XORSTR_STATIC( "Ubered" ),
        ASTRAL_XORSTR_STATIC( "Bonked" ), ASTRAL_XORSTR_STATIC( "Taunting" ),
        ASTRAL_XORSTR_STATIC( "Vaccinator" ), ASTRAL_XORSTR_STATIC( "Friends" ),
        ASTRAL_XORSTR_STATIC( "Astral users" ), ASTRAL_XORSTR_STATIC( "Party members" ),
    };
    static const char *const HITBOXES[]{
        ASTRAL_XORSTR_STATIC( "Pelvis" ), ASTRAL_XORSTR_STATIC( "Stomach" ),
        ASTRAL_XORSTR_STATIC( "Lower chest" ), ASTRAL_XORSTR_STATIC( "Chest" ),
        ASTRAL_XORSTR_STATIC( "Upper chest" ), ASTRAL_XORSTR_STATIC( "Head" ),
        ASTRAL_XORSTR_STATIC( "Arms" ), ASTRAL_XORSTR_STATIC( "Legs" ),
    };

    auto enabled{ trigger->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_triggerbot.m_enabled ) };
    auto targets{ trigger->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Targets" ), TARGETS, 4,
                                                   g_cfg.m_triggerbot.m_targets ) };
    auto hitboxes{ trigger->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Hitboxes" ), HITBOXES,
                                                    config::HITSCAN_COUNT, g_cfg.m_triggerbot.m_hitboxes ) };
    auto ignores{ trigger->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Ignore filters" ), IGNORES,
                                                   config::IGNORE_COUNT, g_cfg.m_triggerbot.m_ignores ) };

    enabled->add_module< gui::keybind >( string_view{ }, &g_cfg.m_triggerbot.m_key );
    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Fires the moment the crosshair covers a valid hitscan target. With no key bound it runs constantly." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "triggerbot hitscan trigger auto shoot fire click" ) );
    targets->set_render_on( &g_cfg.m_triggerbot.m_enabled );
    hitboxes->set_render_on( &g_cfg.m_triggerbot.m_enabled );
    ignores->set_render_on( &g_cfg.m_triggerbot.m_enabled );
}

static void build_ragebot( )
{
    static const char *const CONFIG_CLASSES[]{
        ASTRAL_XORSTR_STATIC( "Auto" ),
        ASTRAL_XORSTR_STATIC( "Scout" ),
        ASTRAL_XORSTR_STATIC( "Soldier" ),
        ASTRAL_XORSTR_STATIC( "Pyro" ),
        ASTRAL_XORSTR_STATIC( "Demoman" ),
        ASTRAL_XORSTR_STATIC( "Heavy" ),
        ASTRAL_XORSTR_STATIC( "Engineer" ),
        ASTRAL_XORSTR_STATIC( "Medic" ),
        ASTRAL_XORSTR_STATIC( "Sniper" ),
        ASTRAL_XORSTR_STATIC( "Spy" ),
    };

    static const char *const TARGET_OPTIONS[]{
        ASTRAL_XORSTR_STATIC( "Players" ),
        ASTRAL_XORSTR_STATIC( "Sentry guns" ),
        ASTRAL_XORSTR_STATIC( "Dispensers" ),
        ASTRAL_XORSTR_STATIC( "Teleporters" ),
        ASTRAL_XORSTR_STATIC( "Projectiles" ),
        ASTRAL_XORSTR_STATIC( "Entities" ),
    };

    static const char *const IGNORE_OPTIONS[]{
        ASTRAL_XORSTR_STATIC( "Cloaked" ),
        ASTRAL_XORSTR_STATIC( "Disguised" ),
        ASTRAL_XORSTR_STATIC( "Feigned" ),
        ASTRAL_XORSTR_STATIC( "Ubered" ),
        ASTRAL_XORSTR_STATIC( "Bonked" ),
        ASTRAL_XORSTR_STATIC( "Taunting" ),
        ASTRAL_XORSTR_STATIC( "Vaccinator" ),
        ASTRAL_XORSTR_STATIC( "Friends" ),
        ASTRAL_XORSTR_STATIC( "Astral users" ),
        ASTRAL_XORSTR_STATIC( "Party members" ),
    };

    static const char *const RAGEBOT_PREFERENCES[]{
        ASTRAL_XORSTR_STATIC( "Auto-charge" ),
        ASTRAL_XORSTR_STATIC( "Auto-rev" ),
        ASTRAL_XORSTR_STATIC( "Auto-scope" ),
        ASTRAL_XORSTR_STATIC( "Natascha aim at bonked" ),
        ASTRAL_XORSTR_STATIC( "Allow friendly fire" ),
        ASTRAL_XORSTR_STATIC( "Sniper wait for charge" ),
        ASTRAL_XORSTR_STATIC( "Target dormant" ),
    };

    static const char *const TARGET_CLASSES[]{
        ASTRAL_XORSTR_STATIC( "Scout" ),
        ASTRAL_XORSTR_STATIC( "Soldier" ),
        ASTRAL_XORSTR_STATIC( "Pyro" ),
        ASTRAL_XORSTR_STATIC( "Demoman" ),
        ASTRAL_XORSTR_STATIC( "Heavy" ),
        ASTRAL_XORSTR_STATIC( "Engineer" ),
        ASTRAL_XORSTR_STATIC( "Medic" ),
        ASTRAL_XORSTR_STATIC( "Sniper" ),
        ASTRAL_XORSTR_STATIC( "Spy" )
    };

    static const char *const SPLASH_QUALITIES[]{
        ASTRAL_XORSTR_STATIC( "Low" ),
        ASTRAL_XORSTR_STATIC( "Medium" ),
        ASTRAL_XORSTR_STATIC( "High" )
    };

    static const char *const AIM_TYPES[]{
        ASTRAL_XORSTR_STATIC( "Plain" ),
        ASTRAL_XORSTR_STATIC( "Smooth" ),
        ASTRAL_XORSTR_STATIC( "Silent" ),
        ASTRAL_XORSTR_STATIC( "Locking" ),
        ASTRAL_XORSTR_STATIC( "Assistive" ),
        ASTRAL_XORSTR_STATIC( "Legit" ),
        ASTRAL_XORSTR_STATIC( "Smooth velocity" ),
        ASTRAL_XORSTR_STATIC( "Humanized" ),
    };
    static const char *const OPTIONAL_AIM_TYPES[]{
        ASTRAL_XORSTR_STATIC( "Plain" ),
        ASTRAL_XORSTR_STATIC( "Smooth" ),
        ASTRAL_XORSTR_STATIC( "Silent" ),
        ASTRAL_XORSTR_STATIC( "Locking" ),
        ASTRAL_XORSTR_STATIC( "Assistive" ),
        ASTRAL_XORSTR_STATIC( "Legit" ),
        ASTRAL_XORSTR_STATIC( "Smooth velocity" ),
        ASTRAL_XORSTR_STATIC( "Humanized" ),
        ASTRAL_XORSTR_STATIC( "Inherit" ),
    };
    static const char *const MELEE_AIM_TYPES[]{
        ASTRAL_XORSTR_STATIC( "Plain" ), ASTRAL_XORSTR_STATIC( "Smooth" ),
        ASTRAL_XORSTR_STATIC( "Silent" ), ASTRAL_XORSTR_STATIC( "Locking" ),
        ASTRAL_XORSTR_STATIC( "Assistive" ), ASTRAL_XORSTR_STATIC( "Legit" ),
        ASTRAL_XORSTR_STATIC( "Smooth velocity" ), ASTRAL_XORSTR_STATIC( "Humanized" ),
    };

    static const char *const SMOOTH_CURVES[]{
        ASTRAL_XORSTR_STATIC( "Linear" ),
        ASTRAL_XORSTR_STATIC( "Fast start" ),
        ASTRAL_XORSTR_STATIC( "Fast end" ),
        ASTRAL_XORSTR_STATIC( "Slow start" ),
        ASTRAL_XORSTR_STATIC( "Slow end" ),
    };

    static const char *const PRIORITY_AIMPOINTS[]{
        ASTRAL_XORSTR_STATIC( "Pelvis" ),
        ASTRAL_XORSTR_STATIC( "Stomach" ),
        ASTRAL_XORSTR_STATIC( "Lower chest" ),
        ASTRAL_XORSTR_STATIC( "Chest" ),
        ASTRAL_XORSTR_STATIC( "Upper chest" ),
        ASTRAL_XORSTR_STATIC( "Head" ),
        ASTRAL_XORSTR_STATIC( "Auto" ),
    };

    static const char *const HITSCAN_AIMPOINTS[]{
        ASTRAL_XORSTR_STATIC( "Pelvis" ),
        ASTRAL_XORSTR_STATIC( "Stomach" ),
        ASTRAL_XORSTR_STATIC( "Lower chest" ),
        ASTRAL_XORSTR_STATIC( "Chest" ),
        ASTRAL_XORSTR_STATIC( "Upper chest" ),
        ASTRAL_XORSTR_STATIC( "Head" ),
        ASTRAL_XORSTR_STATIC( "Arms" ),
        ASTRAL_XORSTR_STATIC( "Legs" ),
    };

    static const char *const PROJECTILE_AIMPOINTS[]{
        ASTRAL_XORSTR_STATIC( "Top" ),
        ASTRAL_XORSTR_STATIC( "Middle" ),
        ASTRAL_XORSTR_STATIC( "Bottom" ),
        ASTRAL_XORSTR_STATIC( "Auto" ),
    };

    static const char *const SPLASHBOT_MODES[]{
        ASTRAL_XORSTR_STATIC( "Include" ),
        ASTRAL_XORSTR_STATIC( "Prefer" ),
    };

    static const char *const POSITION_MODES[]{
        ASTRAL_XORSTR_STATIC( "Original" ),
        ASTRAL_XORSTR_STATIC( "Predicted" ),
    };

    static const char *const AUTO_CHARGE_TARGETS[]{
        ASTRAL_XORSTR_STATIC( "Stickybomb Launcher" ),
        ASTRAL_XORSTR_STATIC( "Scottish Resistance" ),
        ASTRAL_XORSTR_STATIC( "Quickiebomb Launcher" ),
    };

    auto aim{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Ragebot" ), ICON_CROSSHAIR ) };
    auto general{ aim->add_subtab( ASTRAL_XORSTR_STATIC( "General" ) ) };
    auto visuals{ aim->add_subtab( ASTRAL_XORSTR_STATIC( "Visuals" ) ) };

    auto global{ general->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Global" ), gui::column_t::left ) };
    auto melee{ general->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Melee (Global)" ), gui::column_t::left ) };
    auto conditional_aim{ general->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Conditional aim types" ), gui::column_t::right ) };
    auto hitscan_trigger{ general->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Hitscan Trigger" ), gui::column_t::right ) };
    build_hitscan_trigger( hitscan_trigger );

    auto aim_rings{ conditional_aim->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enable Rings" ),
                                                            &g_cfg.m_ragebot.m_aim_rings ) };
    auto inner_fov{ conditional_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Inner FoV" ),
                                                              &g_cfg.m_ragebot.m_aim_ring_inner_fov,
                                                              0, 180, ASTRAL_XORSTR_STATIC( "deg" ) ) };
    auto inner_type{ conditional_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Inner aim type" ),
                                                              OPTIONAL_AIM_TYPES,
                                                              config::ragebot_t::AIM_TYPE_COUNT + 1,
                                                              &g_cfg.m_ragebot.m_aim_ring_inner_type ) };
    auto outer_fov{ conditional_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Outer FoV" ),
                                                              &g_cfg.m_ragebot.m_aim_ring_outer_fov,
                                                              1, 180, ASTRAL_XORSTR_STATIC( "deg" ) ) };
    auto outer_type{ conditional_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Outer aim type" ),
                                                              OPTIONAL_AIM_TYPES,
                                                              config::ragebot_t::AIM_TYPE_COUNT + 1,
                                                              &g_cfg.m_ragebot.m_aim_ring_outer_type ) };
    auto spectator_types{ conditional_aim->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Enable Spectator AimTypes" ),
        &g_cfg.m_ragebot.m_spectator_aim_types ) };
    auto spectator_third{ conditional_aim->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Spectated 3rd person" ), OPTIONAL_AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT + 1,
        &g_cfg.m_ragebot.m_spectator_thirdperson_type ) };
    auto spectator_first{ conditional_aim->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Spectated 1st person" ), OPTIONAL_AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT + 1,
        &g_cfg.m_ragebot.m_spectator_firstperson_type ) };
    auto inner_third{ conditional_aim->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Inner spectated 3rd" ), OPTIONAL_AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT + 1,
        &g_cfg.m_ragebot.m_aim_ring_inner_thirdperson_type ) };
    auto inner_first{ conditional_aim->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Inner spectated 1st" ), OPTIONAL_AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT + 1,
        &g_cfg.m_ragebot.m_aim_ring_inner_firstperson_type ) };
    auto outer_third{ conditional_aim->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Outer spectated 3rd" ), OPTIONAL_AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT + 1,
        &g_cfg.m_ragebot.m_aim_ring_outer_thirdperson_type ) };
    auto outer_first{ conditional_aim->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Outer spectated 1st" ), OPTIONAL_AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT + 1,
        &g_cfg.m_ragebot.m_aim_ring_outer_firstperson_type ) };

    static const auto rings_and_spectators = ( bool ( * )( ) )( [ ]
    {
        return g_cfg.m_ragebot.m_aim_rings && g_cfg.m_ragebot.m_spectator_aim_types;
    } );
    inner_fov->set_render_on( &g_cfg.m_ragebot.m_aim_rings );
    inner_type->set_render_on( &g_cfg.m_ragebot.m_aim_rings );
    outer_fov->set_render_on( &g_cfg.m_ragebot.m_aim_rings );
    outer_type->set_render_on( &g_cfg.m_ragebot.m_aim_rings );
    spectator_third->set_render_on( &g_cfg.m_ragebot.m_spectator_aim_types );
    spectator_first->set_render_on( &g_cfg.m_ragebot.m_spectator_aim_types );
    inner_third->set_render_on( rings_and_spectators );
    inner_first->set_render_on( rings_and_spectators );
    outer_third->set_render_on( rings_and_spectators );
    outer_first->set_render_on( rings_and_spectators );

    aim_rings->set_tooltip( ASTRAL_XORSTR_STATIC( "Selects an aim type from at most two angular zones around the current target. Outside the outer ring uses the regular Aim type." ) );
    spectator_types->set_tooltip( ASTRAL_XORSTR_STATIC( "Overrides aim type for valid first- or third-person spectators watching you. Deathcams and freezecams are ignored." ) );

    auto config_class{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Configure class" ), CONFIG_CLASSES,
                                                       config::CFG_PROFILE_COUNT + 1, &s_ragebot_class_selector ) };
    config_class->set_tooltip( ASTRAL_XORSTR_STATIC( "Auto follows your current in-game class. A manual choice only changes which class profile this menu edits and resets to Auto when the menu closes." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "configure class profile manual auto offline" ) );

    auto notice{ global->add< gui::label >( ASTRAL_XORSTR_STATIC( "notice" ), &in_game_warning, 2 ) };
    notice->set_render_on( &ragebot_context_missing );

    auto enabled{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_ragebot.m_enabled ) };
    enabled->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_ragebot.m_activate_key, true );
    enabled->set_render_on( &ragebot_context_available );

    auto auto_shoot{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-shoot" ), &g_cfg.m_ragebot.m_auto_shoot ) };
    auto_shoot->set_tooltip( ASTRAL_XORSTR_STATIC( "Let the ragebot automatically shoot when a target is found and states are optimal." ) );
    SET_ELEMENT( auto_shoot );

    auto projectile_trigger{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Projectile trigger" ), &g_cfg.m_ragebot.m_projectile_trigger ) };
    projectile_trigger->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_ragebot.m_projectile_trigger_key );
    projectile_trigger->set_tooltip( ASTRAL_XORSTR_STATIC( "Runs projectile prediction without steering your aim. Fires after the predicted aim point remains inside the trigger circle for the configured delay." ) );

    auto projectile_trigger_radius{ global->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Trigger circle size" ), &g_cfg.m_ragebot.m_projectile_trigger_radius, 4, 240, ASTRAL_XORSTR_STATIC( "px" ) ) };
    projectile_trigger_radius->set_render_on( &g_cfg.m_ragebot.m_projectile_trigger );

    auto projectile_trigger_delay{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Trigger delay" ),
        &g_cfg.m_ragebot.m_projectile_trigger_delay_min, &g_cfg.m_ragebot.m_projectile_trigger_delay_max, 0, 1000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    projectile_trigger_delay->set_render_on( &g_cfg.m_ragebot.m_projectile_trigger );

    auto projectile_trigger_color{ global->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Trigger circle" ), &g_cfg.m_ragebot.m_projectile_trigger_color ) };
    projectile_trigger_color->set_render_on( &g_cfg.m_ragebot.m_projectile_trigger );

    auto projectile_trigger_active_color{ global->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Trigger active" ), &g_cfg.m_ragebot.m_projectile_trigger_active_color ) };
    projectile_trigger_active_color->set_render_on( &g_cfg.m_ragebot.m_projectile_trigger );

    static const auto ragebot_on_attack = ( bool ( * )( ) )( [ ]
    {
        return is_able_to_configure_ragebot_option( ) &&
               g_cfg.m_ragebot.m_activate_key.m_mode == bind_mode_t::on_attack;
    } );
    auto delay_shot{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Delay shot" ), &g_cfg.m_ragebot.m_delay_shot ) };
    delay_shot->set_tooltip( ASTRAL_XORSTR_STATIC( "In On attack mode, holds the requested shot until the aimbot has reached the target and all firing checks pass." ) );
    delay_shot->set_render_on( ragebot_on_attack );

    auto manual_backtrack{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Backtrack manual shots" ), &g_cfg.m_ragebot.m_manual_backtrack ) };
    manual_backtrack->set_tooltip( ASTRAL_XORSTR_STATIC( "Rewinds a manually aimed hitscan shot to the valid history record beneath your crosshair. It does not move your view or fire for you." ) );
    SET_ELEMENT( manual_backtrack );

    auto backtrack_crosshair{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Backtrack to crosshair" ), &g_cfg.m_ragebot.m_backtrack_crosshair ) };
    backtrack_crosshair->set_tooltip( ASTRAL_XORSTR_STATIC( "Makes automated hitscan targeting prioritize valid historical poses nearest your crosshair instead of scanning history newest-first." ) );
    SET_ELEMENT( backtrack_crosshair );

    auto aim_type{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Aim type" ), AIM_TYPES, config::ragebot_t::AIM_TYPE_COUNT, &g_cfg.m_ragebot.m_aim_type ) };
    aim_type->set_tooltip( ASTRAL_XORSTR_STATIC( "Controls how solved aim angles are applied to your view and outgoing command." ) );
    SET_ELEMENT( aim_type );

    static const auto ragebot_smooth_aim = ( bool ( * )( ) )( [ ]
    {
        const usize types[]{
            g_cfg.m_ragebot.m_aim_type,
            g_cfg.m_ragebot.m_aim_ring_inner_type,
            g_cfg.m_ragebot.m_aim_ring_inner_thirdperson_type,
            g_cfg.m_ragebot.m_aim_ring_inner_firstperson_type,
            g_cfg.m_ragebot.m_aim_ring_outer_type,
            g_cfg.m_ragebot.m_aim_ring_outer_thirdperson_type,
            g_cfg.m_ragebot.m_aim_ring_outer_firstperson_type,
            g_cfg.m_ragebot.m_spectator_thirdperson_type,
            g_cfg.m_ragebot.m_spectator_firstperson_type,
        };
        for ( usize type : types )
        {
            if ( type == config::ragebot_t::AIM_SMOOTH || type == config::ragebot_t::AIM_ASSISTIVE ||
                 type == config::ragebot_t::AIM_LEGIT || type == config::ragebot_t::AIM_SMOOTH_VELOCITY ||
                 type == config::ragebot_t::AIM_HUMANIZED ) return true;
        }
        return false;
    } );
    static const auto ragebot_strength_aim = ( bool ( * )( ) )( [ ]
    {
        return is_able_to_configure_ragebot_option( ) && ragebot_smooth_aim( );
    } );
    static const auto ragebot_smooth_curve_shown = ( bool ( * )( ) )( [ ]
    {
        return is_able_to_configure_ragebot_option( ) && ragebot_smooth_aim( );
    } );
    static const auto ragebot_humanized_aim = ( bool ( * )( ) )( [ ]
    {
        if ( !is_able_to_configure_ragebot_option( ) ) return false;
        const usize types[]{
            g_cfg.m_ragebot.m_aim_type,
            g_cfg.m_ragebot.m_aim_ring_inner_type,
            g_cfg.m_ragebot.m_aim_ring_inner_thirdperson_type,
            g_cfg.m_ragebot.m_aim_ring_inner_firstperson_type,
            g_cfg.m_ragebot.m_aim_ring_outer_type,
            g_cfg.m_ragebot.m_aim_ring_outer_thirdperson_type,
            g_cfg.m_ragebot.m_aim_ring_outer_firstperson_type,
            g_cfg.m_ragebot.m_spectator_thirdperson_type,
            g_cfg.m_ragebot.m_spectator_firstperson_type,
        };
        for ( usize type : types )
            if ( type == config::ragebot_t::AIM_HUMANIZED || type == config::ragebot_t::AIM_LEGIT ) return true;
        return false;
    } );

    auto aim_strength{ global->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Aim strength" ), &g_cfg.m_ragebot.m_aim_strength, 1, 100, ASTRAL_XORSTR_STATIC( "%" ) ) };
    aim_strength->set_tooltip( ASTRAL_XORSTR_STATIC( "Steering strength for Smooth, Smooth velocity, Assistive, Legit, and Humanized aim types." ) );
    aim_strength->set_render_on( ragebot_strength_aim );

    auto smooth_curve{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Smooth curve" ), SMOOTH_CURVES, config::ragebot_t::SMOOTH_CURVE_COUNT, &g_cfg.m_ragebot.m_smooth_curve ) };
    smooth_curve->set_tooltip( ASTRAL_XORSTR_STATIC( "Shapes how steering strength changes with angular distance for smooth-style aim types." ) );
    smooth_curve->set_render_on( ragebot_smooth_curve_shown );

    auto smooth_curve_amount{ global->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Smooth curve amount" ), &g_cfg.m_ragebot.m_smooth_curve_amount, 0, 200, ASTRAL_XORSTR_STATIC( "%" ) ) };
    smooth_curve_amount->set_tooltip( ASTRAL_XORSTR_STATIC( "Scales the selected smooth curve. 100% uses the full curve, 0% behaves linearly." ) );
    smooth_curve_amount->set_render_on( ragebot_smooth_curve_shown );

    auto human_reaction{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human reaction" ),
        &g_cfg.m_ragebot.m_humanized_reaction_ms_min, &g_cfg.m_ragebot.m_humanized_reaction_ms_max, 0, 1000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    human_reaction->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized delay range before Humanized or Legit aim starts committing to a newly acquired target." ) );
    human_reaction->set_render_on( ragebot_humanized_aim );

    auto human_fire_delay{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human fire delay" ),
        &g_cfg.m_ragebot.m_humanized_fire_delay_ms_min, &g_cfg.m_ragebot.m_humanized_fire_delay_ms_max, 0, 1000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    human_fire_delay->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized settle delay range after alignment before Humanized or Legit auto-shoot fires (hitscan/melee)." ) );
    human_fire_delay->set_render_on( ragebot_humanized_aim );

    auto human_projectile_fire_delay{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human projectile delay" ),
        &g_cfg.m_ragebot.m_humanized_projectile_fire_delay_ms_min, &g_cfg.m_ragebot.m_humanized_projectile_fire_delay_ms_max, 0, 2000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    human_projectile_fire_delay->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized settle delay range before Humanized auto-shoot for projectile weapons." ) );
    human_projectile_fire_delay->set_render_on( ragebot_humanized_aim );

    auto human_projectile_tolerance{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human projectile aim tol." ),
        &g_cfg.m_ragebot.m_humanized_projectile_aim_tolerance_min, &g_cfg.m_ragebot.m_humanized_projectile_aim_tolerance_max, 10, 300, ASTRAL_XORSTR_STATIC( "cdeg" ) ) };
    human_projectile_tolerance->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized aim error tolerance range (centidegrees) allowed before Humanized projectile auto-shoot can fire." ) );
    human_projectile_tolerance->set_render_on( ragebot_humanized_aim );

    auto human_lock{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human target lock" ),
        &g_cfg.m_ragebot.m_humanized_lock_ms_min, &g_cfg.m_ragebot.m_humanized_lock_ms_max, 0, 2000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    human_lock->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized minimum time range to keep tracking the current Humanized target before switching." ) );
    human_lock->set_render_on( ragebot_humanized_aim );

    auto human_switch{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human switch delay" ),
        &g_cfg.m_ragebot.m_humanized_switch_ms_min, &g_cfg.m_ragebot.m_humanized_switch_ms_max, 0, 2000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    human_switch->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized time range a different target must remain preferred before Humanized aim switches after lock expiry." ) );
    human_switch->set_render_on( ragebot_humanized_aim );

    auto human_fire_interval{ global->add< gui::slider_range_int >( ASTRAL_XORSTR_STATIC( "Human fire interval" ),
        &g_cfg.m_ragebot.m_humanized_min_fire_interval_ms_min, &g_cfg.m_ragebot.m_humanized_min_fire_interval_ms_max, 0, 2000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    human_fire_interval->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomized minimum time range between Humanized auto-shots to avoid unnatural firing cadence." ) );
    human_fire_interval->set_render_on( ragebot_humanized_aim );

    auto allow_aim_in_ui{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Allow aim in UI" ), &g_cfg.m_ragebot.m_allow_aim_in_ui ) };
    allow_aim_in_ui->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows ragebot aiming logic to run while the menu/UI is open." ) );
    SET_ELEMENT( allow_aim_in_ui );

    auto fov{ global->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "FOV" ), &g_cfg.m_ragebot.m_fov, 1, 180, ASTRAL_XORSTR_STATIC( "deg" ) ) };
    fov->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates how many degrees the ragebot can scan for targets within. Setting to 180 disables all restrictions." ) );
    SET_ELEMENT( fov );

    for ( usize i{ }; i < config::CFG_PROFILE_COUNT; ++i )
    {
        auto &override{ g_cfg.m_ragebot.m_class_aim_overrides[ i ] };
        auto enabled_override{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Class Aim/FOV override" ), &override.m_enabled ) };
        auto override_type{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Class aim type" ), AIM_TYPES,
                                                           config::ragebot_t::AIM_TYPE_COUNT, &override.m_aim_type ) };
        auto override_fov{ global->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Class FOV" ), &override.m_fov,
                                                           1, 180, ASTRAL_XORSTR_STATIC( "deg" ) ) };
        enabled_override->set_render_on( RAGEBOT_CLASS_OVERRIDE_PREDICATES[ i ] );
        override_type->set_render_on( RAGEBOT_CLASS_OVERRIDE_ENABLED_PREDICATES[ i ] );
        override_fov->set_render_on( RAGEBOT_CLASS_OVERRIDE_ENABLED_PREDICATES[ i ] );
    }

    auto projectile_override{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Projectile Aim/FOV override" ),
        &g_cfg.m_ragebot.m_projectile_aim_override.m_enabled ) };
    auto projectile_override_type{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Projectile aim type" ), AIM_TYPES,
        config::ragebot_t::AIM_TYPE_COUNT, &g_cfg.m_ragebot.m_projectile_aim_override.m_aim_type ) };
    auto projectile_override_fov{ global->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Projectile FOV" ),
        &g_cfg.m_ragebot.m_projectile_aim_override.m_fov, 1, 180, ASTRAL_XORSTR_STATIC( "deg" ) ) };
    SET_ELEMENT( projectile_override );
    projectile_override_type->set_render_on( &g_cfg.m_ragebot.m_projectile_aim_override.m_enabled );
    projectile_override_fov->set_render_on( &g_cfg.m_ragebot.m_projectile_aim_override.m_enabled );

    auto targets{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Targets" ), TARGET_OPTIONS, config::TARGET_COUNT, g_cfg.m_ragebot.m_targets ) };
    targets->set_tooltip( ASTRAL_XORSTR_STATIC( "What should the ragebot actually target at?" ) );

    auto ignore{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Ignore filters" ), IGNORE_OPTIONS, config::IGNORE_COUNT, g_cfg.m_ragebot.m_ignores ) };
    ignore->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows the ragebot to filter certain targets based on conditions." ) );

    auto preferences{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Preferences" ), RAGEBOT_PREFERENCES, config::PREFERENCE_COUNT, g_cfg.m_ragebot.m_preferences ) };
    preferences->set_tooltip( ASTRAL_XORSTR_STATIC( "Control what the ragebot can do automatically for you when it runs." ) );

    auto classes{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Allowed target classes" ), TARGET_CLASSES, config::TARGET_CLASS_COUNT, g_cfg.m_ragebot.m_target_classes ) };
    classes->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates what classes the ragebot is allowed to target." ) );

    SET_ELEMENT( targets );
    SET_ELEMENT( ignore );
    SET_ELEMENT( preferences );
    SET_ELEMENT( classes );

    auto prevent_bait{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Prevent bait" ), &g_cfg.m_ragebot.m_prevent_bait ) };
    prevent_bait->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_ragebot.m_bait_override_key );
    prevent_bait->set_tooltip( ASTRAL_XORSTR_STATIC( "Prevent the ragebot from targeting vaccinator medics that are walking into your FOV to bait your shots. Use the keybind to override this setting and shoot the medic anyways." ) );
    SET_ELEMENT( prevent_bait );

    auto splash_quality{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Splashbot quality" ), SPLASH_QUALITIES, config::SPLASH_QUALITY_COUNT, &g_cfg.m_ragebot.m_splash_quality ) };
    splash_quality->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the point generation quality of the splashbot on all weapons.\nDepending on your CPU, a higher setting could cause a significant performance hit." ) );
    SET_ELEMENT( splash_quality );

    auto extrapolation{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Extrapolation" ), &g_cfg.m_lagcomp.m_extrapolate ) };
    extrapolation->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows the ragebot to extrapolate a player's position to hit their potential unlag position.\nUseful against warping or fakelagging players." ) );
    extrapolation->set_render_on( &ragebot_context_available );

    auto resolve{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Resolver" ), &g_cfg.m_resolver.m_enabled ) };
    resolve->set_tooltip( ASTRAL_XORSTR_STATIC( "Resolves the real yaw and pitch of players faking their angles, for headshots with the sniper rifles and the Ambassador.\nA missed headshot makes it try the next guess." ) );
    resolve->set_render_on( &ragebot_context_available );

    auto melee_enabled{ melee->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_melee.m_enabled ) };
    melee_enabled->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_melee.m_activate_key );
    melee_enabled->set_render_on( &ragebot_context_available );

    auto melee_aim_type{ melee->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Aim type" ), MELEE_AIM_TYPES,
        config::melee_t::AIM_TYPE_COUNT, &g_cfg.m_melee.m_aim_type ) };
    melee_aim_type->set_tooltip( ASTRAL_XORSTR_STATIC( "Plain visibly turns your view toward the swing. Silent changes only the outgoing command and corrects movement." ) );
    melee_aim_type->set_render_on( &ragebot_context_available );

    auto melee_trigger{ melee->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Melee trigger" ), &g_cfg.m_melee.m_trigger ) };
    melee_trigger->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_melee.m_trigger_key );
    melee_trigger->set_tooltip( ASTRAL_XORSTR_STATIC( "Swings only when the current crosshair direction can connect with a valid melee target." ) );
    melee_trigger->set_render_on( &ragebot_context_available );

    auto auto_swing{ melee->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-swing" ), &g_cfg.m_melee.m_auto_swing ) };
    auto_swing->set_tooltip( ASTRAL_XORSTR_STATIC( "Automatically swings when the auto-melee finds a target." ) );
    auto_swing->set_render_on( &ragebot_context_available );

    auto swing_pred{ melee->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Swing prediction" ), &g_cfg.m_melee.m_swing_prediction ) };
    swing_pred->set_tooltip( ASTRAL_XORSTR_STATIC( "Attempts to predict the right time to swing with the auto melee so you can get a hit off faster. Useful when using the Market Gardener." ) );
    swing_pred->set_render_on( &is_able_to_configure_melee );

    auto auto_backstab{ melee->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-backstab" ), &g_cfg.m_melee.m_auto_backstab ) };
    auto_backstab->set_tooltip( ASTRAL_XORSTR_STATIC( "Lets the auto-melee account for spy's backstab." ) );
    auto_backstab->set_render_on( &is_able_to_configure_melee );

    auto always_crit{ melee->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Always crit" ), &g_cfg.m_melee.m_always_crit ) };
    always_crit->set_tooltip( ASTRAL_XORSTR_STATIC( "Forces a crit on every melee swing, manual or from the auto-melee, without the crit key.\nNeeds Crits enabled under Misc and a bucket that can pay for the crit." ) );
    always_crit->set_render_on( &ragebot_context_available );

    for ( usize i{ 0 }; i < config::CFG_PROFILE_COUNT; ++i )
    {
        // Hitscan (if available).
        if ( can_create_hitscan_profile( i ) )
        {
            auto hitscan{ general->add_group( ICON_CROSSHAIR, ragebot_hitscan_label( i ), gui::column_t::right ) };
            auto aimpoint_priority{ hitscan->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Priority aimpoint" ), PRIORITY_AIMPOINTS, config::AIMPOINT_PRIORITY_COUNT, &g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_aimpoint_priority ) };
            aimpoint_priority->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the hitbox the ragebot should scan first." ) );
            SET_HITSCAN( aimpoint_priority, i );

            auto hitscan_points{ hitscan->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Hitscan points" ), HITSCAN_AIMPOINTS, config::HITSCAN_COUNT, g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_hitscan_boxes ) };
            hitscan_points->set_tooltip( ASTRAL_XORSTR_STATIC( "Filters what other hitboxes the ragebot is allowed to shoot at if the priority point fails." ) );
            SET_HITSCAN( hitscan_points, i );

            auto multipoint{ hitscan->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Multipoint" ), &g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_hitscan_multipoint ) };
            multipoint->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows the ragebot to scan multiple points within the hitbox mins/maxs if the center point is not visible." ) );
            SET_HITSCAN( multipoint, i );

            auto point_scale{ hitscan->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Point scale" ), &g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_hitscan_multipoint_scale, 1, 100, ASTRAL_XORSTR_STATIC( "pts" ) ) };
            point_scale->set_tooltip( ASTRAL_XORSTR_STATIC( "How many points in the hitbox mins/maxs should be seeded for multipoint." ) );
            point_scale->set_render_on( RAGEBOT_MULTIPOINT_PREDICATES[ i ] );

            auto hitchance{ hitscan->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Hitchance" ), &g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_hitscan_hitchance, 0, 100, ASTRAL_XORSTR_STATIC( "%" ) ) };
            hitchance->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates the minimum percentile chance needed for a shot to actually fire on the ragebot when a point is found." ) );
            hitchance->set_render_on( RAGEBOT_MULTIPOINT_PREDICATES[ i ] );

            auto roll_angles{ hitscan->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Use roll angles" ), &g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_hitscan_roll_angles ) };
            roll_angles->set_tooltip( ASTRAL_XORSTR_STATIC( "Rolls the view on shots with a predicted seed so the spread pattern lines up with the target's long side, which lands more pellets at range. The server sees the roll angle." ) );
            SET_HITSCAN( roll_angles, i );

            auto use_history{ hitscan->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Use history" ), &g_cfg.m_ragebot.m_hitscan_profiles[ i ].m_use_history ) };
            use_history->set_tooltip( ASTRAL_XORSTR_STATIC( "Should the ragebot use the lag compensation record history generated by the server? This can allow you to shoot players that have gone behind a wall." ) );
            SET_HITSCAN( use_history, i );
        }

        // Projectile (if available)
        if ( can_create_proj_profile( i ) )
        {
            auto projectile{ general->add_group( ICON_CROSSHAIR, ragebot_projectile_label( i ), gui::column_t::right ) };
            auto aimpoint_priority{ projectile->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Priority aimpoint" ), PROJECTILE_AIMPOINTS, config::BBOX_AIMPOINT_PRIORITY_COUNT, &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_aimpoint_priority ) };
            aimpoint_priority->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the hitbox the ragebot should scan first." ) );
            SET_PROJECTILE( aimpoint_priority, i );

            auto projectile_points{ projectile->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Projectile points" ), PROJECTILE_AIMPOINTS, config::PROJECTILE_BBOX_COUNT, g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_aimpoints ) };
            projectile_points->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows the ragebot to select a different bbox point if the priority one is not available." ) );
            SET_PROJECTILE( projectile_points, i );

            auto multipoint{ projectile->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Building multipoint" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_multipoint ) };
            multipoint->set_tooltip( ASTRAL_XORSTR_STATIC( "Scans points near the edges of building bounds when the center points cannot be hit." ) );
            SET_PROJECTILE( multipoint, i );

            auto point_scale{ projectile->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Building point scale" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_multipoint_scale, 1, 100, ASTRAL_XORSTR_STATIC( "%" ) ) };
            point_scale->set_tooltip( ASTRAL_XORSTR_STATIC( "Controls how far building multipoints are placed toward the edges of the bounds." ) );
            point_scale->set_render_on( RAGEBOT_PROJECTILE_MULTIPOINT_PREDICATES[ i ] );

            auto splashbot{ projectile->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Splashbot" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_splashbot ) };
            splashbot->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows the ragebot to try and find a point on a brush to deal splash damage to a player.\nWhile splashbot runs in parallel, some CPUs will run it slower than others. Tweak the splash quality options if you're suffering FPS issues using this." ) );
            SET_PROJECTILE( splashbot, i );

            auto splashbot_mode{ projectile->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Splashbot mode" ), SPLASHBOT_MODES, config::SPLASHBOT_COUNT, &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_splashbot_mode ) };
            splashbot_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the mode the splashbot should operate in.\n\"Include\" will include splash points in the scan if the main points fail.\n\"Prefer\" will prefer a splash point no matter what when using the ragebot." ) );
            splashbot_mode->set_render_on( RAGEBOT_SPLASH_PREDICATES[ i ] );

            auto max_targets{ projectile->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Max targets" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_max_targets, 1, 6, ASTRAL_XORSTR_STATIC( "tgts" ) ) };
            max_targets->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the maximum amount of targets the ragebot can try and scan for.\nWhile projectile calculations are done in parallel, some CPUs will run calculations slower than others. Set this lower if you experience performance issues in gameplay." ) );
            SET_PROJECTILE( max_targets, i );

            if ( i == config::CFG_PROFILE_SNIPER )
            {
                auto wait_for_kill{ projectile->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Huntsman: Wait for kill" ), &g_cfg.m_ragebot.m_huntsman_wait_for_kill ) };
                wait_for_kill->set_tooltip( ASTRAL_XORSTR_STATIC( "Makes the ragebot stall a shot until the shot would actually kill the target in one shot." ) );
                SET_PROJECTILE( wait_for_kill, i );
            }

            if ( i == config::CFG_PROFILE_DEMOMAN )
            {
                auto smark_donk{ projectile->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Smark Donk" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_smark_donk ) };
                smark_donk->set_tooltip( ASTRAL_XORSTR_STATIC( "Times the Loose Cannon fuse to detonate immediately after the predicted direct hit. Direct hits that are already lethal are fired immediately." ) );
                SET_PROJECTILE( smark_donk, i );

                auto sticky_auto_charge_targets{ projectile->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Auto-charge targets" ), AUTO_CHARGE_TARGETS, config::CHARGE_COUNT, g_cfg.m_ragebot.m_auto_charge_sticky_targets ) };
                sticky_auto_charge_targets->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows you to dictate exactly which stickybomb launchers to auto-charge with." ) );
                SET_PROJECTILE( sticky_auto_charge_targets, i );
            }

            if ( i != config::CFG_PROFILE_DEMOMAN )
            {
                auto roll_angles{ projectile->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Use roll angles" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_roll_angles ) };
                roll_angles->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows the ragebot to use roll angles in the angle calculations. This allows you to shoot in certain positions where other people conventionally can't." ) );
                SET_PROJECTILE( roll_angles, i );
            }

            auto max_sim_time{ projectile->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Max sim time" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_max_sim_time, 0.1f, MAX_SIM_TIME, 1, ASTRAL_XORSTR_STATIC( "s" ) ) };
            max_sim_time->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the maximum projectile travel time the ragebot is allowed to search for targets from (high arc adds 5 seconds onto this).\nYou may wish to set this lower if you experience performance issues in gameplay." ) );
            SET_PROJECTILE( max_sim_time, i );

            auto hitchance{ projectile->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Hitchance" ), &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_hitchance, 0, 100, ASTRAL_XORSTR_STATIC( "%" ) ) };
            hitchance->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates the minimum chance a projectile prediction path must have before the ragebot will shoot.\nSetting this to 0 completely disables the hitchance system, but is not recommended unless you are going for long range arcs with Demoman." ) );
            SET_PROJECTILE( hitchance, i );

            auto position_mode{ projectile->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Position mode" ), POSITION_MODES, config::POSITION_COUNT, &g_cfg.m_ragebot.m_projectile_profiles[ i ].m_projectile_position_mode ) };
            position_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates which position the FOV scanner should use when searching for a target.\n\"Original\" will do tests against the original pre-prediction position.\n\"Predicted\" will do tests against the predicted player position." ) );
            SET_PROJECTILE( position_mode, i );
        }
    }

    build_ragebot_visuals( visuals );
}

static bool aa_enabled( )
{
    return has_been_in_game_once( ) && g_cfg.m_anti_hack.m_anti_aim_enabled;
}

static const char *fakelag_label( usize idx )
{
    switch ( idx )
    {
        case config::CFG_PROFILE_SCOUT: return ASTRAL_XORSTR_STATIC( "Fakelag (Scout)" );
        case config::CFG_PROFILE_SOLDIER: return ASTRAL_XORSTR_STATIC( "Fakelag (Soldier)" );
        case config::CFG_PROFILE_PYRO: return ASTRAL_XORSTR_STATIC( "Fakelag (Pyro)" );
        case config::CFG_PROFILE_DEMOMAN: return ASTRAL_XORSTR_STATIC( "Fakelag (Demoman)" );
        case config::CFG_PROFILE_HEAVY: return ASTRAL_XORSTR_STATIC( "Fakelag (Heavy)" );
        case config::CFG_PROFILE_ENGINEER: return ASTRAL_XORSTR_STATIC( "Fakelag (Engineer)" );
        case config::CFG_PROFILE_MEDIC: return ASTRAL_XORSTR_STATIC( "Fakelag (Medic)" );
        case config::CFG_PROFILE_SNIPER: return ASTRAL_XORSTR_STATIC( "Fakelag (Sniper)" );
        case config::CFG_PROFILE_SPY: return ASTRAL_XORSTR_STATIC( "Fakelag (Spy)" );
        default: return ASTRAL_XORSTR_STATIC( "Should never see this..." );
    }
}

static const char *anti_aim_label( usize idx )
{
    switch ( idx )
    {
        case config::CFG_PROFILE_SCOUT: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Scout)" );
        case config::CFG_PROFILE_SOLDIER: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Soldier)" );
        case config::CFG_PROFILE_PYRO: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Pyro)" );
        case config::CFG_PROFILE_DEMOMAN: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Demoman)" );
        case config::CFG_PROFILE_HEAVY: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Heavy)" );
        case config::CFG_PROFILE_ENGINEER: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Engineer)" );
        case config::CFG_PROFILE_MEDIC: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Medic)" );
        case config::CFG_PROFILE_SNIPER: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Sniper)" );
        case config::CFG_PROFILE_SPY: return ASTRAL_XORSTR_STATIC( "Anti-Aim (Spy)" );
        default: return ASTRAL_XORSTR_STATIC( "Should never see this..." );
    }
}

static bool class_profile_edited( usize idx )
{
    if ( !has_been_in_game_once( ) )
    {
        return false;
    }

    return g_cl.m_target_config_profile == idx;
}

static bool is_fakelag_on( usize idx )
{
    return class_profile_edited( idx ) && g_cfg.m_anti_hack.m_fakelag_enabled;
}

template< usize idx >
static bool is_anti_aim_on( )
{
    return class_profile_edited( idx ) && g_cfg.m_anti_hack.m_anti_aim_enabled && g_cfg.m_anti_hack.m_aa_mode == config::AA_MODE_RAGE;
}

template< usize idx >
static bool fakelag_enabled( )
{
    return is_fakelag_on( idx );
}

template< usize idx >
static bool is_static_enabled( )
{
    return is_fakelag_on( idx ) && g_cfg.m_anti_hack.m_fl_profiles[ idx ].m_fakelag_mode == config::FAKELAG_MODE_STATIC;
}

template< usize idx >
static bool is_adaptive_enabled( )
{
    return is_fakelag_on( idx ) && g_cfg.m_anti_hack.m_fl_profiles[ idx ].m_fakelag_mode == config::FAKELAG_MODE_ADAPTIVE;
}

template< usize idx >
static bool is_random_enabled( )
{
    return is_fakelag_on( idx ) && g_cfg.m_anti_hack.m_fl_profiles[ idx ].m_fakelag_mode == config::FAKELAG_MODE_RANDOM;
}

// for use in the below funcs...
static bool aa_on( usize idx )
{
    return class_profile_edited( idx ) && g_cfg.m_anti_hack.m_anti_aim_enabled && g_cfg.m_anti_hack.m_aa_mode == config::AA_MODE_RAGE;
}

// Yea, this fucking sucks.
// FWork - ya dont say...
template< usize idx >
static bool real_static_yaw( )
{
    return aa_on( idx ) && g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_real_yaw_mode == config::YAW_MODE_STATIC;
}

template< usize idx >
static bool fake_static_yaw( )
{
    return aa_on( idx ) && g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_fake_yaw_mode == config::YAW_MODE_STATIC;
}

template< usize idx >
static bool real_cylical_yaw( )
{
    return aa_on( idx ) && g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_real_yaw_mode == config::YAW_MODE_CYLICAL;
}

template< usize idx >
static bool fake_cylical_yaw( )
{
    return aa_on( idx ) && g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_fake_yaw_mode == config::YAW_MODE_CYLICAL;
}

template< usize idx >
static bool any_jitter_applied( )
{
    return aa_on( idx ) && ( g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_apply_jitter_fake || g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_apply_jitter_real );
}

template< usize idx >
static bool any_spin_applied( )
{
    return aa_on( idx ) && ( g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_real_yaw_mode == config::YAW_MODE_SPIN || g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_fake_yaw_mode == config::YAW_MODE_SPIN );
}

template< usize idx >
static bool break_pitch_off( )
{
    return aa_on( idx ) && !g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_break_pitch;
}

template< usize idx >
static bool real_yaw_on( )
{
    return aa_on( idx ) && g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_real_yaw_mode != config::YAW_MODE_NONE;
}

template< usize idx >
static bool fake_yaw_on( )
{
    return aa_on( idx ) && g_cfg.m_anti_hack.m_aa_profiles[ idx ].m_fake_yaw_mode != config::YAW_MODE_NONE;
}

MAKE_PREDICATES( is_static_enabled, FAKELAG_STATIC_PREDICATES );
MAKE_PREDICATES( is_adaptive_enabled, FAKELAG_ADAPTIVE_PREDICATES );
MAKE_PREDICATES( is_random_enabled, FAKELAG_RANDOM_PREDICATES );
MAKE_PREDICATES( fakelag_enabled, FAKELAG_ENABLED_PREDICATES );
MAKE_PREDICATES( is_anti_aim_on, ANTIAIM_ENABLED_PREDICATES );
MAKE_PREDICATES( real_static_yaw, STATIC_REAL_YAW_PREDICATES );
MAKE_PREDICATES( fake_static_yaw, STATIC_FAKE_YAW_PREDICATES );
MAKE_PREDICATES( real_cylical_yaw, CYLICAL_REAL_YAW_PREDICATES );
MAKE_PREDICATES( fake_cylical_yaw, CYLICAL_FAKE_YAW_PREDICATES );
MAKE_PREDICATES( any_jitter_applied, JITTER_PREDICATES );
MAKE_PREDICATES( break_pitch_off, BREAK_PITCH_PREDICATES );
MAKE_PREDICATES( real_yaw_on, REAL_YAW_ENABLED_PREDICATES );
MAKE_PREDICATES( fake_yaw_on, FAKE_YAW_ENABLED_PREDICATES );
MAKE_PREDICATES( any_spin_applied, SPIN_APPLIED_PREDICATES );

static bool real_highlight_possible( )
{
    return g_cfg.m_anti_hack.m_draw_real_hitboxes && g_cfg.m_anti_hack.m_real_draw_mode != config::DRAW_MODE_SOLID;
}

static bool fake_highlight_possible( )
{
    return g_cfg.m_anti_hack.m_draw_fake_hitboxes && g_cfg.m_anti_hack.m_fake_draw_mode != config::DRAW_MODE_SOLID;
}

static bool real_base_possible( )
{
    return g_cfg.m_anti_hack.m_draw_real_hitboxes && g_cfg.m_anti_hack.m_real_draw_mode != config::DRAW_MODE_ONLY_HEAD;
}

static bool fake_base_possible( )
{
    return g_cfg.m_anti_hack.m_draw_fake_hitboxes && g_cfg.m_anti_hack.m_fake_draw_mode != config::DRAW_MODE_ONLY_HEAD;
}

static void build_anti_hack( )
{
    static const char *const ANTI_AIM_MODES[]{ ASTRAL_XORSTR_STATIC( "Legit" ), ASTRAL_XORSTR_STATIC( "Rage" ) };
    static const char *const FAKELAG_MODES[]{ ASTRAL_XORSTR_STATIC( "Static" ), ASTRAL_XORSTR_STATIC( "Adaptive" ), ASTRAL_XORSTR_STATIC( "Random" ) };
    static const char *const REAL_PITCHES[]{ ASTRAL_XORSTR_STATIC( "Up" ), ASTRAL_XORSTR_STATIC( "Down" ), ASTRAL_XORSTR_STATIC( "Zero" ), ASTRAL_XORSTR_STATIC( "Jitter" ) };
    static const char *const FAKE_PITCHES[]{ ASTRAL_XORSTR_STATIC( "Off" ), ASTRAL_XORSTR_STATIC( "Up" ), ASTRAL_XORSTR_STATIC( "Down" ), ASTRAL_XORSTR_STATIC( "Jitter" ) };
    static const char *const YAW_MODES[]{ ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Auto" ), ASTRAL_XORSTR_STATIC( "Static" ), ASTRAL_XORSTR_STATIC( "Cylical" ), ASTRAL_XORSTR_STATIC( "Spin" ) };
    static const char *const YAW_BASES[]{ ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Eye angles" ), ASTRAL_XORSTR_STATIC( "At FOV target" ) };

    auto anti_hack{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Anti-Hack" ), ICON_USER ) };
    auto general{ anti_hack->add_subtab( ASTRAL_XORSTR_STATIC( "General" ) ) };
    auto visuals{ anti_hack->add_subtab( ASTRAL_XORSTR_STATIC( "Visuals" ) ) };

    auto global{ general->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Global" ), gui::column_t::left ) };

    auto notice{ global->add< gui::label >( ASTRAL_XORSTR_STATIC( "notice" ), &in_game_warning, 2 ) };
    notice->set_render_on( &is_not_in_game );

    auto anti_aim_enabled{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enable anti-aim" ), &g_cfg.m_anti_hack.m_anti_aim_enabled ) };
    anti_aim_enabled->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_anti_hack.m_anti_aim_key );
    anti_aim_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Abuses flaws with animation updates to desynchronize your hitboxes to other players." ) );
    anti_aim_enabled->set_render_on( &has_been_in_game_once );

    auto fakelag_enabled{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enable fakelag" ), &g_cfg.m_anti_hack.m_fakelag_enabled ) };
    fakelag_enabled->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_anti_hack.m_fakelag_key );
    fakelag_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Purposely chokes commands to break the game's lag compensation." ) );
    fakelag_enabled->set_render_on( &has_been_in_game_once );

    auto retain_blastjump{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Retain blastjump" ),
                                                          &g_cfg.m_anti_hack.m_retain_blastjump ) };
    auto retain_soldier{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Soldier only" ),
                                                        &g_cfg.m_anti_hack.m_retain_blastjump_soldier_only ) };
    retain_blastjump->set_tooltip( ASTRAL_XORSTR_STATIC( "Chokes the landing command while the blast-jumping condition is still active, retaining it across touchdown." ) );
    retain_blastjump->set_render_on( &has_been_in_game_once );
    retain_soldier->set_render_on( &g_cfg.m_anti_hack.m_retain_blastjump );

    auto anti_aim_mode{ global->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Anti-aim mode" ), ANTI_AIM_MODES, config::AA_MODE_COUNT, &g_cfg.m_anti_hack.m_aa_mode ) };
    anti_aim_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates the mode which anti-aim should run in.\n\"Legit\" tries to make your angles look legitimate.\n\"Rage\" will do everything it can to mess with animations." ) );
    anti_aim_mode->set_render_on( &aa_enabled );

    auto headshot_threats_only{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Only on headshot threats" ), &g_cfg.m_anti_hack.m_headshot_threats_only ) };
    headshot_threats_only->set_tooltip( ASTRAL_XORSTR_STATIC( "Auto yaws and legit anti-aim only count enemies holding a weapon that can headshot (sniper rifles, Ambassador, Huntsman).\nWithout one around, auto stays on your camera and legit stops choking." ) );
    headshot_threats_only->set_render_on( &aa_enabled );

    auto anti_backstab{ global->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Anti-backstab" ), &g_cfg.m_anti_hack.m_anti_backstab ) };
    anti_backstab->set_tooltip( ASTRAL_XORSTR_STATIC( "Turns your yaw toward the closest enemy spy holding a knife once he can reach you before your next packet.\nThe server checks backstabs against the yaw of the last command you sent, so your camera does not move." ) );
    anti_backstab->set_render_on( &has_been_in_game_once );

    auto flip_direction{ global->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Flip direction" ), &g_cfg.m_anti_hack.m_flip_key ) };
    flip_direction->set_tooltip( ASTRAL_XORSTR_STATIC( "Mirrors configured real and fake yaw offsets. Supports Hold, Toggle and Always bind modes." ) );
    flip_direction->set_render_on( &aa_enabled );

    auto anti_aim_visuals{ visuals->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Hitboxes" ), gui::column_t::left ) };

    static const char *const HITBOX_DRAW_MODES[]{
        ASTRAL_XORSTR_STATIC( "Highlight head" ),
        ASTRAL_XORSTR_STATIC( "Only head" ),
        ASTRAL_XORSTR_STATIC( "Solid" ),
    };

    auto draw_real{ anti_aim_visuals->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Draw real hitboxes" ), &g_cfg.m_anti_hack.m_draw_real_hitboxes ) };
    draw_real->set_tooltip( ASTRAL_XORSTR_STATIC( "Visualizes your real angle's hitboxes, according to how the server would animate them." ) );

    auto real_draw_mode{ anti_aim_visuals->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Real draw mode" ), HITBOX_DRAW_MODES, config::DRAW_MODE_COUNT, &g_cfg.m_anti_hack.m_real_draw_mode ) };
    real_draw_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates how the hitbox drawsystem should draw the hitboxes of the real angle." ) );
    real_draw_mode->set_render_on( &g_cfg.m_anti_hack.m_draw_real_hitboxes );

    auto real_base_color{ anti_aim_visuals->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Real base color" ), &g_cfg.m_anti_hack.m_real_base_color ) };
    real_base_color->set_render_on( &real_base_possible );

    auto real_highlight{ anti_aim_visuals->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Real highlight color" ), &g_cfg.m_anti_hack.m_real_highlight_color ) };
    real_highlight->set_render_on( &real_highlight_possible );

    auto draw_fake{ anti_aim_visuals->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Draw fake hitboxes" ), &g_cfg.m_anti_hack.m_draw_fake_hitboxes ) };
    draw_fake->set_tooltip( ASTRAL_XORSTR_STATIC( "Visualizes your fake angle's hitboxes, according to how other clients would animate them." ) );

    auto fake_draw_mode{ anti_aim_visuals->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Fake draw mode" ), HITBOX_DRAW_MODES, config::DRAW_MODE_COUNT, &g_cfg.m_anti_hack.m_fake_draw_mode ) };
    fake_draw_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Dictates how the hitbox drawsystem should draw the hitboxes of the fake angle." ) );
    fake_draw_mode->set_render_on( &g_cfg.m_anti_hack.m_draw_fake_hitboxes );

    auto fake_base_color{ anti_aim_visuals->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Fake base color" ), &g_cfg.m_anti_hack.m_fake_base_color ) };
    fake_base_color->set_render_on( &fake_base_possible );

    auto fake_highlight{ anti_aim_visuals->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Fake highlight color" ), &g_cfg.m_anti_hack.m_fake_highlight_color ) };
    fake_highlight->set_render_on( &fake_highlight_possible );

    auto fake_model{ visuals->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Models" ), gui::column_t::right ) };

    auto fake_chams{ fake_model->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Material" ), chams_modes( ), chams::CHAM_COUNT, &g_cfg.m_anti_hack.m_fake_chams ) };
    fake_chams->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws your model in the pose other players see on you, from the angles of the last command you sent.\nOnly shown in thirdperson." ) );
    fake_chams->set_render_on( &aa_enabled );
    add_custom_cham_slot( fake_model, &g_cfg.m_anti_hack.m_fake_chams,
                          g_cfg.m_anti_hack.m_fake_chams_custom,
                          sizeof( g_cfg.m_anti_hack.m_fake_chams_custom ), &aa_enabled );

    auto fake_chams_tint{ fake_model->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Material tint" ), &g_cfg.m_anti_hack.m_fake_chams_color ) };
    fake_chams_tint->set_render_on( &aa_enabled );

    add_glow_layers( fake_model, g_cfg.m_anti_hack.m_fake_glow, &aa_enabled );

    for ( usize i{ 0 }; i < config::CFG_PROFILE_COUNT; ++i )
    {
        // Fakelag.
        {
            auto fakelag{ general->add_group( ICON_ZAP, fakelag_label( i ), gui::column_t::left ) };

            auto mode{ fakelag->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Fakelag mode" ), FAKELAG_MODES, config::FAKELAG_MODE_COUNT, &g_cfg.m_anti_hack.m_fl_profiles[ i ].m_fakelag_mode ) };
            mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the choke mode for fakelag.\n\"Static\" chokes based on a static tick amount.\n\"Adaptive\" will choke the lowest needed to break LC.\n\"Random\" will choke randomly." ) );
            mode->set_render_on( FAKELAG_ENABLED_PREDICATES[ i ] );

            auto static_amt{ fakelag->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Static amount" ), &g_cfg.m_anti_hack.m_fl_profiles[ i ].m_static_amount, 3, 22 ) };
            static_amt->set_tooltip( ASTRAL_XORSTR_STATIC( "How many ticks should the fakelag choke before unchoking?" ) );
            static_amt->set_render_on( FAKELAG_STATIC_PREDICATES[ i ] );

            auto random_min{ fakelag->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Minimum choke" ), &g_cfg.m_anti_hack.m_fl_profiles[ i ].m_random_min, 3, 22 ) };
            auto random_max{ fakelag->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Maximum choke" ), &g_cfg.m_anti_hack.m_fl_profiles[ i ].m_random_max, 3, 22 ) };
            random_min->set_tooltip( ASTRAL_XORSTR_STATIC( "The minimum random choke amount for the fakelag.\nNOTE: If this is greater than the maximum, fakelag WILL NOT RUN!" ) );
            random_max->set_tooltip( ASTRAL_XORSTR_STATIC( "The maximum random choke amount for the fakelag.\nNOTE: If this is less than the minimum, fakelag WILL NOT RUN!" ) );
            random_min->set_render_on( FAKELAG_RANDOM_PREDICATES[ i ] );
            random_max->set_render_on( FAKELAG_RANDOM_PREDICATES[ i ] );

            auto adaptive_randomizer{ fakelag->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Randomize unchoke time" ), &g_cfg.m_anti_hack.m_fl_profiles[ i ].m_adaptive_randomized_choke ) };
            adaptive_randomizer->set_tooltip( ASTRAL_XORSTR_STATIC( "Randomly sets an up to 4 tick window before unchoking from the base LC amount.\nThis can cause some cheats relying on static extrapolation to miss." ) );
            adaptive_randomizer->set_render_on( FAKELAG_ADAPTIVE_PREDICATES[ i ] );
        }

        // Anti-Aim.
        {
            auto anti_aim{ general->add_group( ICON_CROSSHAIR, anti_aim_label( i ), gui::column_t::right ) };

            auto yaw_base{ anti_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Yaw base" ), YAW_BASES, config::YAW_BASE_COUNT, &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_yaw_base ) };

            yaw_base->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets what base yaw the Anti-Aim should add on from.\n\"Eye angles\" will add on from the direction you are looking.\n\"At FOV target\" adds from the nearest threat you're looking at." ) );
            yaw_base->set_render_on( ANTIAIM_ENABLED_PREDICATES[ i ] );

            auto real_yaw_mode{ anti_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Real yaw" ), YAW_MODES, config::YAW_MODE_COUNT, &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_real_yaw_mode ) };
            auto fake_yaw_mode{ anti_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Fake yaw" ), YAW_MODES, config::YAW_MODE_COUNT, &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_fake_yaw_mode ) };

            real_yaw_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets what mode your real (hidden) yaw is." ) );
            fake_yaw_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets what mode your fake (shown) yaw is." ) );
            real_yaw_mode->set_render_on( ANTIAIM_ENABLED_PREDICATES[ i ] );
            fake_yaw_mode->set_render_on( ANTIAIM_ENABLED_PREDICATES[ i ] );

            auto static_real{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Real angle" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_static_real_angle, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            auto static_fake{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Fake angle" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_static_fake_angle, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            static_real->set_render_on( STATIC_REAL_YAW_PREDICATES[ i ] );
            static_fake->set_render_on( STATIC_FAKE_YAW_PREDICATES[ i ] );

            auto cycle_real_1{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Real angle 1" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_cylical_pos_1_real, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            auto cycle_real_2{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Real angle 2" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_cylical_pos_2_real, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            cycle_real_1->set_render_on( CYLICAL_REAL_YAW_PREDICATES[ i ] );
            cycle_real_2->set_render_on( CYLICAL_REAL_YAW_PREDICATES[ i ] );

            auto cycle_fake_1{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Fake angle 1" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_cylical_pos_1_fake, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            auto cycle_fake_2{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Fake angle 2" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_cylical_pos_2_fake, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            cycle_fake_1->set_render_on( CYLICAL_FAKE_YAW_PREDICATES[ i ] );
            cycle_fake_2->set_render_on( CYLICAL_FAKE_YAW_PREDICATES[ i ] );

            auto real_cycle{ anti_aim->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Real cycle time" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_real_cycle_time, 0.1f, 1.5, 2, ASTRAL_XORSTR_STATIC( " sec" ) ) };
            auto fake_cycle{ anti_aim->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Fake cycle time" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_fake_cycle_time, 0.1f, 1.5f, 2, ASTRAL_XORSTR_STATIC( " sec" ) ) };
            real_cycle->set_render_on( CYLICAL_REAL_YAW_PREDICATES[ i ] );
            fake_cycle->set_render_on( CYLICAL_FAKE_YAW_PREDICATES[ i ] );

            auto apply_jitter_real{ anti_aim->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Apply jitter to real" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_apply_jitter_real ) };
            auto apply_jitter_fake{ anti_aim->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Apply jitter to fake" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_apply_jitter_fake ) };
            apply_jitter_real->set_render_on( REAL_YAW_ENABLED_PREDICATES[ i ] );
            apply_jitter_fake->set_render_on( FAKE_YAW_ENABLED_PREDICATES[ i ] );

            auto jitter_amount{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Jitter amount" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_jitter_amount, 0, 180 ) };
            jitter_amount->set_render_on( JITTER_PREDICATES[ i ] );

            auto spin_amount{ anti_aim->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Spin amount" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_spin_amount, -180, 180, ASTRAL_XORSTR_STATIC( " deg" ) ) };
            spin_amount->set_render_on( SPIN_APPLIED_PREDICATES[ i ] );

            auto real_pitch{ anti_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Real pitch" ), REAL_PITCHES, config::REAL_PITCH_COUNT, &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_real_pitch_mode ) };
            auto fake_pitch{ anti_aim->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Fake pitch" ), FAKE_PITCHES, config::FAKE_PITCH_COUNT, &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_fake_pitch_mode ) };
            real_pitch->set_render_on( ANTIAIM_ENABLED_PREDICATES[ i ] );
            fake_pitch->set_render_on( BREAK_PITCH_PREDICATES[ i ] );

            auto break_pitch{ anti_aim->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Break pitch" ), &g_cfg.m_anti_hack.m_aa_profiles[ i ].m_break_pitch ) };
            break_pitch->set_tooltip( ASTRAL_XORSTR_STATIC( "Attempts to break the resolver of some cheats by jittering your pitch inside and outside of clamped (89/-89) ranges.\nNOTE: Forces your pitch to be either up or down." ) );
            break_pitch->set_render_on( ANTIAIM_ENABLED_PREDICATES[ i ] );
        }
    }
}

static bool target_visual_shown( )
{
    return g_cfg.m_esp.m_target_style != config::TARGET_VISUAL_OFF || g_cfg.m_esp.m_target_chevron;
}

static void build_player_visuals( gui::tab *visuals )
{
    static bool ( *const TARGET_PREDICATES[ config::ESP_TARGET_COUNT ] )( ){
        &editing_target< 0 >,
        &editing_target< 1 >,
        &editing_target< 2 >,
        &editing_target< 3 >,
        &editing_target< 4 >,
    };

    static bool ( *const BLENDED_HEALTH_PREDICATES[ config::ESP_TARGET_COUNT ] )( ){
        &editing_blended_health< 0 >,
        &editing_blended_health< 1 >,
        &editing_blended_health< 2 >,
        &editing_blended_health< 3 >,
        &editing_blended_health< 4 >,
    };

    gui::sub_tab *players{ visuals->add_subtab( ASTRAL_XORSTR_STATIC( "Entities" ) ) };

    attach_esp_preview( players );

    gui::group_box *target{ players->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Entities" ), gui::column_t::left ) };
    gui::group_box *elements{ players->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Elements" ), gui::column_t::left ) };
    gui::group_box *model{ players->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Model" ), gui::column_t::right ) };

    static const char *const TARGET_PLAYERS[]{ ASTRAL_XORSTR_STATIC( "Enemies" ),
                                               ASTRAL_XORSTR_STATIC( "Teammates" ),
                                               ASTRAL_XORSTR_STATIC( "Local" ),
                                               ASTRAL_XORSTR_STATIC( "Friends" ),
                                               ASTRAL_XORSTR_STATIC( "Astral users" ) };
    static const char *const TARGET_BUILDINGS[]{ ASTRAL_XORSTR_STATIC( "Buildings" ) };
    static const char *const TARGET_WORLD[]{ ASTRAL_XORSTR_STATIC( "Pickups" ),
                                             ASTRAL_XORSTR_STATIC( "Intel" ),
                                             ASTRAL_XORSTR_STATIC( "Projectiles" ),
                                             ASTRAL_XORSTR_STATIC( "NPCs" ) };

    static const gui::target_picker::group_t TARGET_GROUPS[]{
        { ASTRAL_XORSTR_STATIC( "Players" ), TARGET_PLAYERS, config::ESP_TARGET_COUNT },
        { ASTRAL_XORSTR_STATIC( "Buildings" ), TARGET_BUILDINGS, 1 },
        { ASTRAL_XORSTR_STATIC( "World" ), TARGET_WORLD, config::WORLD_COUNT },
    };

    gui::target_picker *picker{ target->add< gui::target_picker >(
        TARGET_GROUPS, TARGET_GROUP_COUNT, &g_cfg.m_esp.m_target_group,
        &g_cfg.m_esp.m_edited ) };

    picker->set_marks( &target_marked )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "esp target enemy team local building world projectile pickup health ammo intel npc halloween boss skeleton" ) );

    gui::slider_int *dormant_time{ target->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Dormant memory" ), &g_cfg.m_esp.m_dormant_time, 1, 15 ) };

    dormant_time->set_tooltip( ASTRAL_XORSTR_STATIC(
                                   "How long an entity the server stopped sending keeps being "
                                   "drawn where it was last known." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "dormant memory pvs last known position fade" ) );

    gui::checkbox   *distance_fade{ target->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Distance fade" ), &g_cfg.m_esp.m_distance_fade ) };
    gui::slider_int *distance_fade_start{ target->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Fade starts" ), &g_cfg.m_esp.m_distance_fade_start, 0, 10000,
        ASTRAL_XORSTR_STATIC( "u" ) ) };
    gui::slider_int *distance_fade_end{ target->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Fade ends" ), &g_cfg.m_esp.m_distance_fade_end, 100, 12000,
        ASTRAL_XORSTR_STATIC( "u" ) ) };
    gui::slider_int *distance_fade_minimum{ target->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Minimum opacity" ), &g_cfg.m_esp.m_distance_fade_minimum, 0, 100,
        ASTRAL_XORSTR_STATIC( "%" ) ) };

    distance_fade->set_tooltip( ASTRAL_XORSTR_STATIC(
                                    "Gradually reduces entity ESP opacity with distance while preserving each element's configured alpha." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "distance fade opacity far esp" ) );
    distance_fade_start->set_tooltip( ASTRAL_XORSTR_STATIC( "Distance where ESP begins fading." ) )
        ->set_render_on( &g_cfg.m_esp.m_distance_fade );
    distance_fade_end->set_tooltip( ASTRAL_XORSTR_STATIC( "Distance where ESP reaches its minimum opacity." ) )
        ->set_render_on( &g_cfg.m_esp.m_distance_fade );
    distance_fade_minimum->set_tooltip( ASTRAL_XORSTR_STATIC( "Opacity retained for entities at or beyond the fade end distance." ) )
        ->set_render_on( &g_cfg.m_esp.m_distance_fade );

    static const char *const AVATAR_SHAPES[]{ ASTRAL_XORSTR_STATIC( "Off" ),
                                              ASTRAL_XORSTR_STATIC( "Square" ),
                                              ASTRAL_XORSTR_STATIC( "Circle" ) };

    gui::combo_box *avatars{ target->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Avatars" ), AVATAR_SHAPES, 3, &g_cfg.m_esp.m_avatars ) };

    avatars->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the player's steam picture in front of "
                                               "the name." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "avatar pfp steam picture photo profile" ) );

    auto players_on_top{ target->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Players on top" ), &g_cfg.m_esp.m_players_on_top ) };
    players_on_top->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws player ESP after buildings, pickups and projectiles so important player information is not covered." ) );

    static const char *const LOCAL_MODELS[]{ ASTRAL_XORSTR_STATIC( "Player" ),
                                             ASTRAL_XORSTR_STATIC( "Viewmodel" ),
                                             ASTRAL_XORSTR_STATIC( "Weapon" ) };

    gui::segment_bar *local_model{ model->add< gui::segment_bar >(
        LOCAL_MODELS, config::LOCAL_MODEL_COUNT, &g_cfg.m_chams.m_local_model ) };

    local_model->set_render_on( &editing_target< 2 > );

    static const char *const ENEMY_MODELS[]{ ASTRAL_XORSTR_STATIC( "Player" ),
                                             ASTRAL_XORSTR_STATIC( "Lagcomp" ) };

    gui::segment_bar *enemy_model{ model->add< gui::segment_bar >(
        ENEMY_MODELS, config::ENEMY_MODEL_COUNT, &g_cfg.m_chams.m_enemy_model ) };

    enemy_model->set_render_on( &editing_target< 0 > );

    gui::combo_box *viewmodel_cham{ model->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Base material" ), chams_modes( ), chams::CHAM_COUNT,
        &g_cfg.m_chams.m_viewmodel ) };
    add_custom_cham_slot( model, &g_cfg.m_chams.m_viewmodel, g_cfg.m_chams.m_viewmodel_custom,
                          sizeof( g_cfg.m_chams.m_viewmodel_custom ), &editing_local_viewmodel );
    gui::combo_box *viewmodel_overlay{ model->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Overlay material" ), chams_modes( ), chams::CHAM_OVERLAY_COUNT,
        &g_cfg.m_chams.m_viewmodel_overlay ) };
    add_custom_cham_slot( model, &g_cfg.m_chams.m_viewmodel_overlay,
                          g_cfg.m_chams.m_viewmodel_overlay_custom,
                          sizeof( g_cfg.m_chams.m_viewmodel_overlay_custom ), &editing_local_viewmodel );
    gui::combo_box *weapon_cham{ model->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Base material" ), chams_modes( ), chams::CHAM_COUNT,
        &g_cfg.m_chams.m_weapon ) };
    add_custom_cham_slot( model, &g_cfg.m_chams.m_weapon, g_cfg.m_chams.m_weapon_custom,
                          sizeof( g_cfg.m_chams.m_weapon_custom ), &editing_local_weapon );
    gui::combo_box *weapon_overlay{ model->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Overlay material" ), chams_modes( ), chams::CHAM_OVERLAY_COUNT,
        &g_cfg.m_chams.m_weapon_overlay ) };
    add_custom_cham_slot( model, &g_cfg.m_chams.m_weapon_overlay,
                          g_cfg.m_chams.m_weapon_overlay_custom,
                          sizeof( g_cfg.m_chams.m_weapon_overlay_custom ), &editing_local_weapon );

    viewmodel_cham->set_tooltip( ASTRAL_XORSTR_STATIC( "Material for the hands and weapon you "
                                                      "see in first person." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "viewmodel hands chams material" ) )
        ->set_render_on( &editing_local_viewmodel );

    viewmodel_overlay->set_render_on( &editing_local_viewmodel );

    gui::color_picker *viewmodel_tint{ model->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Material tint" ), &g_cfg.m_chams.m_viewmodel_color ) };
    gui::color_picker *viewmodel_overlay_tint{ model->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Overlay tint" ), &g_cfg.m_chams.m_viewmodel_overlay_color ) };

    viewmodel_tint->set_render_on( &editing_local_viewmodel );
    viewmodel_overlay_tint->set_render_on( &editing_local_viewmodel );

    weapon_cham->set_tooltip( ASTRAL_XORSTR_STATIC( "Material for the weapon model everyone "
                                                   "else sees, and you do in thirdperson." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "weapon world model chams material" ) )
        ->set_render_on( &editing_local_weapon );

    weapon_overlay->set_render_on( &editing_local_weapon );

    gui::color_picker *weapon_tint{ model->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Material tint" ), &g_cfg.m_chams.m_weapon_color ) };
    gui::color_picker *weapon_overlay_tint{ model->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Overlay tint" ), &g_cfg.m_chams.m_weapon_overlay_color ) };

    weapon_tint->set_render_on( &editing_local_weapon );
    weapon_overlay_tint->set_render_on( &editing_local_weapon );

    add_glow_layers( model, g_cfg.m_glow.m_viewmodel, &editing_local_viewmodel );
    add_glow_layers( model, g_cfg.m_glow.m_weapon, &editing_local_weapon );

    static const char *const ELEMENT_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Name" ),
        ASTRAL_XORSTR_STATIC( "Distance" ), ASTRAL_XORSTR_STATIC( "Health" ),
        ASTRAL_XORSTR_STATIC( "Weapon" ), ASTRAL_XORSTR_STATIC( "Flags" ),
        ASTRAL_XORSTR_STATIC( "Skeleton" ), ASTRAL_XORSTR_STATIC( "Sandevistan Skeletons" ),
        ASTRAL_XORSTR_STATIC( "Tag" )
    };

    static const char *const HEALTH_STYLES[]{
        ASTRAL_XORSTR_STATIC( "Solid" ), ASTRAL_XORSTR_STATIC( "Gradient" ),
        ASTRAL_XORSTR_STATIC( "Health based" )
    };

    for ( usize index{ }; index < config::ESP_TARGET_COUNT; ++index )
    {
        config::esp_profile_t &profile{ g_cfg.m_esp.m_profiles[ index ] };
        bool ( *const when )( ){ TARGET_PREDICATES[ index ] };

        gui::combo_box *shown{ elements->add< gui::combo_box >(
            ASTRAL_XORSTR_STATIC( "Shown" ), ELEMENT_LABELS, config::ESP_ELEMENT_COUNT,
            profile.m_elements ) };

        shown->set_keywords( ASTRAL_XORSTR_STATIC( "box name distance health weapon flags skeleton sandevistan backtrack trail tag friend ignored cheater" ) )
            ->set_render_on( when );

        if ( index == 1 )
        {
            gui::checkbox *no_show_condition{ elements->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Draw conditionally" ), &g_cfg.m_esp.m_draw_teammates_conditionally ) };
            no_show_condition->set_tooltip( ASTRAL_XORSTR_STATIC( "Prevents teammates from drawing unless certain conditions are met (i.e. You're medic and a teammate is injured)." ) )
                ->set_render_on( TARGET_PREDICATES[ 1 ] );
        }

        gui::color_picker *pickers[]{
            elements->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Box" ), &profile.m_box_color ),
            elements->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Name" ), &profile.m_name_color ),
            elements->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Distance" ), &profile.m_distance_color ),
            elements->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Health" ), &profile.m_health_color ),
            elements->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Weapon" ), &profile.m_weapon_color ),
            elements->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Skeleton" ), &profile.m_skeleton_color )
        };

        const bool *sandevistan_shown{
            &profile.m_elements[ static_cast< usize >( config::esp_element_t::sandevistan ) ]
        };

        gui::color_picker *sandevistan_start{ elements->add< gui::color_picker >(
            ASTRAL_XORSTR_STATIC( "Sandevistan start" ), &profile.m_sandevistan_start_color ) };
        gui::color_picker *sandevistan_end{ elements->add< gui::color_picker >(
            ASTRAL_XORSTR_STATIC( "Sandevistan end" ), &profile.m_sandevistan_end_color ) };
        gui::checkbox *sandevistan_show_all{ elements->add< gui::checkbox >(
            ASTRAL_XORSTR_STATIC( "Sandevistan show all" ), &g_cfg.m_esp.m_sandevistan_show_all ) };

        sandevistan_show_all
            ->set_tooltip( ASTRAL_XORSTR_STATIC(
                "Draws Sandevistan trails for every enabled ESP player instead of only the current aimbot target. This can be expensive." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "sandevistan skeleton trail all players performance lag aimbot target" ) )
            ->set_render_on( sandevistan_shown )
            ->set_render_on( when );

        sandevistan_start
            ->set_tooltip( ASTRAL_XORSTR_STATIC(
                "Color of the newest skeleton in the Sandevistan backtrack trail." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "sandevistan skeleton trail backtrack ghost start fade color" ) )
            ->set_render_on( sandevistan_shown )
            ->set_render_on( when );

        sandevistan_end
            ->set_tooltip( ASTRAL_XORSTR_STATIC(
                "Color the oldest skeleton in the Sandevistan trail fades into." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "sandevistan skeleton trail backtrack ghost end fade color" ) )
            ->set_render_on( sandevistan_shown )
            ->set_render_on( when );

        const bool *health_shown{ &profile.m_elements[ static_cast< usize >( config::esp_element_t::health ) ] };

        gui::combo_box    *health_style{ elements->add< gui::combo_box >(
            ASTRAL_XORSTR_STATIC( "Health style" ), HEALTH_STYLES, config::HEALTH_STYLE_COUNT,
            &profile.m_health_style ) };
        gui::color_picker *health_low{ elements->add< gui::color_picker >(
            ASTRAL_XORSTR_STATIC( "Low health" ), &profile.m_health_low_color ) };
        gui::color_picker *overheal{ elements->add< gui::color_picker >(
            ASTRAL_XORSTR_STATIC( "Overheal" ), &profile.m_overheal_color ) };
        gui::slider_int   *health_divisions{ elements->add< gui::slider_int >(
            ASTRAL_XORSTR_STATIC( "Health divisions" ), &profile.m_health_divisions, 0, 10 ) };

        health_style
            ->set_tooltip( ASTRAL_XORSTR_STATIC(
                "Solid paints the bar in the health color. Gradient fades the bar from the low "
                "health color at its base to the health color at its tip. Health based paints the "
                "whole bar in a mix of the two that follows the player's health." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "health bar color gradient fade hp based" ) )
            ->set_render_on( health_shown )
            ->set_render_on( when );

        health_low->set_render_on( health_shown )->set_render_on( BLENDED_HEALTH_PREDICATES[ index ] );

        overheal->set_tooltip( ASTRAL_XORSTR_STATIC( "Color of the bar laid over health above the maximum." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "overheal overhealed buff health color" ) )
            ->set_render_on( health_shown )
            ->set_render_on( when );

        health_divisions
            ->set_tooltip( ASTRAL_XORSTR_STATIC(
                "Splits the health bar into equal sections. Set to 0 for a continuous bar." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "health separator separators fractions divisions segments" ) )
            ->set_render_on( health_shown )
            ->set_render_on( when );

        for ( gui::color_picker *picker : pickers )
        {
            picker->set_render_on( when );
        }

        bool ( *const model_when )( ){ index == 2 ? &editing_local_player : index == 0 ? &editing_enemy_player
                                                                                       : when };
        add_cham_layers( model, profile.m_chams, model_when );
        add_glow_layers( model, g_cfg.m_glow.m_players[ index ], model_when );
        add_cham_tints( model, profile.m_chams, model_when );
    }

    static const char *const RECORD_MODES[]{ ASTRAL_XORSTR_STATIC( "Oldest record" ),
                                             ASTRAL_XORSTR_STATIC( "All records" ) };

    gui::combo_box *records{ model->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Records" ), RECORD_MODES, config::BACKTRACK_COUNT,
        &g_cfg.m_chams.m_backtrack_records ) };

    add_cham_layers( model, g_cfg.m_chams.m_backtrack, &editing_enemy_backtrack );
    add_glow_layers( model, g_cfg.m_glow.m_backtrack, &editing_enemy_backtrack );

    records->set_tooltip( ASTRAL_XORSTR_STATIC( "Which of the lag compensation records the ragebot can reach get a model drawn on them." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "backtrack lagcomp records chams" ) )
        ->set_render_on( &editing_enemy_backtrack );

    gui::color_picker *backtrack_tint{ model->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Material tint" ), &g_cfg.m_chams.m_backtrack_color ) };
    gui::color_picker *backtrack_overlay_tint{ model->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Overlay tint" ), &g_cfg.m_chams.m_backtrack_overlay_color ) };

    backtrack_tint->set_render_on( &editing_enemy_backtrack );
    backtrack_overlay_tint->set_render_on( &editing_enemy_backtrack );

    static const char *const BUILDING_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Name" ),
        ASTRAL_XORSTR_STATIC( "Distance" ), ASTRAL_XORSTR_STATIC( "Health" ),
        ASTRAL_XORSTR_STATIC( "Flags" )
    };

    static const char *const PROJECTILE_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Name" ),
        ASTRAL_XORSTR_STATIC( "Distance" ), ASTRAL_XORSTR_STATIC( "Crit" )
    };

    static const char *const PROJECTILE_TEAMS[]{ ASTRAL_XORSTR_STATIC( "Enemy" ), ASTRAL_XORSTR_STATIC( "Teammate" ) };

    gui::combo_box *building_shown{ elements->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Shown" ), BUILDING_LABELS, 5,
        g_cfg.m_esp.m_building_elements ) };
    gui::combo_box *projectile_shown{ elements->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Shown" ), PROJECTILE_LABELS, 4,
        g_cfg.m_esp.m_projectile_elements ) };
    gui::combo_box *projectile_teams{ elements->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Team" ), PROJECTILE_TEAMS, config::PROJECTILE_TEAM_COUNT,
        g_cfg.m_esp.m_projectile_teams ) };

    building_shown->set_keywords( ASTRAL_XORSTR_STATIC(
                                      "building sentry dispenser teleporter box name distance health flags" ) )
        ->set_render_on( &editing_buildings );

    projectile_shown->set_keywords( ASTRAL_XORSTR_STATIC(
                                        "projectile rocket sticky pipe box name distance crit" ) )
        ->set_render_on( &editing_world< config::WORLD_PROJECTILES > );

    projectile_teams->set_tooltip( ASTRAL_XORSTR_STATIC( "Whose projectiles get drawn. Your own count as a teammate's." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "projectile team enemy teammate friendly own" ) )
        ->set_render_on( &editing_world< config::WORLD_PROJECTILES > );

    add_cham_layers( model, g_cfg.m_chams.m_buildings, &editing_buildings );
    add_glow_layers( model, g_cfg.m_glow.m_buildings, &editing_buildings );
    add_cham_tints( model, g_cfg.m_chams.m_buildings, &editing_buildings );
    add_cham_layers( model, g_cfg.m_chams.m_projectiles, &editing_world< config::WORLD_PROJECTILES > );
    add_glow_layers( model, g_cfg.m_glow.m_projectiles, &editing_world< config::WORLD_PROJECTILES > );
    add_cham_tints( model, g_cfg.m_chams.m_projectiles, &editing_world< config::WORLD_PROJECTILES > );

    static const char *const PICKUP_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Name" ),
        ASTRAL_XORSTR_STATIC( "Distance" ), ASTRAL_XORSTR_STATIC( "Respawn" )
    };

    static const char *const INTEL_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Name" ),
        ASTRAL_XORSTR_STATIC( "Distance" ), ASTRAL_XORSTR_STATIC( "Dropped" )
    };

    gui::combo_box *pickup_shown{ elements->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Shown" ), PICKUP_LABELS, 4,
        g_cfg.m_esp.m_pickup_elements ) };
    gui::combo_box *intel_shown{ elements->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Shown" ), INTEL_LABELS, 4,
        g_cfg.m_esp.m_intel_elements ) };

    pickup_shown->set_keywords( ASTRAL_XORSTR_STATIC(
                                    "pickup health ammo medkit box name distance respawn" ) )
        ->set_render_on( &editing_world< config::WORLD_PICKUPS > );

    intel_shown->set_keywords( ASTRAL_XORSTR_STATIC(
                                   "intel flag briefcase box name distance dropped" ) )
        ->set_render_on( &editing_world< config::WORLD_INTEL > );

    add_cham_layers( model, g_cfg.m_chams.m_pickups, &editing_world< config::WORLD_PICKUPS > );
    add_glow_layers( model, g_cfg.m_glow.m_pickups, &editing_world< config::WORLD_PICKUPS > );
    add_cham_tints( model, g_cfg.m_chams.m_pickups, &editing_world< config::WORLD_PICKUPS > );
    add_cham_layers( model, g_cfg.m_chams.m_intel, &editing_world< config::WORLD_INTEL > );
    add_glow_layers( model, g_cfg.m_glow.m_intel, &editing_world< config::WORLD_INTEL > );
    add_cham_tints( model, g_cfg.m_chams.m_intel, &editing_world< config::WORLD_INTEL > );

    static const char *const NPC_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Box" ), ASTRAL_XORSTR_STATIC( "Name" ),
        ASTRAL_XORSTR_STATIC( "Distance" )
    };

    gui::combo_box *npc_shown{ elements->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Shown" ), NPC_LABELS, 3,
        g_cfg.m_esp.m_npc_elements ) };

    npc_shown->set_tooltip( ASTRAL_XORSTR_STATIC( "Halloween NPCs: Monoculus, Merasmus, the Horseless "
                                                 "Headless Horsemann and skeletons." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "npc halloween boss monoculus eyeball merasmus wizard "
                                             "horsemann hhh skeleton box name distance" ) )
        ->set_render_on( &editing_world< config::WORLD_NPCS > );

    add_cham_layers( model, g_cfg.m_chams.m_npcs, &editing_world< config::WORLD_NPCS > );
    add_glow_layers( model, g_cfg.m_glow.m_npcs, &editing_world< config::WORLD_NPCS > );
    add_cham_tints( model, g_cfg.m_chams.m_npcs, &editing_world< config::WORLD_NPCS > );

    gui::sub_tab *aim_target{ visuals->add_subtab( ASTRAL_XORSTR_STATIC( "Indicators" ) ) };

    gui::group_box *indicator{ aim_target->add_group( ICON_CROSSHAIR,
                                                      ASTRAL_XORSTR_STATIC( "Target visualizer" ),
                                                      gui::column_t::left ) };

    static const char *const TARGET_STYLES[]{
        ASTRAL_XORSTR_STATIC( "Off" ), ASTRAL_XORSTR_STATIC( "Brackets" ),
        ASTRAL_XORSTR_STATIC( "Orbit" )
    };

    gui::combo_box    *style{ indicator->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "3D style" ), TARGET_STYLES, config::TARGET_VISUAL_COUNT,
        &g_cfg.m_esp.m_target_style ) };
    gui::checkbox     *chevron{ indicator->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Chevron" ), &g_cfg.m_esp.m_target_chevron ) };
    gui::checkbox     *animate{ indicator->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Animation" ), &g_cfg.m_esp.m_target_animate ) };
    gui::color_picker *color{ indicator->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Color" ), &g_cfg.m_esp.m_target_color ) };

    style->set_tooltip( ASTRAL_XORSTR_STATIC( "Marks the player currently selected by the ragebot with world-space lines." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "aim target visualizer brackets orbit 3d" ) );
    chevron->set_tooltip( ASTRAL_XORSTR_STATIC( "Points to the selected player above their ESP name, or above the box when the name is hidden." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "aim target arrow name marker" ) );
    animate->set_render_on( &target_visual_shown );
    color->set_render_on( &target_visual_shown );

    gui::group_box *hits{ aim_target->add_group( ICON_ZAP,
                                                 ASTRAL_XORSTR_STATIC( "Hitmarker" ),
                                                 gui::column_t::right ) };

    static const char *const HIT_EVENTS[]{
        ASTRAL_XORSTR_STATIC( "Normal" ), ASTRAL_XORSTR_STATIC( "Critical" ),
        ASTRAL_XORSTR_STATIC( "Kill" )
    };
    static bool ( *const HIT_EVENT_PREDICATES[ config::HITMARKER_EVENT_COUNT ] )( ){
        &editing_hitmarker_event< 0 >, &editing_hitmarker_event< 1 >,
        &editing_hitmarker_event< 2 >
    };
    static bool ( *const HIT_COLOR_PREDICATES[ config::HITMARKER_EVENT_COUNT ] )( ){
        &editing_hitmarker_color< 0 >, &editing_hitmarker_color< 1 >,
        &editing_hitmarker_color< 2 >
    };
    static const char *const HIT_RENDER_MODES[]{
        ASTRAL_XORSTR_STATIC( "World" ), ASTRAL_XORSTR_STATIC( "2D" )
    };

    gui::checkbox     *hit_enabled{ hits->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_hitmarker.m_enabled ) };
    gui::checkbox     *hit_damage{ hits->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Show damage" ), &g_cfg.m_hitmarker.m_show_damage ) };
    gui::combo_box    *hit_mode{ hits->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Rendering mode" ), HIT_RENDER_MODES, config::HITMARKER_MODE_COUNT,
        &g_cfg.m_hitmarker.m_render_mode ) };
    gui::color_picker *hit_color{ hits->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Base color" ), &g_cfg.m_hitmarker.m_color ) };
    gui::combo_box    *hit_event{ hits->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Event" ), HIT_EVENTS, config::HITMARKER_EVENT_COUNT,
        &g_cfg.m_hitmarker.m_edited_event ) };

    hit_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Marks damage you deal to players." ) );
    hit_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "World marks the player's position when damage arrives. 2D marks the center of the screen." ) );
    hit_event->set_tooltip( ASTRAL_XORSTR_STATIC( "Critical includes mini-crits. Kill takes priority when the death event arrives." ) );
    hit_damage->set_render_on( &g_cfg.m_hitmarker.m_enabled );
    hit_mode->set_render_on( &g_cfg.m_hitmarker.m_enabled );
    hit_color->set_render_on( &g_cfg.m_hitmarker.m_enabled );
    hit_event->set_render_on( &g_cfg.m_hitmarker.m_enabled );

    for ( usize index{ }; index < config::HITMARKER_EVENT_COUNT; ++index )
    {
        gui::checkbox     *override_color{ hits->add< gui::checkbox >(
            ASTRAL_XORSTR_STATIC( "Override color" ),
            &g_cfg.m_hitmarker.m_override_color[ index ] ) };
        gui::color_picker *event_color{ hits->add< gui::color_picker >(
            ASTRAL_XORSTR_STATIC( "Event color" ),
            &g_cfg.m_hitmarker.m_event_color[ index ] ) };

        override_color->set_render_on( HIT_EVENT_PREDICATES[ index ] );
        event_color->set_render_on( HIT_COLOR_PREDICATES[ index ] );
    }
}

static void build_world_visuals( gui::tab *visuals )
{
    gui::sub_tab *world{ visuals->add_subtab( ASTRAL_XORSTR_STATIC( "World" ) ) };

    gui::group_box *weather{ world->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Weather" ),
                                               gui::column_t::left ) };
    gui::group_box *sky{ world->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Skybox" ),
                                           gui::column_t::right ) };
    gui::group_box *fog{ world->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Fog" ),
                                           gui::column_t::left ) };
    gui::group_box *tint{ world->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Modulation" ),
                                            gui::column_t::left ) };
    gui::group_box *post{ world->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Post processing" ),
                                            gui::column_t::left ) };

    config::post_process_t &grade{ g_cfg.m_post_process };

    gui::checkbox   *post_enabled{ post->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ),
                                                               &grade.m_enabled ) };
    gui::slider_int *exposure{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Exposure" ),
                                                             &grade.m_exposure, -100, 100 ) };
    gui::slider_int *contrast{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Contrast" ),
                                                             &grade.m_contrast, 50, 200,
                                                             ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *saturation{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Saturation" ),
                                                               &grade.m_saturation, 0, 250,
                                                               ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *vibrance{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Vibrance" ),
                                                             &grade.m_vibrance, 0, 100,
                                                             ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *shadows{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Shadows" ),
                                                            &grade.m_shadows, -100, 100 ) };
    gui::slider_int *highlights{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Highlights" ),
                                                               &grade.m_highlights, -100, 100 ) };
    gui::slider_int *temperature{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Temperature" ),
                                                                &grade.m_temperature, -100, 100 ) };
    gui::slider_int *tint_shift{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Tint" ),
                                                               &grade.m_tint, -100, 100 ) };
    gui::slider_int *vignette{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Vignette" ),
                                                             &grade.m_vignette, 0, 100,
                                                             ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *bloom{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Bloom" ),
                                                          &grade.m_bloom, 0, 200,
                                                          ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *threshold{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Bloom threshold" ),
                                                              &grade.m_bloom_threshold, 0, 90,
                                                              ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *motion_blur{ post->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Motion blur" ),
                                                                &grade.m_motion_blur, 0, 400,
                                                                ASTRAL_XORSTR_STATIC( "%" ) ) };

    post_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Grades the game's frame like a video editor would, so recordings come out finished. The game HUD, chams and glow are graded with it, the Astral menu and ESP overlay are not." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "post processing color grading colour saturation contrast shadows bloom motion blur video cinematic" ) );

    temperature->set_tooltip( ASTRAL_XORSTR_STATIC( "Below zero pushes the frame towards blue, above zero towards orange." ) );
    threshold->set_tooltip( ASTRAL_XORSTR_STATIC( "How bright a pixel has to be before it starts to glow." ) );
    motion_blur->set_tooltip( ASTRAL_XORSTR_STATIC( "Strength of the engine's camera motion blur. Zero leaves the game's own setting alone." ) );

    exposure->set_render_on( &post_processed );
    contrast->set_render_on( &post_processed );
    saturation->set_render_on( &post_processed );
    vibrance->set_render_on( &post_processed );
    shadows->set_render_on( &post_processed );
    highlights->set_render_on( &post_processed );
    temperature->set_render_on( &post_processed );
    tint_shift->set_render_on( &post_processed );
    vignette->set_render_on( &post_processed );
    bloom->set_render_on( &post_processed );
    threshold->set_render_on( &bloom_enabled );
    motion_blur->set_render_on( &post_processed );

    gui::dynamic_list_box *sky_list{ sky->add< gui::dynamic_list_box >(
        &g_cfg.m_skybox.m_index ) };

    g_skybox.bind_list( sky_list );

    static const char *const FOG_MODES[]{ ASTRAL_XORSTR_STATIC( "Off" ),
                                          ASTRAL_XORSTR_STATIC( "Remove" ),
                                          ASTRAL_XORSTR_STATIC( "Custom" ) };

    gui::combo_box    *fog_mode{ fog->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Mode" ),
                                                             FOG_MODES, config::FOG_COUNT,
                                                             &g_cfg.m_fog.m_mode ) };
    gui::slider_int   *fog_start{ fog->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Start" ), &g_cfg.m_fog.m_start, 0, 8000 ) };
    gui::slider_int   *fog_end{ fog->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "End" ),
                                                             &g_cfg.m_fog.m_end, 0, 16000 ) };
    gui::slider_int   *fog_density{ fog->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Density" ), &g_cfg.m_fog.m_density, 0, 100,
        ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::color_picker *fog_color{ fog->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Color" ), &g_cfg.m_fog.m_color, false ) };
    gui::checkbox     *fog_sky{ fog->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Reach skybox" ),
                                                           &g_cfg.m_fog.m_skybox ) };

    fog_mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Remove takes the map's fog out entirely. Custom keeps fog but drives its distance, density and colour." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fog haze mist distance density remove" ) );

    fog_start->set_render_on( &fog_shaped );
    fog_end->set_render_on( &fog_shaped );
    fog_density->set_render_on( &fog_shaped );
    fog_color->set_render_on( &fog_shaped );
    fog_sky->set_render_on( &fog_driven );

    gui::checkbox     *tint_enabled{ tint->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Recolor world" ), &g_cfg.m_world.m_modulate ) };
    gui::color_picker *tint_color{ tint->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Color" ),
                                                                   &g_cfg.m_world.m_color, false ) };
    gui::checkbox     *tint_sky{ tint->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Reach skybox" ),
                                                             &g_cfg.m_world.m_skybox ) };
    gui::checkbox     *tint_props{ tint->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Reach props" ),
                                                               &g_cfg.m_world.m_props ) };

    tint_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Recolours the map's brushwork, plus the skybox and the map props when those are ticked. Players, weapons and buildings are never touched." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "world modulation nightmode colour color map tint" ) );

    tint_props->set_tooltip( ASTRAL_XORSTR_STATIC( "Map props only. They share a texture group with players and weapons, so the split is made on the material path." ) );

    tint_color->set_render_on( &world_modulated );
    tint_sky->set_render_on( &world_modulated );
    tint_props->set_render_on( &world_modulated );

    static const char *const WEATHER_TYPES[]{ ASTRAL_XORSTR_STATIC( "Off" ),
                                              ASTRAL_XORSTR_STATIC( "Rain" ),
                                              ASTRAL_XORSTR_STATIC( "Snow" ) };

    gui::combo_box  *type{ weather->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Type" ),
                                                           WEATHER_TYPES, 3,
                                                           &g_cfg.m_weather.m_type ) };
    gui::slider_int *density{ weather->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Density" ), &g_cfg.m_weather.m_density, 0, 100,
        ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *wind{ weather->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Wind speed" ), &g_cfg.m_weather.m_wind_speed, 0, 500 ) };
    gui::slider_int *direction{ weather->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Wind direction" ), &g_cfg.m_weather.m_wind_direction, 0, 360 ) };

    type->set_tooltip( ASTRAL_XORSTR_STATIC( "Replaces whatever the map ships with." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "weather rain snow precipitation" ) );
    density->set_render_on( &weather_enabled );
    wind->set_render_on( &weather_enabled );
    direction->set_render_on( &weather_enabled );
}

static void build_local_visuals( gui::tab *visuals )
{
    gui::sub_tab *local{ visuals->add_subtab( ASTRAL_XORSTR_STATIC( "Misc" ) ) };

    gui::group_box *tracers{ local->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Bullet tracers" ), gui::column_t::left ) };
    gui::group_box *view{ local->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Viewmodel" ), gui::column_t::left ) };
    gui::group_box *incoming{ local->add_group( ICON_TRIANGLE_ALERT, ASTRAL_XORSTR_STATIC( "Projectile warning" ), gui::column_t::left ) };
    gui::group_box *world_feel{ local->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Screen" ), gui::column_t::right ) };
    gui::group_box *arrows{ local->add_group( ICON_CHEVRON_UP, ASTRAL_XORSTR_STATIC( "Out of view" ), gui::column_t::right ) };
    gui::group_box *streamer_mode{ local->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Streamer mode" ), gui::column_t::right ) };

    static const char *const REMOVAL_LABELS[]{
        ASTRAL_XORSTR_STATIC( "Viewpunch" ),
        ASTRAL_XORSTR_STATIC( "Team push" ),
        ASTRAL_XORSTR_STATIC( "Post processing" ),
        ASTRAL_XORSTR_STATIC( "Motion blur" ),
        ASTRAL_XORSTR_STATIC( "Condition overlays" ),
        ASTRAL_XORSTR_STATIC( "Footsteps" ),
        ASTRAL_XORSTR_STATIC( "Disguises" ),
        ASTRAL_XORSTR_STATIC( "Taunting players" ),
        ASTRAL_XORSTR_STATIC( "Scope zoom" ),
        ASTRAL_XORSTR_STATIC( "Scope overlay" ),
        ASTRAL_XORSTR_STATIC( "Taunt restrictions" ),
        ASTRAL_XORSTR_STATIC( "MOTDs" ),
        ASTRAL_XORSTR_STATIC( "Equip region restrictions" ),
        ASTRAL_XORSTR_STATIC( "Kart cage" ),
        ASTRAL_XORSTR_STATIC( "Kart turn restrictions" ),
        ASTRAL_XORSTR_STATIC( "Item notifications" ),
    };

    gui::combo_box *removals{ world_feel->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Removals" ), REMOVAL_LABELS, config::REMOVAL_COUNT, g_cfg.m_misc.m_removals ) };
    gui::checkbox  *clean_screenshots{ world_feel->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Clean screenshots" ), &g_cfg.m_misc.m_clean_screenshots ) };
    gui::checkbox  *clean_demos{ world_feel->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Clean demos" ), &g_cfg.m_misc.m_clean_demos ) };
    gui::checkbox  *giant_sounds{ world_feel->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Giant weapon sounds" ), &g_cfg.m_misc.m_giant_weapon_sounds ) };
    gui::checkbox  *apply_to_alternative{ world_feel->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Apply to alternative weapons" ), &g_cfg.m_misc.m_apply_to_alternate_weapons ) };

    clean_screenshots->set_tooltip( ASTRAL_XORSTR_STATIC( "Prevents critical visual features from showing up in engine screenshots." ) );

    clean_demos->set_tooltip( ASTRAL_XORSTR_STATIC( "Records your real view angles in demos instead of the ragebot and anti-aim ones." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "demo record pov hide ragebot anti-aim angles" ) );

    giant_sounds->set_tooltip( ASTRAL_XORSTR_STATIC( "Your weapons fire with the sounds the MvM giant robots use. Only you hear them." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "mvm giant robot weapon shoot fire sound" ) );

    apply_to_alternative->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows giant weapon sounds to play on weapons that share the same weapon ID but do not play giant weapon sounds." ) );
    apply_to_alternative->set_render_on( &g_cfg.m_misc.m_giant_weapon_sounds );

    gui::slider_int *weapon_pitch{ world_feel->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Weapon pitch" ), &g_cfg.m_misc.m_weapon_pitch, 50, 200, ASTRAL_XORSTR_STATIC( "%" ) ) };

    weapon_pitch->set_tooltip( ASTRAL_XORSTR_STATIC( "Raises or lowers the pitch of every weapon sound you hear. 100% leaves them untouched." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "weapon gun shoot fire sound pitch audio" ) );

    removals->set_tooltip( ASTRAL_XORSTR_STATIC( "Removes certain quirks and effects that the client/engine applies." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "remove viewpunch recoil push post processing motion blur overlay tint invuln jarate milk bleed stealth bonk gas burning footstep sound mute removals taunt scope overlay zoom kart bumper car cage halloween" ) );

    gui::checkbox   *view_offset{ view->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Offset" ), &g_cfg.m_viewmodel.m_enabled ) };
    gui::slider_int *view_forward{ view->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Forward" ), &g_cfg.m_viewmodel.m_forward, -30, 30 ) };
    gui::slider_int *view_right{ view->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Right" ), &g_cfg.m_viewmodel.m_right, -30, 30 ) };
    gui::slider_int *view_up{ view->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Up" ), &g_cfg.m_viewmodel.m_up, -30, 30 ) };
    gui::slider_int *view_bob{ view->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Bobbing" ), &g_cfg.m_viewmodel.m_bob, 0, 200, ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *view_sway{ view->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Sway" ), &g_cfg.m_viewmodel.m_sway, 0, 200, ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::checkbox   *view_aim{ view->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Follow ragebot" ), &g_cfg.m_viewmodel.m_aim ) };
    gui::slider_int *view_swing{ view->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Swing" ), &g_cfg.m_viewmodel.m_aim_swing, 0, 400, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    gui::checkbox   *view_show_in_scope{ view->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Show in scope" ), &g_cfg.m_viewmodel.m_show_in_scope ) };

    view_offset->set_tooltip( ASTRAL_XORSTR_STATIC( "Moves the weapon model along the way you are looking." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "viewmodel offset position weapon" ) );

    view_forward->set_render_on( &g_cfg.m_viewmodel.m_enabled );
    view_right->set_render_on( &g_cfg.m_viewmodel.m_enabled );
    view_up->set_render_on( &g_cfg.m_viewmodel.m_enabled );

    view_bob->set_tooltip( ASTRAL_XORSTR_STATIC( "Scales how far the weapon swings as you walk. "
                                                "Zero holds it still, past a hundred exaggerates "
                                                "it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "viewmodel bob bobbing walk" ) );

    view_sway->set_tooltip( ASTRAL_XORSTR_STATIC( "Lets the weapon trail behind the camera when "
                                                 "you turn. The game ships this switched off, so "
                                                 "a hundred is what it was tuned for." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "viewmodel sway lag drift turn" ) );

    view_aim->set_tooltip( ASTRAL_XORSTR_STATIC( "Points the weapon model where the ragebot is "
                                                "firing. The camera and the command sent are "
                                                "left alone, so this only changes what you see." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "viewmodel ragebot silent aim follow weapon" ) );

    view_swing->set_tooltip( ASTRAL_XORSTR_STATIC( "How long the model takes to swing onto a new "
                                                  "target, and back off once the aim is gone." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "viewmodel aim smoothing swing time" ) );
    view_swing->set_render_on( &g_cfg.m_viewmodel.m_aim );

    view_show_in_scope->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows your viewmodel to draw while scoped.\n"
                                                          "NOTE: This option ONLY applies when both scope overlay and scope zoom removals are turned on!" ) );
    view_show_in_scope->set_render_on( &is_scopezoom_removal_enabled );

    static const char *const TRACER_STYLES[]{
        ASTRAL_XORSTR_STATIC( "Lines" ), ASTRAL_XORSTR_STATIC( "Beams" ),
        ASTRAL_XORSTR_STATIC( "Merasmus" ), ASTRAL_XORSTR_STATIC( "Big nasty" ),
        ASTRAL_XORSTR_STATIC( "Raygun" ), ASTRAL_XORSTR_STATIC( "Machina" ),
        ASTRAL_XORSTR_STATIC( "Lightning" )
    };

    static const char *const IMPACT_STYLES[]{
        ASTRAL_XORSTR_STATIC( "Box" ),
        ASTRAL_XORSTR_STATIC( "Circle" ),
    };

    gui::checkbox     *enabled{ tracers->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_tracers.m_enabled ) };
    gui::combo_box    *style{ tracers->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Style" ), TRACER_STYLES, config::TRACER_STYLE_COUNT, &g_cfg.m_tracers.m_style ) };
    gui::checkbox     *local_only{ tracers->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Local player only" ), &g_cfg.m_tracers.m_local_only ) };
    gui::color_picker *local_tint{ tracers->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Local tracers" ), &g_cfg.m_tracers.m_local_color ) };
    gui::color_picker *local_tint_impact{ tracers->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Local tracer impacts" ), &g_cfg.m_tracers.m_local_impact_color ) };
    gui::color_picker *entity_tint{ tracers->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Entity tracers" ), &g_cfg.m_tracers.m_entity_color ) };
    gui::color_picker *entity_tint_impact{ tracers->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Entity tracer impacts" ), &g_cfg.m_tracers.m_entity_impact_color ) };
    gui::slider_int   *local_duration{ tracers->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Local duration" ), &g_cfg.m_tracers.m_local_duration, 100, 3000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    gui::slider_int   *friend_duration{ tracers->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Friend duration" ), &g_cfg.m_tracers.m_friend_duration, 100, 3000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    gui::slider_int   *teammate_duration{ tracers->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Teammate duration" ), &g_cfg.m_tracers.m_teammate_duration, 100, 3000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    gui::slider_int   *enemy_duration{ tracers->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Enemy duration" ), &g_cfg.m_tracers.m_enemy_duration, 100, 3000, ASTRAL_XORSTR_STATIC( "ms" ) ) };
    gui::checkbox     *impact{ tracers->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Impact marker" ), &g_cfg.m_tracers.m_impact_marker ) };
    gui::combo_box    *impact_marker_style{ tracers->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Impact style" ), IMPACT_STYLES, config::IMPACT_STYLE_COUNT, &g_cfg.m_tracers.m_impact_style ) };

    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the path of every bullet fired, "
                                               "replacing the tracer the game would show." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "bullet tracer beam trail shot" ) );

    style->set_tooltip( ASTRAL_XORSTR_STATIC( "Lines use our renderer and beams use the engine's beam system. The rest are game particles tinted by the shooter's team." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "tracer style particle beam merasmus raygun "
                                             "machina lightning lines" ) );

    local_only->set_tooltip( ASTRAL_XORSTR_STATIC( "Off also traces the shots everyone else "
                                                  "fires." ) );

    impact->set_tooltip( ASTRAL_XORSTR_STATIC( "Marks where your bullet landed in the world." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "impact hit square box marker" ) );

    impact_marker_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Adjusts the style in which the impact marker is drawn. \"Box\" draws a 3D hull box where the impact was, where as \"Circle\" draws a 2D circle overlay where the impact was." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "impact style circle box hull bullet 3d 2d" ) );

    local_tint->set_render_on( &tracers_drawn_by_us );
    entity_tint->set_render_on( &tracers_colour_entities );
    local_duration->set_render_on( &tracers_drawn_by_us );
    friend_duration->set_render_on( &tracers_colour_entities );
    teammate_duration->set_render_on( &tracers_colour_entities );
    enemy_duration->set_render_on( &tracers_colour_entities );
    impact->set_render_on( &tracer_lines );
    impact_marker_style->set_render_on( &impact_condition );

    local_tint_impact->set_render_on( &impact_condition );
    entity_tint_impact->set_render_on( &tracer_impact_entities );

    gui::checkbox     *arrows_enabled{ arrows->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_arrows.m_enabled ) };
    static const char *const ARROW_MODES[]{ ASTRAL_XORSTR_STATIC( "All enemies" ),
                                            ASTRAL_XORSTR_STATIC( "Spies only" ),
                                            ASTRAL_XORSTR_STATIC( "Spy alert" ) };
    gui::combo_box    *arrows_mode{ arrows->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Mode" ), ARROW_MODES,
                                                                   config::arrows_t::MODE_COUNT, &g_cfg.m_arrows.m_mode ) };
    gui::color_picker *arrows_color{ arrows->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Color" ), &g_cfg.m_arrows.m_color ) };
    gui::color_picker *arrows_spy_color{ arrows->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Spy alert color" ), &g_cfg.m_arrows.m_spy_color ) };
    gui::slider_int   *arrows_radius{ arrows->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Radius" ), &g_cfg.m_arrows.m_radius, 60, 500, ASTRAL_XORSTR_STATIC( "px" ) ) };
    gui::slider_int   *arrows_size{ arrows->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Size" ), &g_cfg.m_arrows.m_size, 8, 40, ASTRAL_XORSTR_STATIC( "px" ) ) };
    gui::checkbox     *arrows_distance{ arrows->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Distance" ), &g_cfg.m_arrows.m_distance ) };

    arrows_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Points a chevron at every enemy the camera is "
                                                      "not looking at, on a ring around the crosshair. "
                                                      "One behind you sits at the bottom of the ring." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "out of view fov arrow chevron indicator offscreen radar" ) );

    arrows_radius->set_tooltip( ASTRAL_XORSTR_STATIC( "How far out from the crosshair the ring is." ) );
    arrows_distance->set_tooltip( ASTRAL_XORSTR_STATIC( "Writes how far away the enemy is on the inside "
                                                       "of the ring." ) );

    arrows_mode->set_render_on( &g_cfg.m_arrows.m_enabled );
    arrows_color->set_render_on( &g_cfg.m_arrows.m_enabled );
    arrows_spy_color->set_render_on( &g_cfg.m_arrows.m_enabled );
    arrows_radius->set_render_on( &g_cfg.m_arrows.m_enabled );
    arrows_size->set_render_on( &g_cfg.m_arrows.m_enabled );
    arrows_distance->set_render_on( &g_cfg.m_arrows.m_enabled );

    static const char *const WARNING_STYLES[]{ ASTRAL_XORSTR_STATIC( "Badge and timer" ), ASTRAL_XORSTR_STATIC( "Trail and impact" ),
                                               ASTRAL_XORSTR_STATIC( "Impact zone" ), ASTRAL_XORSTR_STATIC( "Crosshair radar" ),
                                               ASTRAL_XORSTR_STATIC( "Combined" ) };

    config::proj_warning_t &warning{ g_cfg.m_projectile_warning };

    gui::checkbox  *warning_enabled{ incoming->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &warning.m_enabled ) };
    gui::combo_box *warning_style{ incoming->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Style" ), WARNING_STYLES,
                                                                    config::PROJECTILE_WARNING_STYLE_COUNT, &warning.m_style ) };
    gui::checkbox  *warning_threats{ incoming->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Only threats to you" ), &warning.m_only_threats ) };
    gui::checkbox  *warning_resting{ incoming->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Resting stickies" ), &warning.m_resting_stickies ) };

    warning_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Warns about enemy rockets, stickies, flares and arrows in flight: where "
                                                       "each one lands and how long you have before it does, timed against the server "
                                                       "frame your next command runs in." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "projectile warning incoming rocket sticky pipe grenade indicator danger" ) )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Threat" ), &warning.m_color, false );
    warning_enabled->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Danger" ), &warning.m_danger_color, false );

    warning_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Badge follows the projectile with a countdown ring. Trail draws its path to the "
                                                     "impact. Impact zone grows a ring out to the blast edge as the impact nears. Crosshair radar "
                                                     "points at it around the crosshair. Combined is the trail, the zone edge and the "
                                                     "badge together. The danger colour means the blast reaches where you stand." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "warning style badge trail zone radar combined" ) )
        ->set_render_on( &warning.m_enabled );

    warning_threats->set_tooltip( ASTRAL_XORSTR_STATIC( "Hides projectiles whose blast or direct hit would not reach where you stand." ) )
        ->set_render_on( &warning.m_enabled );

    warning_resting->set_tooltip( ASTRAL_XORSTR_STATIC( "Keeps warning about enemy stickies that have come to rest, with the time left "
                                                       "until they are armed." ) )
        ->set_render_on( &warning.m_enabled );

    static const char *const STREAMER_MODE_TARGETS[]{
        ASTRAL_XORSTR_STATIC( "Local player" ),
        ASTRAL_XORSTR_STATIC( "Enemy players" ),
        ASTRAL_XORSTR_STATIC( "Teammates" ),
        ASTRAL_XORSTR_STATIC( "Friends" ),
        ASTRAL_XORSTR_STATIC( "Party members" ),
        ASTRAL_XORSTR_STATIC( "Astral users" ),
        ASTRAL_XORSTR_STATIC( "Sensitive panels" ),
    };

    streamer_mode->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_streamer_mode.m_enabled )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Hides certain sensitive in game information to prevent stream sniping or identification. Spectators and unassigned players are hidden by default while this is enabled." ) );
    streamer_mode->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Hide targets" ), STREAMER_MODE_TARGETS, config::STREAMER_MODE_HIDE_COUNT, g_cfg.m_streamer_mode.m_hide_targets )
        ->set_render_on( &g_cfg.m_streamer_mode.m_enabled );
    streamer_mode->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Stream proof" ), &g_cfg.m_streamer_mode.m_stream_proof )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the menu, ESP and other overlays in a window that OBS, Discord and screenshots skip. Needs windowed or borderless mode on Windows 10 2004 or newer. Chams, glow and tracers are drawn within engine space and still show on stream." ) );
}

static void build_visuals( )
{
    gui::tab *visuals{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Visuals" ), ICON_EYE ) };

    build_player_visuals( visuals );
    build_world_visuals( visuals );
    build_local_visuals( visuals );

    gui::sub_tab   *radar{ visuals->add_subtab( ASTRAL_XORSTR_STATIC( "Radar" ) ) };
    gui::group_box *general{ radar->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Radar" ), gui::column_t::left ) };
    gui::group_box *entities{ radar->add_group( ICON_PLAYERS, ASTRAL_XORSTR_STATIC( "Entities" ), gui::column_t::right ) };
    gui::group_box *map{ radar->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Automap" ), gui::column_t::left ) };
    gui::group_box *colors{ radar->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Colors" ), gui::column_t::right ) };

    general->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_radar.m_enabled )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows a draggable radar containing selected visual entities and optional collision-map geometry." ) );
    general->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Size" ), &g_cfg.m_radar.m_size, 180, 480, ASTRAL_XORSTR_STATIC( "px" ) );
    general->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Range" ), &g_cfg.m_radar.m_range, 500, 8000, ASTRAL_XORSTR_STATIC( "u" ) );
    general->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Icon size" ), &g_cfg.m_radar.m_icon_size, 10, 32, ASTRAL_XORSTR_STATIC( "px" ) );
    general->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Rotate with view" ), &g_cfg.m_radar.m_rotate );
    static const char *const RADAR_ICON_STYLES[]{
        ASTRAL_XORSTR_STATIC( "Regular icons" ), ASTRAL_XORSTR_STATIC( "Class icons" ),
        ASTRAL_XORSTR_STATIC( "Simple dots" )
    };
    general->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Player icons" ), RADAR_ICON_STYLES,
                                    config::RADAR_ICON_COUNT, &g_cfg.m_radar.m_icon_style )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Uses standard player glyphs, TF2 class portraits, or compact colored dots." ) );

    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Local player" ), &g_cfg.m_radar.m_show_local );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enemies" ), &g_cfg.m_radar.m_show_enemies );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Teammates" ), &g_cfg.m_radar.m_show_teammates );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Dormant players" ), &g_cfg.m_radar.m_show_dormant );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Player names" ), &g_cfg.m_radar.m_show_names );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Buildings" ), &g_cfg.m_radar.m_show_buildings );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Projectiles" ), &g_cfg.m_radar.m_show_projectiles );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Pickups" ), &g_cfg.m_radar.m_show_pickups );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Objectives" ), &g_cfg.m_radar.m_show_objectives );
    entities->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "NPCs" ), &g_cfg.m_radar.m_show_npcs );

    static const char *const RADAR_MAP_MODES[]{
        ASTRAL_XORSTR_STATIC( "Off" ), ASTRAL_XORSTR_STATIC( "DOOM" ), ASTRAL_XORSTR_STATIC( "MSG" )
    };
    map->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Map style" ), RADAR_MAP_MODES,
                                config::RADAR_MAP_COUNT, &g_cfg.m_radar.m_map_mode );
    map->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Map detail" ), &g_cfg.m_radar.m_map_detail, 1, 3 )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Controls how many nearby floors are cached. DOOM uses solid layered floors; MSG uses the tilted wireframe." ) );

    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Map" ), &g_cfg.m_radar.m_map_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Local player" ), &g_cfg.m_radar.m_local_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Enemies" ), &g_cfg.m_radar.m_enemy_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Teammates" ), &g_cfg.m_radar.m_teammate_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Buildings" ), &g_cfg.m_radar.m_building_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Projectiles" ), &g_cfg.m_radar.m_projectile_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Pickups" ), &g_cfg.m_radar.m_pickup_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Objectives" ), &g_cfg.m_radar.m_objective_color );
    colors->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "NPCs" ), &g_cfg.m_radar.m_npc_color );
}

struct player_list_row_t
{
    char m_name[ 64 ]{ };
    char m_real_name[ 64 ]{ };
    u32  m_account{ };
    i32  m_class{ };
    i32  m_team{ };
    i32  m_ping{ };
    u8   m_tag{ };
    u32  m_marks{ };
    u16  m_groups{ };
    bool m_hidden{ };
    bool m_aliased{ };
    bool m_cloud{ };
    bool m_astral{ };
    u8   m_party{ };
    char m_party_label[ 12 ]{ };
};

struct player_chip_t
{
    const char *m_label{ };
    color_t     m_tint{ };
};

struct chip_flow_t
{
    point_t m_origin{ };
    i32     m_room{ };
    i32     m_x{ };
    i32     m_y{ };

    rect_t place( i32 width )
    {
        const i32 gap{ g_gui.scaled( 3 ) };
        if ( this->m_x && this->m_x + width > this->m_room )
        {
            this->m_x = 0;
            this->m_y += g_gui.scaled( WIDGET_CHIP_HEIGHT ) + gap;
        }

        const i32 left{ this->m_origin.x + this->m_x };
        const i32 top{ this->m_origin.y + this->m_y };
        this->m_x += width + gap;
        return rect_t{ left, top, left + width, top + g_gui.scaled( WIDGET_CHIP_HEIGHT ) };
    }
};

static constexpr usize PLAYER_CHIP_COUNT{ config::PLAYER_GROUP_COUNT + 16 };
static constexpr usize PLAYER_MARK_COUNT{ 13 };

static constexpr u8    PLAYER_STATUS_COUNT{ config::PLAYER_TAG_TARGET + 1 };
static constexpr u8    PLAYER_MARK_FILTER_BASE{ PLAYER_STATUS_COUNT };
static constexpr u8    PLAYER_GROUP_FILTER_BASE{ PLAYER_MARK_FILTER_BASE + 13 };
static constexpr usize PLAYER_FILTER_COUNT{ PLAYER_GROUP_FILTER_BASE + config::PLAYER_GROUP_COUNT };

static constexpr color_t PLAYER_GROUP_PALETTE[ config::PLAYER_GROUP_COUNT ]{
    { 255, 170, 60 },
    { 90, 170, 255 },
    { 235, 110, 235 },
    { 255, 230, 90 },
    { 120, 230, 230 },
    { 150, 220, 100 },
    { 255, 130, 160 },
    { 180, 150, 255 },
    { 255, 200, 140 },
    { 110, 210, 170 },
    { 200, 160, 120 },
    { 140, 190, 255 },
    { 255, 150, 90 },
    { 210, 210, 120 },
    { 170, 120, 220 },
    { 200, 200, 205 },
};

static player_list_row_t s_player_rows[ 64 ]{ };
static usize             s_player_count{ };
static u32               s_selected_account{ };
static u8                s_player_filter{ };
static usize             s_selected_group{ config::PLAYER_GROUP_COUNT };
static usize             s_shown_group{ config::PLAYER_GROUP_COUNT };
static bool              s_group_shown[ config::PLAYER_GROUP_COUNT ]{ };
static gui::text_input  *s_alias_input{ };
static gui::text_input  *s_report_reason_input{ };
static gui::text_input  *s_new_group_input{ };
static gui::text_input  *s_group_name_input{ };
static gui::text_input  *s_steam_api_key_input{ };
static gui::text_input  *s_steam_history_key_input{ };
static gui::text_input  *s_tf2pl_path_input{ };

static const char *player_tag_label( u8 tag )
{
    switch ( tag )
    {
        case config::PLAYER_TAG_FRIEND: return ASTRAL_XORSTR_STATIC( "Friend" );
        case config::PLAYER_TAG_IGNORED: return ASTRAL_XORSTR_STATIC( "Ignored" );
        case config::PLAYER_TAG_TARGET: return ASTRAL_XORSTR_STATIC( "Target" );
        default: return ASTRAL_XORSTR_STATIC( "None" );
    }
}

static color_t player_tag_color( u8 tag )
{
    switch ( tag )
    {
        case config::PLAYER_TAG_FRIEND: return color_t{ 100, 215, 110 };
        case config::PLAYER_TAG_IGNORED: return color_t{ 170, 170, 175 };
        case config::PLAYER_TAG_TARGET: return color_t{ 255, 100, 100 };
        default: return MENU_TEXT_DIM;
    }
}

static const char *player_mark_label( usize mark )
{
    static const char *labels[]{
        ASTRAL_XORSTR_STATIC( "Cheater" ), ASTRAL_XORSTR_STATIC( "Suspicious" ),
        ASTRAL_XORSTR_STATIC( "Exploiter" ), ASTRAL_XORSTR_STATIC( "Racist" ),
        ASTRAL_XORSTR_STATIC( "Hostile" ), ASTRAL_XORSTR_STATIC( "Suspected cheater" ),
        ASTRAL_XORSTR_STATIC( "Blacklisted" ), ASTRAL_XORSTR_STATIC( "VAC banned" ),
        ASTRAL_XORSTR_STATIC( "Game banned" ), ASTRAL_XORSTR_STATIC( "SourceBanned" ),
        ASTRAL_XORSTR_STATIC( "Pedophilia" ), ASTRAL_XORSTR_STATIC( "UList user" ),
        ASTRAL_XORSTR_STATIC( "Zoophile/zoo jokes" )
    };
    return mark < sizeof( labels ) / sizeof( labels[ 0 ] ) ? labels[ mark ] : ASTRAL_XORSTR_STATIC( "Mark" );
}

static color_t player_mark_color( usize mark )
{
    static constexpr color_t colors[]{
        { 255, 90, 90 }, { 255, 175, 70 }, { 235, 120, 255 }, { 255, 120, 150 }, { 235, 90, 90 }, { 255, 205, 80 }, { 175, 80, 80 }, { 235, 75, 75 }, { 235, 110, 75 }, { 255, 140, 70 }, { 200, 90, 150 }, { 120, 160, 255 }, { 180, 100, 180 }
    };
    return mark < sizeof( colors ) / sizeof( colors[ 0 ] ) ? colors[ mark ] : MENU_TEXT_DIM;
}

static const char *player_class_label( i32 player_class )
{
    switch ( player_class )
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
        default: return ASTRAL_XORSTR_STATIC( "Player" );
    }
}

static const char *player_group_name( usize index )
{
    return reinterpret_cast< const char * >( g_cfg.m_player_groups[ index ].m_name );
}

static bool player_group_defined( usize index )
{
    return g_cfg.m_player_groups[ index ].m_name[ 0 ] != 0;
}

static const char *player_filter_label( u8 filter )
{
    if ( filter >= PLAYER_MARK_FILTER_BASE && filter < PLAYER_GROUP_FILTER_BASE )
    {
        return player_mark_label( filter - PLAYER_MARK_FILTER_BASE );
    }
    switch ( filter )
    {
        case config::PLAYER_TAG_NONE: return ASTRAL_XORSTR_STATIC( "All" );
        case config::PLAYER_TAG_FRIEND: return ASTRAL_XORSTR_STATIC( "Friends" );
        case config::PLAYER_TAG_IGNORED: return ASTRAL_XORSTR_STATIC( "Ignored" );
        case config::PLAYER_TAG_TARGET: return ASTRAL_XORSTR_STATIC( "Targets" );
        default: return player_group_name( filter - PLAYER_GROUP_FILTER_BASE );
    }
}

static bool matches_filter( const player_list_row_t &row, u8 filter )
{
    if ( filter >= PLAYER_GROUP_FILTER_BASE )
    {
        return ( row.m_groups & ( 1u << ( filter - PLAYER_GROUP_FILTER_BASE ) ) ) != 0;
    }
    if ( filter >= PLAYER_MARK_FILTER_BASE )
    {
        return ( row.m_marks & ( 1u << ( filter - PLAYER_MARK_FILTER_BASE ) ) ) != 0;
    }

    return filter == config::PLAYER_TAG_NONE || row.m_tag == filter;
}

static i32 card_inner_width( )
{
    return g_gui.scaled( MENU_COLUMN_WIDTH ) - g_gui.scaled( CARD_PADDING_X ) * 2;
}

static usize player_chips( const player_list_row_t &row, player_chip_t ( &chips )[ PLAYER_CHIP_COUNT ] )
{
    usize count{ };
    if ( row.m_tag )
    {
        chips[ count++ ] = { player_tag_label( row.m_tag ), player_tag_color( row.m_tag ) };
    }

    for ( usize mark{ }; mark < PLAYER_MARK_COUNT; ++mark )
    {
        if ( row.m_marks & ( 1u << mark ) )
        {
            chips[ count++ ] = { player_mark_label( mark ), player_mark_color( mark ) };
        }
    }

    for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
    {
        if ( row.m_groups & ( 1u << index ) )
        {
            chips[ count++ ] = { player_group_name( index ), g_cfg.m_player_groups[ index ].m_color };
        }
    }

    if ( row.m_astral )
    {
        chips[ count++ ] = { ASTRAL_XORSTR_STATIC( "AstralMod" ), color_t{ 150, 120, 255 } };
    }

    if ( row.m_party )
    {
        chips[ count++ ] = { row.m_party_label, client::party_color( row.m_party ) };
    }

    return count;
}

static i32 player_chip_width( const player_chip_t &chip )
{
    return gui::estimate_tiny_text_width( chip.m_label ) + g_gui.scaled( 8 );
}

static void draw_player_chip( const rect_t &bounds, const player_chip_t &chip )
{
    g_render.box( bounds, g_gui.color( gui::with_alpha( chip.m_tint, 24 ) ), true );
    g_render.box( bounds, g_gui.color( gui::with_alpha( chip.m_tint, 130 ) ) );
    g_render.text( MENU_FONT_TINY( ),
                   point_t{ ( bounds.left + bounds.right ) / 2, bounds.top + g_gui.scaled( 3 ) },
                   g_gui.color( chip.m_tint ), chip.m_label, false, render::text_align::center );
}

static const player_list_row_t *connected_player( u32 account )
{
    for ( usize index{ }; index < s_player_count; ++index )
    {
        if ( s_player_rows[ index ].m_account == account )
        {
            return &s_player_rows[ index ];
        }
    }
    return nullptr;
}

static const player_list_row_t *selected_player( )
{
    return connected_player( s_selected_account );
}

static void refresh_player_list( );

static bool selected_is_cheater( )
{
    const player_list_row_t *player{ selected_player( ) };
    return player && ( player->m_marks & config::PLAYER_MARK_CHEATING );
}

static void toggle_selected_player_mark( u32 mark )
{
    const player_list_row_t *player{ selected_player( ) };
    if ( !player )
        return;
    char      alias[ 129 ]{ };
    const u32 imported{ g_player_cloud.marks( player->m_account, alias ) };
    bool      changed{ };
    if ( imported & mark )
    {
        changed = g_player_cloud.remove_mark( player->m_account, mark );
    } else
    {
        changed = g_cfg.set_player_marks( player->m_account,
                                          g_cfg.player_marks( player->m_account ) ^ mark );
    }
    push_notification( changed ? notify_kind_t::success : notify_kind_t::warning,
                       ICON_PLAYERS, ASTRAL_XORSTR_STATIC( "Player mark" ),
                       changed ? ASTRAL_XORSTR_STATIC( "Player mark updated" )
                               : ASTRAL_XORSTR_STATIC( "Could not update player mark" ) );
    refresh_player_list( );
    g_gui.invalidate_layout( );
}

#define PLAYER_MARK_CALLBACK( name, value )   \
    static void name( )                       \
    {                                         \
        toggle_selected_player_mark( value ); \
    }
PLAYER_MARK_CALLBACK( toggle_mark_cheater, config::PLAYER_MARK_CHEATER )
PLAYER_MARK_CALLBACK( toggle_mark_suspicious, config::PLAYER_MARK_SUSPICIOUS )
PLAYER_MARK_CALLBACK( toggle_mark_exploiter, config::PLAYER_MARK_EXPLOITER )
PLAYER_MARK_CALLBACK( toggle_mark_racist, config::PLAYER_MARK_RACIST )
PLAYER_MARK_CALLBACK( toggle_mark_hostile, config::PLAYER_MARK_HOSTILE )
PLAYER_MARK_CALLBACK( toggle_mark_suspected, config::PLAYER_MARK_SUSPECTED_CHEATER )
PLAYER_MARK_CALLBACK( toggle_mark_blacklisted, config::PLAYER_MARK_BLACKLISTED )
PLAYER_MARK_CALLBACK( toggle_mark_vac, config::PLAYER_MARK_VAC_BANNED )
PLAYER_MARK_CALLBACK( toggle_mark_game, config::PLAYER_MARK_GAME_BANNED )
PLAYER_MARK_CALLBACK( toggle_mark_source, config::PLAYER_MARK_SOURCEBANNED )
PLAYER_MARK_CALLBACK( toggle_mark_pedophilia, config::PLAYER_MARK_PEDOPHILIA )
PLAYER_MARK_CALLBACK( toggle_mark_ulist, config::PLAYER_MARK_ULISTUSER )
PLAYER_MARK_CALLBACK( toggle_mark_zoophile, config::PLAYER_MARK_ZOOPHILE_ZOOJOKES )
#undef PLAYER_MARK_CALLBACK

static void mark_selected_cheater_and_party( )
{
    const player_list_row_t *player{ selected_player( ) };
    if ( !player )
    {
        return;
    }

    const u32 marks{ g_cfg.player_marks( player->m_account ) };
    const bool saved{ g_cfg.set_player_marks(
        player->m_account, marks | config::PLAYER_MARK_CHEATER ) };
    const usize associates{ saved ? g_cl.mark_party_suspected( player->m_account ) : 0 };

    char message[ 96 ]{ };
    if ( saved )
    {
        if ( associates == 1 )
        {
            str::copy( message, sizeof( message ),
                       ASTRAL_XORSTR_STATIC( "Cheater marked; 1 party member marked suspected" ) );
        } else
        {
            str::format_to( message, sizeof( message ),
                            ASTRAL_XORSTR( "Cheater marked; {} party members marked suspected" ).get( ),
                            associates );
        }
    }

    push_notification( saved ? notify_kind_t::success : notify_kind_t::warning,
                       ICON_BAN, ASTRAL_XORSTR_STATIC( "Player mark" ),
                       saved ? message : ASTRAL_XORSTR_STATIC( "Could not update player marks" ) );
    refresh_player_list( );
    g_gui.invalidate_layout( );
}

static bool has_selected_group( )
{
    return s_selected_group < config::PLAYER_GROUP_COUNT;
}

static usize saved_group_members( usize index )
{
    usize count{ };
    for ( const config::player_tag_t &entry : g_cfg.m_player_tags )
    {
        count += entry.m_account && ( entry.m_groups & ( 1u << index ) );
    }
    return count;
}

static void refresh_player_list( )
{
    if ( !g_cl.m_is_running || !g_gui.open( ) )
    {
        return;
    }

    usize count{ };
    bool  changed{ };
    if ( g_tf2.m_engine->IsInGame( ) )
    {
        const i32 local_index{ g_tf2.m_engine->GetLocalPlayer( ) };
        for ( i32 index{ 1 }; index <= g_tf2.m_engine->GetMaxClients( ) && count < 64;
              ++index )
        {
            if ( index == local_index )
            {
                continue;
            }

            player_info_t info{ };
            if ( !g_tf2.m_engine->GetPlayerInfo( index, &info ) || !info.friendsID ||
                 info.fakeplayer || info.ishltv || info.isreplay )
            {
                continue;
            }

            player_list_row_t &row{ s_player_rows[ count++ ] };
            const u32          old_account{ row.m_account };
            const u8           old_tag{ row.m_tag };
            const u32          old_marks{ row.m_marks };
            const u16          old_groups{ row.m_groups };
            const bool         old_astral{ row.m_astral };
            const u8           old_party{ row.m_party };
            row.m_account = info.friendsID;
            row.m_class   = g_player_data.player_class( index );
            row.m_team    = g_player_data.team( index );
            row.m_ping    = g_cl.m_resource ? g_cl.m_resource->m_iPing( )[ index ] : 0;
            row.m_hidden  = g_streamer_mode.should_hide_player( index );
            row.m_astral   = g_cl.is_astral_user( info.friendsID );
            row.m_party   = g_cl.party_index( info.friendsID );
            row.m_tag     = g_cfg.player_tag( info.friendsID );
            row.m_groups  = g_cfg.player_groups( info.friendsID );
            char      cloud_alias[ 129 ]{ };
            const u32 cloud_marks{ g_player_cloud.marks( info.friendsID, cloud_alias ) };
            row.m_marks = g_cfg.player_marks( info.friendsID ) | cloud_marks;
            row.m_cloud = cloud_marks != 0;
            if ( !row.m_tag && g_steam.is_friend( info.friendsID ) )
            {
                row.m_tag = config::PLAYER_TAG_FRIEND;
            }
            changed |= row.m_account != old_account || row.m_tag != old_tag || row.m_marks != old_marks ||
                       row.m_groups != old_groups || row.m_astral != old_astral ||
                       row.m_party != old_party;

            if ( row.m_party > 1 )
            {
                str::format_to( row.m_party_label, sizeof( row.m_party_label ),
                                ASTRAL_XORSTR( "Party {}" ).get( ), row.m_party - 1 );
            } else if ( row.m_party )
            {
                str::copy( row.m_party_label, sizeof( row.m_party_label ),
                           ASTRAL_XORSTR_STATIC( "Party" ) );
            }

            str::copy( row.m_real_name, sizeof( row.m_real_name ), info.name );
            str::copy( row.m_name, sizeof( row.m_name ),
                       row.m_hidden ? g_streamer_mode.get_ideal_player_name( index )
                                    : info.name );

            const char *alias{ row.m_hidden ? nullptr : g_cfg.player_alias( info.friendsID ) };
            row.m_aliased = alias != nullptr;
            if ( alias )
            {
                str::copy( row.m_name, sizeof( row.m_name ), alias );
            }
        }
    }

    if ( changed || count != s_player_count )
    {
        g_gui.invalidate_layout( );
    }
    s_player_count = count;

    if ( !selected_player( ) )
    {
        s_selected_account = count ? s_player_rows[ 0 ].m_account : 0;
    }
}

class player_list_widget : public gui::baseobject
{
public:
    player_list_widget( ) : gui::baseobject{ ASTRAL_XORSTR_STATIC( "Players" ) }
    {
    }

    i32 measure( ) const override
    {
        u8          filters[ PLAYER_FILTER_COUNT ]{ };
        rect_t      chips[ PLAYER_FILTER_COUNT ]{ };
        const usize chip_count{ this->layout_filters( filters, chips ) };
        const i32   rows_top{ chips[ chip_count - 1 ].bottom + g_gui.scaled( 18 ) };
        return math::bigger( g_gui.scaled( 248 ),
                             rows_top - this->m_bounds.top +
                                 static_cast< i32 >( this->filter_matches( s_player_filter ) ) * g_gui.scaled( 35 ) );
    }

    void think( ) override
    {
        const u32 delta{ gui::animation_delta( this->m_last_tick ) };

        if ( s_player_filter >= PLAYER_GROUP_FILTER_BASE &&
             !player_group_defined( s_player_filter - PLAYER_GROUP_FILTER_BASE ) )
        {
            s_player_filter = config::PLAYER_TAG_NONE;
        }

        u8          filters[ PLAYER_FILTER_COUNT ]{ };
        rect_t      chips[ PLAYER_FILTER_COUNT ]{ };
        const usize chip_count{ this->layout_filters( filters, chips ) };
        for ( usize chip{ }; chip < chip_count; ++chip )
        {
            const u8   filter{ filters[ chip ] };
            const bool hovered{ g_gui.hit_test( chips[ chip ] ) };
            if ( hovered )
            {
                set_cursor( );
                if ( g_input.pressed( VK_LBUTTON ) )
                {
                    s_player_filter = filter;
                }
            }

            gui::step_mix( this->m_filter_hover[ filter ], hovered, delta, WIDGET_HOVER_DURATION );
            gui::step_mix( this->m_filter_active[ filter ], s_player_filter == filter, delta,
                           WIDGET_TOGGLE_DURATION );
        }

        const i32 rows_top{ chips[ chip_count - 1 ].bottom + g_gui.scaled( 18 ) };
        this->m_hovered_row = -1;
        u32   first_account{ };
        bool  selected_visible{ };
        usize visible{ };
        for ( usize index{ }; index < s_player_count; ++index )
        {
            const player_list_row_t &row{ s_player_rows[ index ] };
            if ( !matches_filter( row, s_player_filter ) )
            {
                continue;
            }

            if ( !first_account )
            {
                first_account = row.m_account;
            }
            selected_visible |= row.m_account == s_selected_account;

            if ( g_gui.hit_test( this->row_bounds( rows_top, visible++ ) ) )
            {
                set_cursor( );
                this->m_hovered_row = static_cast< i32 >( index );
                if ( g_input.pressed( VK_LBUTTON ) )
                {
                    s_selected_account = row.m_account;
                    selected_visible   = true;
                }
                if ( g_input.pressed( VK_RBUTTON ) )
                {
                    g_steam.open_profile( row.m_account );
                }
            }
        }

        if ( !selected_visible )
        {
            s_selected_account = first_account;
        }

        for ( usize index{ }; index < s_player_count; ++index )
        {
            const player_list_row_t &row{ s_player_rows[ index ] };
            if ( !matches_filter( row, s_player_filter ) )
            {
                continue;
            }

            row_animation_t &animation{ this->m_rows[ index ] };
            if ( animation.m_account != row.m_account )
            {
                animation = row_animation_t{ row.m_account };
            }

            gui::step_mix( animation.m_reveal, true, delta, WIDGET_REVEAL_DURATION );
            gui::step_mix( animation.m_hover, static_cast< i32 >( index ) == this->m_hovered_row,
                           delta, WIDGET_HOVER_DURATION );
            gui::step_mix( animation.m_selected, row.m_account == s_selected_account,
                           delta, WIDGET_TOGGLE_DURATION );

            const i32 bars{ row.m_ping < 80 ? 3 : row.m_ping < 150 ? 2
                                                                   : 1 };
            for ( i32 bar{ }; bar < 3; ++bar )
            {
                gui::step_mix( animation.m_ping[ bar ], row.m_ping > 0 && bar < bars,
                               delta, WIDGET_TOGGLE_DURATION );
            }
        }

        if ( this->measure( ) != this->m_bounds.bottom - this->m_bounds.top )
        {
            g_gui.invalidate_layout( );
        }
    }

    void draw( ) const override
    {
        u8          filters[ PLAYER_FILTER_COUNT ]{ };
        rect_t      chips[ PLAYER_FILTER_COUNT ]{ };
        const usize chip_count{ this->layout_filters( filters, chips ) };
        for ( usize chip{ }; chip < chip_count; ++chip )
        {
            this->draw_filter( chips[ chip ], filters[ chip ] );
        }

        const i32   rows_top{ chips[ chip_count - 1 ].bottom + g_gui.scaled( 18 ) };
        const usize shown_count{ this->filter_matches( s_player_filter ) };
        char        count[ 24 ]{ };
        str::format_to( count, sizeof( count ), ASTRAL_XORSTR( "{} connected" ).get( ), shown_count );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ this->m_bounds.left + g_gui.scaled( 3 ),
                                rows_top - g_gui.scaled( 14 ) },
                       g_gui.color( MENU_TEXT_DIM ), count );

        if ( !shown_count )
        {
            g_render.text( MENU_FONT( ),
                           point_t{ ( this->m_bounds.left + this->m_bounds.right ) / 2,
                                    rows_top + g_gui.scaled( 40 ) },
                           g_gui.color( MENU_TEXT_DIM ),
                           ASTRAL_XORSTR_STATIC( "No players in this filter" ), false,
                           render::text_align::center );
        }

        const i32 more_width{ player_chip_width( player_chip_t{ ASTRAL_XORSTR_STATIC( "+99" ) } ) };
        usize     visible{ };
        for ( usize index{ }; index < s_player_count; ++index )
        {
            const player_list_row_t &row{ s_player_rows[ index ] };
            if ( !matches_filter( row, s_player_filter ) )
            {
                continue;
            }

            const rect_t           bounds{ this->row_bounds( rows_top, visible++ ) };
            const row_animation_t &animation{ this->m_rows[ index ] };
            const u8               hover{ gui::eased_mix( animation.m_hover ) };
            const u8               selected{ gui::eased_mix( animation.m_selected ) };
            const u8               parent_fade{ g_gui.fade( ) };
            g_gui.set_fade( static_cast< u8 >(
                ( static_cast< u32 >( parent_fade ) * gui::eased_out( animation.m_reveal ) +
                  127u ) /
                255u ) );

            color_t background{ gui::mix_color( CONTROL_BACKGROUND,
                                                CONTROL_BACKGROUND_HOVER, hover ) };
            background = gui::mix_color( background, g_theme.m_row_selected_background,
                                         selected );
            g_render.box( bounds, g_gui.color( background ), true );
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1,
                                  bounds.bottom - 1 },
                          g_gui.color( gui::mix_color( CONTROL_BORDER, g_theme.m_accent,
                                                       selected ) ) );

            const i32    avatar_size{ g_gui.scaled( 25 ) };
            const rect_t avatar{ bounds.left + g_gui.scaled( 4 ), bounds.top + g_gui.scaled( 4 ),
                                 bounds.left + g_gui.scaled( 4 ) + avatar_size,
                                 bounds.top + g_gui.scaled( 4 ) + avatar_size };
            g_render.box( avatar, g_gui.color( CONTROL_BACKGROUND_HOVER ), true );
            if ( !row.m_hidden )
            {
                g_render.text( MENU_FONT( ), point_t{ avatar.left, avatar.top },
                               g_gui.color( MENU_TEXT ), ASTRAL_XORSTR_STATIC( " " ), false,
                               render::text_align::left, 0.0f, row.m_account,
                               render::AVATAR_SQUARE, static_cast< u32 >( avatar_size ) );
            } else
            {
                gui::draw_icon_at( ICON_USER, point_t{ avatar.left + g_gui.scaled( 6 ), avatar.top + g_gui.scaled( 6 ) },
                                   g_gui.color( MENU_TEXT_DIM ) );
            }
            g_render.box( avatar, g_gui.color( CONTROL_BORDER ) );

            const i32 text_left{ avatar.right + g_gui.scaled( 5 ) };
            const i32 chip_gap{ g_gui.scaled( 5 ) };

            player_chip_t chips[ PLAYER_CHIP_COUNT ]{ };
            const usize   chip_count{ player_chips( row, chips ) };
            const i32     chip_room{ bounds.right - g_gui.scaled( 4 ) - text_left - g_gui.scaled( 48 ) };
            i32           chips_width{ };
            usize         shown{ };
            for ( ; shown < chip_count; ++shown )
            {
                const i32 width{ chip_gap + player_chip_width( chips[ shown ] ) };
                const i32 reserve{ shown + 1 < chip_count ? chip_gap + more_width : 0 };
                if ( chips_width + width + reserve > chip_room )
                {
                    break;
                }

                chips_width += width;
            }

            char more[ 8 ]{ };
            if ( shown < chip_count )
            {
                str::format_to( more, sizeof( more ), ASTRAL_XORSTR( "+{}" ).get( ), chip_count - shown );
                chips_width += chip_gap + player_chip_width( player_chip_t{ more } );
            }

            const i32 text_right{ bounds.right - g_gui.scaled( 4 ) - chips_width };
            char      name[ 64 ]{ };
            gui::fit_text( name, row.m_name, text_right - text_left );
            g_render.text( MENU_FONT( ), point_t{ text_left, bounds.top + g_gui.scaled( 5 ) },
                           g_gui.color( gui::mix_color( LIST_TEXT_IDLE, MENU_TEXT, selected ) ),
                           name );

            char detail[ 96 ]{ };
            if ( row.m_aliased )
            {
                char real_name[ 96 ]{ };
                str::format_to( real_name, sizeof( real_name ), ASTRAL_XORSTR( "{} | {}" ).get( ),
                                row.m_real_name, player_class_label( row.m_class ) );
                gui::fit_text( detail, real_name, bounds.right - g_gui.scaled( 43 ) - text_left );
            } else
            {
                str::copy( detail, sizeof( detail ), player_class_label( row.m_class ) );
            }
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ text_left, bounds.top + g_gui.scaled( 19 ) },
                           g_gui.color( MENU_TEXT_DIM ), detail );

            i32 chip_left{ text_left + gui::estimate_text_width( name ) };
            for ( usize chip{ }; chip <= shown && chip < chip_count; ++chip )
            {
                const player_chip_t label{ chip < shown ? chips[ chip ]
                                                        : player_chip_t{ more, MENU_TEXT_MUTED } };
                chip_left += chip_gap;

                const rect_t chip_bounds{ chip_left, bounds.top + g_gui.scaled( 5 ),
                                          chip_left + player_chip_width( label ),
                                          bounds.top + g_gui.scaled( 19 ) };
                draw_player_chip( chip_bounds, label );
                chip_left = chip_bounds.right;
            }

            if ( row.m_ping > 0 )
            {
                const color_t signal{ row.m_ping < 80    ? color_t{ 75, 218, 174 }
                                      : row.m_ping < 150 ? color_t{ 235, 190, 90 }
                                                         : color_t{ 255, 110, 100 } };
                const i32     signal_x{ bounds.right - g_gui.scaled( 39 ) };
                const i32     signal_bottom{ bounds.bottom - g_gui.scaled( 5 ) };
                for ( i32 bar{ }; bar < 3; ++bar )
                {
                    const i32 left{ signal_x + g_gui.scaled( bar * 4 ) };
                    g_render.box( rect_t{ left,
                                          signal_bottom - g_gui.scaled( 3 + bar * 2 ),
                                          left + g_gui.scaled( 2 ), signal_bottom },
                                  g_gui.color( gui::mix_color( MENU_TEXT_DIM, signal,
                                                               gui::eased_mix( animation.m_ping[ bar ] ) ) ),
                                  true );
                }

                char ping[ 12 ]{ };
                str::format_to( ping, sizeof( ping ), ASTRAL_XORSTR( "{}" ).get( ),
                                row.m_ping );
                g_render.text( MENU_FONT_TINY( ),
                               point_t{ bounds.right - g_gui.scaled( 4 ),
                                        bounds.top + g_gui.scaled( 19 ) },
                               g_gui.color( MENU_TEXT_DIM ), ping, false,
                               render::text_align::right );
            }

            g_gui.set_fade( parent_fade );
        }
    }

private:
    struct row_animation_t
    {
        u32 m_account{ };
        u8  m_reveal{ };
        u8  m_hover{ };
        u8  m_selected{ };
        u8  m_ping[ 3 ]{ };
    };

    usize filter_matches( u8 filter ) const
    {
        usize count{ };
        for ( usize index{ }; index < s_player_count; ++index )
        {
            count += matches_filter( s_player_rows[ index ], filter );
        }
        return count;
    }

    i32 filter_width( u8 filter ) const
    {
        char count[ 8 ]{ };
        str::format_to( count, sizeof( count ), ASTRAL_XORSTR( "{}" ).get( ),
                        this->filter_matches( filter ) );
        const i32 dot{ filter >= PLAYER_MARK_FILTER_BASE ? g_gui.scaled( 8 ) : 0 };
        return dot + gui::estimate_tiny_text_width( player_filter_label( filter ) ) +
               gui::estimate_tiny_text_width( count ) + g_gui.scaled( 11 );
    }

    usize layout_filters( u8 ( &filters )[ PLAYER_FILTER_COUNT ],
                          rect_t ( &bounds )[ PLAYER_FILTER_COUNT ] ) const
    {
        chip_flow_t flow{ point_t{ this->m_bounds.left, this->m_bounds.top }, card_inner_width( ) };
        usize       count{ };
        for ( u8 filter{ }; filter < PLAYER_FILTER_COUNT; ++filter )
        {
            if ( filter >= PLAYER_GROUP_FILTER_BASE && !player_group_defined( filter - PLAYER_GROUP_FILTER_BASE ) )
            {
                continue;
            }

            filters[ count ] = filter;
            bounds[ count ]  = flow.place( this->filter_width( filter ) );
            ++count;
        }
        return count;
    }

    void draw_filter( const rect_t &bounds, u8 filter ) const
    {
        const u8 active{ gui::eased_mix( this->m_filter_active[ filter ] ) };
        const u8 hover{ gui::eased_mix( this->m_filter_hover[ filter ] ) };
        color_t  background{ gui::mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND, hover ) };
        background = gui::mix_color( background, g_theme.m_row_selected_background, active );
        g_render.box( bounds, g_gui.color( background ), true );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      g_gui.color( gui::mix_color( CONTROL_BORDER, g_theme.m_accent, active ) ) );

        i32 left{ bounds.left + g_gui.scaled( 4 ) };
        if ( filter >= PLAYER_MARK_FILTER_BASE )
        {
            const i32     dot{ g_gui.scaled( 5 ) };
            const i32     top{ ( bounds.top + bounds.bottom - dot ) / 2 };
            const color_t tint{ filter < PLAYER_GROUP_FILTER_BASE
                                    ? player_mark_color( filter - PLAYER_MARK_FILTER_BASE )
                                    : g_cfg.m_player_groups[ filter - PLAYER_GROUP_FILTER_BASE ].m_color };
            g_render.box( rect_t{ left, top, left + dot, top + dot }, g_gui.color( tint ), true );
            left += g_gui.scaled( 8 );
        }

        const char *label{ player_filter_label( filter ) };
        const i32   text_y{ g_gui.tiny_center_y( bounds.top, bounds.bottom - bounds.top ) };
        g_render.text( MENU_FONT_TINY( ), point_t{ left, text_y },
                       g_gui.color( gui::mix_color( gui::mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover ),
                                                    LIST_TEXT_ACTIVE, active ) ),
                       label );

        char count[ 8 ]{ };
        str::format_to( count, sizeof( count ), ASTRAL_XORSTR( "{}" ).get( ),
                        this->filter_matches( filter ) );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ left + gui::estimate_tiny_text_width( label ) + g_gui.scaled( 3 ), text_y },
                       g_gui.color( MENU_TEXT_DIM ), count );
    }

    rect_t row_bounds( i32 rows_top, usize visible ) const
    {
        const i32 top{ rows_top + g_gui.scaled( static_cast< i32 >( visible ) * 35 ) };
        return rect_t{ this->m_bounds.left, top, this->m_bounds.right,
                       top + g_gui.scaled( 33 ) };
    }

    i32             m_hovered_row{ -1 };
    row_animation_t m_rows[ 64 ]{ };
    u8              m_filter_hover[ PLAYER_FILTER_COUNT ]{ };
    u8              m_filter_active[ PLAYER_FILTER_COUNT ]{ };
    u64             m_last_tick{ };
};

static void set_player_status( const player_list_row_t &player, u8 tag )
{
    if ( !g_cfg.set_player_tag( player.m_account, tag ) )
    {
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Playerlist" ),
                           ASTRAL_XORSTR_STATIC( "Tag list is full" ) );
    }
}

class player_tag_widget : public gui::baseobject
{
public:
    player_tag_widget( ) : gui::baseobject{ ASTRAL_XORSTR_STATIC( "Targeting" ) }
    {
    }

    i32 measure( ) const override
    {
        return this->tags_bottom( ) - this->m_bounds.top + g_gui.scaled( 20 );
    }

    void think( ) override
    {
        const player_list_row_t *player{ selected_player( ) };
        const u32                delta{ gui::animation_delta( this->m_last_tick ) };
        const u32                account{ player ? player->m_account : 0 };
        if ( account != this->m_shown_account )
        {
            this->m_shown_account = account;
            this->m_reveal        = 0;
            const char *alias{ g_cfg.player_alias( account ) };
            s_alias_input->set_value( alias ? string_view{ alias } : string_view{ } );
            s_report_reason_input->set_value( { } );
        }
        gui::step_mix( this->m_reveal, player != nullptr, delta, WIDGET_REVEAL_DURATION );

        if ( this->measure( ) != this->m_bounds.bottom - this->m_bounds.top )
        {
            g_gui.invalidate_layout( );
        }

        for ( u8 tag{ }; tag < PLAYER_STATUS_COUNT; ++tag )
        {
            const bool hovered{ player && g_gui.hit_test( this->status_bounds( tag ) ) };
            if ( hovered )
            {
                set_cursor( );
            }

            gui::step_mix( this->m_status_hover[ tag ], hovered, delta, WIDGET_HOVER_DURATION );
            gui::step_mix( this->m_status_active[ tag ], player && player->m_tag == tag, delta,
                           WIDGET_TOGGLE_DURATION );
            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                set_player_status( *player, tag );
                refresh_player_list( );
                g_gui.invalidate_layout( );
                return;
            }
        }

        rect_t mark_chips[ PLAYER_MARK_COUNT ]{ };
        this->layout_marks( mark_chips );
        for ( usize mark{ }; mark < PLAYER_MARK_COUNT; ++mark )
        {
            const bool hovered{ player && g_gui.hit_test( mark_chips[ mark ] ) };
            if ( hovered )
            {
                set_cursor( );
            }
            gui::step_mix( this->m_mark_hover[ mark ], hovered, delta, WIDGET_HOVER_DURATION );
            gui::step_mix( this->m_mark_active[ mark ],
                           player && ( player->m_marks & ( 1u << mark ) ), delta,
                           WIDGET_TOGGLE_DURATION );
            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                toggle_selected_player_mark( 1u << mark );
                return;
            }
        }

        u8          groups[ config::PLAYER_GROUP_COUNT ]{ };
        rect_t      chips[ config::PLAYER_GROUP_COUNT ]{ };
        const usize chip_count{ this->layout_tags( groups, chips ) };
        for ( usize chip{ }; chip < chip_count; ++chip )
        {
            const u8   group{ groups[ chip ] };
            const u16  bit{ static_cast< u16 >( 1u << group ) };
            const bool hovered{ player && g_gui.hit_test( chips[ chip ] ) };
            if ( hovered )
            {
                set_cursor( );
            }

            gui::step_mix( this->m_tag_hover[ group ], hovered, delta, WIDGET_HOVER_DURATION );
            gui::step_mix( this->m_tag_active[ group ], player && ( player->m_groups & bit ), delta,
                           WIDGET_TOGGLE_DURATION );
            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                if ( !g_cfg.set_player_groups( player->m_account, static_cast< u16 >( player->m_groups ^ bit ) ) )
                {
                    push_notification( notify_kind_t::warning, ICON_PLAYERS,
                                       ASTRAL_XORSTR_STATIC( "Playerlist" ),
                                       ASTRAL_XORSTR_STATIC( "Tag list is full" ) );
                }
                refresh_player_list( );
                g_gui.invalidate_layout( );
                return;
            }
        }
    }

    void draw( ) const override
    {
        const player_list_row_t *player{ selected_player( ) };
        if ( !player )
        {
            g_render.text( MENU_FONT( ),
                           point_t{ this->m_bounds.left + g_gui.scaled( 3 ),
                                    this->m_bounds.top + g_gui.scaled( 8 ) },
                           g_gui.color( MENU_TEXT_DIM ),
                           s_player_count ? ASTRAL_XORSTR_STATIC( "No players in this filter" )
                                          : ASTRAL_XORSTR_STATIC( "Join a server to see players" ) );
            return;
        }

        const u8 parent_fade{ g_gui.fade( ) };
        g_gui.set_fade( static_cast< u8 >(
            ( static_cast< u32 >( parent_fade ) * gui::eased_out( this->m_reveal ) +
              127u ) /
            255u ) );

        const i32    avatar_size{ g_gui.scaled( 37 ) };
        const rect_t avatar{ this->m_bounds.left + g_gui.scaled( 3 ),
                             this->m_bounds.top + g_gui.scaled( 3 ),
                             this->m_bounds.left + g_gui.scaled( 3 ) + avatar_size,
                             this->m_bounds.top + g_gui.scaled( 3 ) + avatar_size };
        g_render.box( avatar, g_gui.color( CONTROL_BACKGROUND_HOVER ), true );
        if ( !player->m_hidden )
        {
            g_render.text( MENU_FONT( ), point_t{ avatar.left, avatar.top },
                           g_gui.color( MENU_TEXT ), ASTRAL_XORSTR_STATIC( " " ), false,
                           render::text_align::left, 0.0f, player->m_account,
                           render::AVATAR_SQUARE, static_cast< u32 >( avatar_size ) );
        } else
        {
            gui::draw_icon_at( ICON_USER, point_t{ avatar.left + g_gui.scaled( 12 ), avatar.top + g_gui.scaled( 12 ) },
                               g_gui.color( MENU_TEXT_DIM ) );
        }
        g_render.box( avatar, g_gui.color( CONTROL_BORDER ) );

        const i32 text_left{ avatar.right + g_gui.scaled( 7 ) };
        char      name[ 64 ]{ };
        gui::fit_text( name, player->m_aliased ? player->m_real_name : player->m_name,
                       this->m_bounds.right - text_left - g_gui.scaled( 3 ) );
        g_render.text( MENU_FONT_BOLD( ),
                       point_t{ text_left, this->m_bounds.top + g_gui.scaled( 7 ) },
                       g_gui.color( MENU_TEXT ), name );
        char account[ 48 ]{ };
        str::format_to( account, sizeof( account ),
                        player->m_astral ? ASTRAL_XORSTR( "ID {} | Astral user" ).get( )
                                        : ASTRAL_XORSTR( "ID {}" ).get( ),
                        player->m_account );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ text_left, this->m_bounds.top + g_gui.scaled( 23 ) },
                       g_gui.color( MENU_TEXT_DIM ), account );

        const i32 left{ this->m_bounds.left + g_gui.scaled( 2 ) };
        const i32 right{ this->m_bounds.right - g_gui.scaled( 2 ) };
        g_render.line( point_t{ left, this->m_bounds.top + g_gui.scaled( 46 ) },
                       point_t{ right, this->m_bounds.top + g_gui.scaled( 46 ) },
                       g_gui.color( CARD_RULE ) );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ left, this->m_bounds.top + g_gui.scaled( 54 ) },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "CLASS" ) );
        g_render.text( MENU_FONT( ),
                       point_t{ left, this->m_bounds.top + g_gui.scaled( 67 ) },
                       g_gui.color( MENU_TEXT ), player_class_label( player->m_class ) );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ this->m_bounds.left + g_gui.scaled( 91 ),
                                this->m_bounds.top + g_gui.scaled( 54 ) },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "TEAM" ) );
        const char *team{ player->m_team == TF_TEAM_RED    ? ASTRAL_XORSTR_STATIC( "RED" )
                          : player->m_team == TF_TEAM_BLUE ? ASTRAL_XORSTR_STATIC( "BLU" )
                                                           : ASTRAL_XORSTR_STATIC( "--" ) };
        g_render.text( MENU_FONT( ),
                       point_t{ this->m_bounds.left + g_gui.scaled( 91 ),
                                this->m_bounds.top + g_gui.scaled( 67 ) },
                       g_gui.color( player->m_team == TF_TEAM_RED    ? color_t{ 255, 110, 110 }
                                    : player->m_team == TF_TEAM_BLUE ? color_t{ 110, 170, 255 }
                                                                     : MENU_TEXT ),
                       team );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ this->m_bounds.left + g_gui.scaled( 150 ),
                                this->m_bounds.top + g_gui.scaled( 54 ) },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "PARTY" ) );
        g_render.text( MENU_FONT( ),
                       point_t{ this->m_bounds.left + g_gui.scaled( 150 ),
                                this->m_bounds.top + g_gui.scaled( 67 ) },
                       g_gui.color( player->m_party ? client::party_color( player->m_party ) : MENU_TEXT ),
                       player->m_party == 1 ? ASTRAL_XORSTR_STATIC( "Member" )
                       : player->m_party    ? player->m_party_label
                                            : ASTRAL_XORSTR_STATIC( "--" ) );
        g_render.line( point_t{ left, this->m_bounds.top + g_gui.scaled( 84 ) },
                       point_t{ right, this->m_bounds.top + g_gui.scaled( 84 ) },
                       g_gui.color( CARD_RULE ) );

        this->draw_status( *player, left, right );
        this->draw_marks( *player, left, right );
        this->draw_tags( *player, left, right );

        const i32 alias_top{ this->tags_bottom( ) + g_gui.scaled( 8 ) };
        char      cloud_alias[ 129 ]{ };
        g_render.text( MENU_FONT_TINY( ), point_t{ left, alias_top },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "ALIAS" ) );
        if ( g_player_cloud.known( player->m_account, cloud_alias ) && cloud_alias[ 0 ] )
        {
            char text[ 160 ]{ };
            char note[ 160 ]{ };
            str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "Cloud: {}" ).get( ), cloud_alias );
            gui::fit_text( note, text, ( right - left ) / 2 );
            g_render.text( MENU_FONT_TINY( ), point_t{ right, alias_top },
                           g_gui.color( MENU_TEXT_DIM ), note, false,
                           render::text_align::right );
        }

        g_gui.set_fade( parent_fade );
    }

private:
    rect_t status_bounds( u8 tag ) const
    {
        const i32 left{ this->m_bounds.left + g_gui.scaled( 2 ) };
        const i32 span{ this->m_bounds.right - g_gui.scaled( 2 ) - left };
        const i32 top{ this->m_bounds.top + g_gui.scaled( 104 ) };
        return rect_t{ left + tag * span / PLAYER_STATUS_COUNT, top,
                       left + ( tag + 1 ) * span / PLAYER_STATUS_COUNT,
                       top + g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) };
    }

    static i32 tag_chip_width( usize group )
    {
        return gui::estimate_tiny_text_width( player_group_name( group ) ) + g_gui.scaled( 16 );
    }

    void layout_marks( rect_t ( &bounds )[ PLAYER_MARK_COUNT ] ) const
    {
        const i32 left{ this->m_bounds.left + g_gui.scaled( 2 ) };
        const i32 right{ this->m_bounds.right - g_gui.scaled( 2 ) };
        const i32 top{ this->m_bounds.top + g_gui.scaled( 140 ) };
        const i32 gap{ g_gui.scaled( 3 ) };
        const i32 row_height{ g_gui.scaled( WIDGET_CHIP_HEIGHT ) };
        const i32 column_width{ ( right - left - gap ) / 2 };
        for ( usize mark{ }; mark < PLAYER_MARK_COUNT; ++mark )
        {
            const i32 column{ static_cast< i32 >( mark & 1 ) };
            const i32 row{ static_cast< i32 >( mark / 2 ) };
            const i32 x{ left + column * ( column_width + gap ) };
            const i32 y{ top + row * ( row_height + gap ) };
            bounds[ mark ] = rect_t{ x, y, x + column_width, y + row_height };
        }
    }

    i32 marks_bottom( ) const
    {
        rect_t chips[ PLAYER_MARK_COUNT ]{ };
        this->layout_marks( chips );
        return chips[ PLAYER_MARK_COUNT - 1 ].bottom;
    }

    usize layout_tags( u8 ( &groups )[ config::PLAYER_GROUP_COUNT ],
                       rect_t ( &bounds )[ config::PLAYER_GROUP_COUNT ] ) const
    {
        chip_flow_t flow{ point_t{ this->m_bounds.left + g_gui.scaled( 2 ),
                                   this->marks_bottom( ) + g_gui.scaled( 25 ) },
                          card_inner_width( ) - g_gui.scaled( 4 ) };
        usize       count{ };
        for ( u8 group{ }; group < config::PLAYER_GROUP_COUNT; ++group )
        {
            if ( !player_group_defined( group ) )
            {
                continue;
            }

            groups[ count ] = group;
            bounds[ count ] = flow.place( tag_chip_width( group ) );
            ++count;
        }
        return count;
    }

    i32 tags_bottom( ) const
    {
        u8          groups[ config::PLAYER_GROUP_COUNT ]{ };
        rect_t      chips[ config::PLAYER_GROUP_COUNT ]{ };
        const usize chip_count{ this->layout_tags( groups, chips ) };
        return chip_count ? chips[ chip_count - 1 ].bottom
                          : this->marks_bottom( ) + g_gui.scaled( 39 );
    }

    void draw_status( const player_list_row_t &player, i32 left, i32 right ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( 92 ) };
        g_render.text( MENU_FONT_TINY( ), point_t{ left, top },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "TARGETING" ) );
        if ( player.m_cloud )
        {
            g_render.text( MENU_FONT_TINY( ), point_t{ right, top },
                           g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "From Cloud" ), false,
                           render::text_align::right );
        }

        for ( u8 tag{ }; tag < PLAYER_STATUS_COUNT; ++tag )
        {
            const rect_t  bounds{ this->status_bounds( tag ) };
            const color_t tint{ tag ? player_tag_color( tag ) : CONTROL_VALUE };
            const u8      active{ gui::eased_mix( this->m_status_active[ tag ] ) };
            const u8      hover{ gui::eased_mix( this->m_status_hover[ tag ] ) };
            color_t       background{ gui::mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND, hover ) };
            background = gui::mix_color( background, gui::with_alpha( tint, 30 ), active );
            g_render.box( bounds, g_gui.color( background ), true );
            g_render.box( rect_t{ bounds.left, bounds.top,
                                  tag + 1 == PLAYER_STATUS_COUNT ? bounds.right - 1 : bounds.right,
                                  bounds.bottom - 1 },
                          g_gui.color( CONTROL_BORDER ) );

            g_render.text( MENU_FONT_TINY( ),
                           point_t{ ( bounds.left + bounds.right ) / 2,
                                    g_gui.tiny_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( gui::mix_color( gui::mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover ),
                                                        tint, active ) ),
                           player_tag_label( tag ), false, render::text_align::center );
        }

        for ( u8 tag{ }; tag < PLAYER_STATUS_COUNT; ++tag )
        {
            const u8 active{ gui::eased_mix( this->m_status_active[ tag ] ) };
            if ( !active )
            {
                continue;
            }

            const rect_t bounds{ this->status_bounds( tag ) };
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                          g_gui.color( gui::with_alpha( tag ? player_tag_color( tag ) : CONTROL_VALUE, active ) ) );
        }
    }

    void draw_tags( const player_list_row_t &player, i32 left, i32 right ) const
    {
        u8          groups[ config::PLAYER_GROUP_COUNT ]{ };
        rect_t      chips[ config::PLAYER_GROUP_COUNT ]{ };
        const usize chip_count{ this->layout_tags( groups, chips ) };
        const i32   top{ this->marks_bottom( ) + g_gui.scaled( 12 ) };
        g_render.text( MENU_FONT_TINY( ), point_t{ left, top },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "TAGS" ) );
        if ( !chip_count )
        {
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ left, this->marks_bottom( ) + g_gui.scaled( 25 ) },
                           g_gui.color( MENU_TEXT_FAINT ),
                           ASTRAL_XORSTR_STATIC( "Create tags in the Tags subtab" ) );
            return;
        }

        usize assigned{ };
        for ( usize chip{ }; chip < chip_count; ++chip )
        {
            assigned += ( player.m_groups & ( 1u << groups[ chip ] ) ) != 0;
        }

        char count[ 24 ]{ };
        str::format_to( count, sizeof( count ), ASTRAL_XORSTR( "{} of {}" ).get( ), assigned, chip_count );
        g_render.text( MENU_FONT_TINY( ), point_t{ right, top }, g_gui.color( MENU_TEXT_DIM ),
                       count, false, render::text_align::right );

        for ( usize chip{ }; chip < chip_count; ++chip )
        {
            const config::player_group_t &group{ g_cfg.m_player_groups[ groups[ chip ] ] };
            const rect_t                  bounds{ chips[ chip ] };
            const u8                      active{ gui::eased_mix( this->m_tag_active[ groups[ chip ] ] ) };
            const u8                      hover{ gui::eased_mix( this->m_tag_hover[ groups[ chip ] ] ) };
            color_t                       background{ gui::mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND, hover ) };
            background = gui::mix_color( background, gui::with_alpha( group.m_color, 24 ), active );
            g_render.box( bounds, g_gui.color( background ), true );
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                          g_gui.color( gui::mix_color( CONTROL_BORDER, gui::with_alpha( group.m_color, 150 ), active ) ) );

            const i32 dot{ g_gui.scaled( 5 ) };
            const i32 dot_left{ bounds.left + g_gui.scaled( 4 ) };
            const i32 dot_top{ ( bounds.top + bounds.bottom - dot ) / 2 };
            g_render.box( rect_t{ dot_left, dot_top, dot_left + dot, dot_top + dot },
                          g_gui.color( gui::with_alpha( group.m_color, static_cast< u8 >( 140 + 115 * active / 255 ) ) ),
                          true );
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ dot_left + g_gui.scaled( 8 ),
                                    g_gui.tiny_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( gui::mix_color( gui::mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover ),
                                                        group.m_color, active ) ),
                           reinterpret_cast< const char * >( group.m_name ) );
        }
    }

    void draw_marks( const player_list_row_t &player, i32 left, i32 right ) const
    {
        rect_t chips[ PLAYER_MARK_COUNT ]{ };
        this->layout_marks( chips );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ left, this->m_bounds.top + g_gui.scaled( 127 ) },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "MARKS" ) );

        usize assigned{ };
        for ( usize mark{ }; mark < PLAYER_MARK_COUNT; ++mark )
        {
            assigned += ( player.m_marks & ( 1u << mark ) ) != 0;
        }
        char count[ 24 ]{ };
        str::format_to( count, sizeof( count ), ASTRAL_XORSTR( "{} of {}" ).get( ),
                        assigned, PLAYER_MARK_COUNT );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ right, this->m_bounds.top + g_gui.scaled( 127 ) },
                       g_gui.color( MENU_TEXT_DIM ), count, false, render::text_align::right );

        for ( usize mark{ }; mark < PLAYER_MARK_COUNT; ++mark )
        {
            const rect_t  bounds{ chips[ mark ] };
            const color_t tint{ player_mark_color( mark ) };
            const u8      active{ gui::eased_mix( this->m_mark_active[ mark ] ) };
            const u8      hover{ gui::eased_mix( this->m_mark_hover[ mark ] ) };
            color_t       background{ gui::mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND, hover ) };
            background = gui::mix_color( background, gui::with_alpha( tint, 24 ), active );
            g_render.box( bounds, g_gui.color( background ), true );
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                          g_gui.color( gui::mix_color( CONTROL_BORDER,
                                                       gui::with_alpha( tint, 150 ), active ) ) );
            const i32 dot{ g_gui.scaled( 5 ) };
            const i32 dot_left{ bounds.left + g_gui.scaled( 4 ) };
            const i32 dot_top{ ( bounds.top + bounds.bottom - dot ) / 2 };
            g_render.box( rect_t{ dot_left, dot_top, dot_left + dot, dot_top + dot },
                          g_gui.color( gui::with_alpha( tint,
                                                        static_cast< u8 >( 140 + 115 * active / 255 ) ) ),
                          true );
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ dot_left + g_gui.scaled( 8 ),
                                    g_gui.tiny_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( gui::mix_color(
                               gui::mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover ), tint, active ) ),
                           player_mark_label( mark ) );
        }
    }

    u8  m_status_hover[ PLAYER_STATUS_COUNT ]{ };
    u8  m_status_active[ PLAYER_STATUS_COUNT ]{ };
    u8  m_mark_hover[ PLAYER_MARK_COUNT ]{ };
    u8  m_mark_active[ PLAYER_MARK_COUNT ]{ };
    u8  m_tag_hover[ config::PLAYER_GROUP_COUNT ]{ };
    u8  m_tag_active[ config::PLAYER_GROUP_COUNT ]{ };
    u8  m_reveal{ };
    u32 m_shown_account{ };
    u64 m_last_tick{ };
};

class player_group_list_widget : public gui::baseobject
{
public:
    player_group_list_widget( ) : gui::baseobject{ ASTRAL_XORSTR_STATIC( "Tags" ) }
    {
    }

    i32 measure( ) const override
    {
        return g_gui.scaled( 18 + math::bigger( 1, static_cast< i32 >( this->defined_count( ) ) ) * 23 );
    }

    void think( ) override
    {
        if ( !has_selected_group( ) || !player_group_defined( s_selected_group ) )
        {
            s_selected_group = config::PLAYER_GROUP_COUNT;
            for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
            {
                if ( player_group_defined( index ) )
                {
                    s_selected_group = index;
                    break;
                }
            }
        }

        if ( s_selected_group != s_shown_group )
        {
            s_shown_group = s_selected_group;
            for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
            {
                s_group_shown[ index ] = index == s_selected_group;
            }
            s_group_name_input->set_value( has_selected_group( ) ? string_view{ player_group_name( s_selected_group ) }
                                                                 : string_view{ } );
        }

        if ( this->measure( ) != this->m_bounds.bottom - this->m_bounds.top )
        {
            g_gui.invalidate_layout( );
        }

        const u32 delta{ gui::animation_delta( this->m_last_tick ) };
        usize     row{ };
        for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
        {
            if ( !player_group_defined( index ) )
            {
                continue;
            }

            const bool hovered{ g_gui.hit_test( this->row_bounds( row++ ) ) };
            if ( hovered )
            {
                set_cursor( );
                if ( g_input.pressed( VK_LBUTTON ) )
                {
                    s_selected_group = index;
                }
            }

            gui::step_mix( this->m_hover[ index ], hovered, delta, WIDGET_HOVER_DURATION );
            gui::step_mix( this->m_selected[ index ], index == s_selected_group, delta,
                           WIDGET_TOGGLE_DURATION );
        }
    }

    void draw( ) const override
    {
        char header[ 32 ]{ };
        str::format_to( header, sizeof( header ), ASTRAL_XORSTR( "{} of {} tags" ).get( ),
                        this->defined_count( ), config::PLAYER_GROUP_COUNT );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ this->m_bounds.left + g_gui.scaled( 3 ),
                                this->m_bounds.top + g_gui.scaled( 2 ) },
                       g_gui.color( MENU_TEXT_DIM ), header );

        if ( !this->defined_count( ) )
        {
            const rect_t bounds{ this->row_bounds( 0 ) };
            g_render.text( MENU_FONT( ),
                           point_t{ bounds.left + g_gui.scaled( 3 ),
                                    g_gui.text_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "No tags yet" ) );
            return;
        }

        usize row{ };
        for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
        {
            if ( !player_group_defined( index ) )
            {
                continue;
            }

            const config::player_group_t &group{ g_cfg.m_player_groups[ index ] };
            const rect_t                  bounds{ this->row_bounds( row++ ) };
            const u8                      hover{ gui::eased_mix( this->m_hover[ index ] ) };
            const u8                      selected{ gui::eased_mix( this->m_selected[ index ] ) };
            color_t                       background{ gui::mix_color( CONTROL_BACKGROUND, CONTROL_BACKGROUND_HOVER, hover ) };
            background = gui::mix_color( background, g_theme.m_row_selected_background, selected );
            g_render.box( bounds, g_gui.color( background ), true );
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                          g_gui.color( gui::mix_color( CONTROL_BORDER, g_theme.m_accent, selected ) ) );

            const i32    swatch_size{ g_gui.scaled( 9 ) };
            const i32    swatch_top{ ( bounds.top + bounds.bottom - swatch_size ) / 2 };
            const rect_t swatch{ bounds.left + g_gui.scaled( 6 ), swatch_top,
                                 bounds.left + g_gui.scaled( 6 ) + swatch_size, swatch_top + swatch_size };
            g_render.box( swatch, g_gui.color( group.m_color ), true );
            g_render.box( swatch, g_gui.color( SWATCH_BORDER ) );

            const i32 text_y{ g_gui.text_center_y( bounds.top, bounds.bottom - bounds.top ) };
            g_render.text( MENU_FONT( ), point_t{ swatch.right + g_gui.scaled( 6 ), text_y },
                           g_gui.color( gui::mix_color( LIST_TEXT_IDLE, MENU_TEXT, selected ) ),
                           reinterpret_cast< const char * >( group.m_name ) );

            usize here{ };
            for ( usize player{ }; player < s_player_count; ++player )
            {
                here += ( s_player_rows[ player ].m_groups & ( 1u << index ) ) != 0;
            }

            char counts[ 32 ]{ };
            str::format_to( counts, sizeof( counts ), ASTRAL_XORSTR( "{} here | {} saved" ).get( ), here,
                            saved_group_members( index ) );
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ bounds.right - g_gui.scaled( 6 ),
                                    g_gui.tiny_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( MENU_TEXT_DIM ), counts, false, render::text_align::right );
        }
    }

private:
    usize defined_count( ) const
    {
        usize count{ };
        for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
        {
            count += player_group_defined( index );
        }
        return count;
    }

    rect_t row_bounds( usize row ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( 16 + static_cast< i32 >( row ) * 23 ) };
        return rect_t{ this->m_bounds.left, top, this->m_bounds.right, top + g_gui.scaled( 21 ) };
    }

    u8  m_hover[ config::PLAYER_GROUP_COUNT ]{ };
    u8  m_selected[ config::PLAYER_GROUP_COUNT ]{ };
    u64 m_last_tick{ };
};

class player_group_members_widget : public gui::baseobject
{
public:
    player_group_members_widget( ) : gui::baseobject{ ASTRAL_XORSTR_STATIC( "Members" ) }
    {
    }

    i32 measure( ) const override
    {
        return g_gui.scaled( 24 + math::bigger( 1, static_cast< i32 >( saved_group_members( s_selected_group ) ) ) * 17 );
    }

    void think( ) override
    {
        if ( this->measure( ) != this->m_bounds.bottom - this->m_bounds.top )
        {
            g_gui.invalidate_layout( );
        }

        const u16 bit{ static_cast< u16 >( 1u << s_selected_group ) };
        usize     row{ };
        this->m_hovered_remove = -1;
        for ( const config::player_tag_t &entry : g_cfg.m_player_tags )
        {
            if ( !entry.m_account || !( entry.m_groups & bit ) )
            {
                continue;
            }

            if ( !g_gui.hit_test( this->remove_bounds( row++ ) ) )
            {
                continue;
            }

            set_cursor( );
            this->m_hovered_remove = static_cast< i32 >( row - 1 );
            if ( g_input.pressed( VK_LBUTTON ) )
            {
                g_cfg.set_player_groups( entry.m_account, static_cast< u16 >( entry.m_groups & ~bit ) );
                refresh_player_list( );
                return;
            }
        }
    }

    void draw( ) const override
    {
        const i32 left{ this->m_bounds.left + g_gui.scaled( 2 ) };
        const i32 right{ this->m_bounds.right - g_gui.scaled( 2 ) };
        g_render.line( point_t{ left, this->m_bounds.top + g_gui.scaled( 3 ) },
                       point_t{ right, this->m_bounds.top + g_gui.scaled( 3 ) },
                       g_gui.color( CARD_RULE ) );
        g_render.text( MENU_FONT_TINY( ), point_t{ left, this->m_bounds.top + g_gui.scaled( 8 ) },
                       g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "MEMBERS" ) );

        const usize members{ saved_group_members( s_selected_group ) };
        char        count[ 12 ]{ };
        str::format_to( count, sizeof( count ), ASTRAL_XORSTR( "{}" ).get( ), members );
        g_render.text( MENU_FONT_TINY( ), point_t{ right, this->m_bounds.top + g_gui.scaled( 8 ) },
                       g_gui.color( MENU_TEXT_DIM ), count, false, render::text_align::right );

        if ( !members )
        {
            const rect_t bounds{ this->row_bounds( 0 ) };
            g_render.text( MENU_FONT( ),
                           point_t{ left, g_gui.text_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( MENU_TEXT_DIM ), ASTRAL_XORSTR_STATIC( "No players have this tag" ) );
            return;
        }

        const u16 bit{ static_cast< u16 >( 1u << s_selected_group ) };
        usize     row{ };
        for ( const config::player_tag_t &entry : g_cfg.m_player_tags )
        {
            if ( !entry.m_account || !( entry.m_groups & bit ) )
            {
                continue;
            }

            const rect_t             bounds{ this->row_bounds( row ) };
            const player_list_row_t *online{ connected_player( entry.m_account ) };
            const i32                text_y{ g_gui.text_center_y( bounds.top, bounds.bottom - bounds.top ) };

            const i32 dot{ g_gui.scaled( 4 ) };
            const i32 dot_top{ ( bounds.top + bounds.bottom - dot ) / 2 };
            g_render.box( rect_t{ left, dot_top, left + dot, dot_top + dot },
                          g_gui.color( online ? color_t{ 75, 218, 174 } : MENU_TEXT_FAINT ), true );

            char label[ 64 ]{ };
            if ( online )
            {
                str::copy( label, sizeof( label ), online->m_name );
            } else if ( entry.m_alias[ 0 ] )
            {
                str::copy( label, sizeof( label ), reinterpret_cast< const char * >( entry.m_alias ) );
            } else
            {
                str::format_to( label, sizeof( label ), ASTRAL_XORSTR( "ID {}" ).get( ), entry.m_account );
            }

            char name[ 64 ]{ };
            gui::fit_text( name, label, right - left - g_gui.scaled( 60 ) );
            g_render.text( MENU_FONT( ), point_t{ left + g_gui.scaled( 9 ), text_y },
                           g_gui.color( online ? LIST_TEXT_IDLE : MENU_TEXT_MUTED ), name );
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ right - g_gui.scaled( 14 ),
                                    g_gui.tiny_center_y( bounds.top, bounds.bottom - bounds.top ) },
                           g_gui.color( MENU_TEXT_DIM ),
                           online ? ASTRAL_XORSTR_STATIC( "here" ) : ASTRAL_XORSTR_STATIC( "offline" ), false,
                           render::text_align::right );

            const rect_t remove{ this->remove_bounds( row ) };
            gui::draw_icon_glyph( ICON_CROSS,
                                  point_t{ ( remove.left + remove.right ) / 2, ( remove.top + remove.bottom ) / 2 },
                                  static_cast< i32 >( row ) == this->m_hovered_remove ? MENU_TEXT : MENU_TEXT_DIM );
            ++row;
        }
    }

private:
    rect_t row_bounds( usize row ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( 20 + static_cast< i32 >( row ) * 17 ) };
        return rect_t{ this->m_bounds.left, top, this->m_bounds.right, top + g_gui.scaled( 17 ) };
    }

    rect_t remove_bounds( usize row ) const
    {
        const rect_t bounds{ this->row_bounds( row ) };
        return rect_t{ bounds.right - g_gui.scaled( 12 ), bounds.top, bounds.right, bounds.bottom };
    }

    i32 m_hovered_remove{ -1 };
};

static void save_player_alias( )
{
    if ( !s_selected_account )
    {
        return;
    }
    const string_view alias{ s_alias_input->value( ) };
    if ( !g_cfg.set_player_alias( s_selected_account, alias ) ||
         !g_player_cloud.set_local_alias( s_selected_account, alias ) )
    {
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Playerlist" ),
                           ASTRAL_XORSTR_STATIC( "Alias list is full" ) );
    }
    refresh_player_list( );
}

static void toggle_selected_spectate( )
{
    const player_list_row_t *player{ selected_player( ) };
    if ( player )
    {
        g_enhanced_spectate.toggle( player->m_account );
    }
}

static void refresh_player_databases( )
{
    const bool started{ g_player_cloud.refresh_lists( ) };
    push_notification( started ? notify_kind_t::information : notify_kind_t::warning,
                       ICON_PLAYERS, ASTRAL_XORSTR_STATIC( "Player databases" ),
                       started ? ASTRAL_XORSTR_STATIC( "Refreshing Lunaris and Surface lists" )
                               : ASTRAL_XORSTR_STATIC( "A player database request is already running" ) );
}

static void save_player_integration_keys( )
{
    const string_view steam{ s_steam_api_key_input->value( ) };
    const string_view history{ s_steam_history_key_input->value( ) };
    atl::memset( g_cfg.m_misc.m_steam_api_key, 0, sizeof( g_cfg.m_misc.m_steam_api_key ) );
    atl::memset( g_cfg.m_misc.m_steam_history_api_key, 0, sizeof( g_cfg.m_misc.m_steam_history_api_key ) );
    str::copy( g_cfg.m_misc.m_steam_api_key, sizeof( g_cfg.m_misc.m_steam_api_key ), steam.data( ), steam.size( ) );
    str::copy( g_cfg.m_misc.m_steam_history_api_key, sizeof( g_cfg.m_misc.m_steam_history_api_key ), history.data( ), history.size( ) );
    g_player_cloud.set_integration_keys( steam, history );
    push_notification( notify_kind_t::success, ICON_PLAYERS,
                       ASTRAL_XORSTR_STATIC( "Player integrations" ),
                       ASTRAL_XORSTR_STATIC( "API keys applied; save the config to persist them" ) );
}

static void sync_player_integration_keys( )
{
    if ( s_steam_api_key_input )
    {
        s_steam_api_key_input->set_value( string_view{ g_cfg.m_misc.m_steam_api_key } );
    }
    if ( s_steam_history_key_input )
    {
        s_steam_history_key_input->set_value( string_view{ g_cfg.m_misc.m_steam_history_api_key } );
    }
    g_player_cloud.set_integration_keys( string_view{ g_cfg.m_misc.m_steam_api_key },
                                         string_view{ g_cfg.m_misc.m_steam_history_api_key } );
}

static void import_tf2pl_player_list( )
{
    const bool loaded{ g_player_cloud.load_tf2pl_file( s_tf2pl_path_input->value( ) ) };
    push_notification( loaded ? notify_kind_t::success : notify_kind_t::error,
                       ICON_PLAYERS, ASTRAL_XORSTR_STATIC( "TF2PL player list" ),
                       loaded ? ASTRAL_XORSTR_STATIC( "Player list imported into Astral" )
                              : ASTRAL_XORSTR_STATIC( "Could not import player list" ) );
}

static void request_cloud_report( )
{
    const player_list_row_t *player{ selected_player( ) };
    if ( !player || s_report_reason_input->value( ).empty( ) )
    {
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Cloud report" ),
                           ASTRAL_XORSTR_STATIC( "Select a player and enter a reason" ) );
        return;
    }
    const player_cloud::report_result_t result{ g_player_cloud.submit_report(
        player->m_account, player->m_real_name, s_report_reason_input->value( ),
        s_alias_input->value( ) ) };
    if ( result != player_cloud::report_result_t::started )
    {
        const char *message{ };
        switch ( result )
        {
            case player_cloud::report_result_t::no_session:
#if defined( ASTRAL_DEV )
                message = ASTRAL_XORSTR_STATIC( "Cloud reports unavailable in Debug; No auth." );
#else
                message = ASTRAL_XORSTR_STATIC( "Cloud session unavailable" );
#endif
                break;
            case player_cloud::report_result_t::busy:
                message = ASTRAL_XORSTR_STATIC( "Cloud request in progress" );
                break;
            case player_cloud::report_result_t::invalid:
                message = ASTRAL_XORSTR_STATIC( "Invalid report details" );
                break;
            default:
                message = ASTRAL_XORSTR_STATIC( "Could not start Cloud request" );
                break;
        }
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Cloud report" ),
                           message );
    }
}

static void create_player_group( )
{
    if ( s_new_group_input->value( ).empty( ) )
    {
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Playerlist" ),
                           ASTRAL_XORSTR_STATIC( "Enter a tag name" ) );
        return;
    }

    const usize index{ g_cfg.add_player_group( s_new_group_input->value( ) ) };
    if ( index == config::PLAYER_GROUP_COUNT )
    {
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Playerlist" ),
                           ASTRAL_XORSTR_STATIC( "Every tag slot is taken" ) );
        return;
    }

    g_cfg.m_player_groups[ index ].m_color = PLAYER_GROUP_PALETTE[ index ];
    s_new_group_input->set_value( { } );
    s_selected_group = index;
}

static void rename_selected_group( )
{
    if ( !g_cfg.rename_player_group( s_selected_group, s_group_name_input->value( ) ) )
    {
        push_notification( notify_kind_t::warning, ICON_PLAYERS,
                           ASTRAL_XORSTR_STATIC( "Playerlist" ),
                           ASTRAL_XORSTR_STATIC( "Enter a tag name" ) );
    }
}

static void delete_selected_group( )
{
    g_cfg.remove_player_group( s_selected_group );
    refresh_player_list( );
}

static void build_player_list( )
{
    gui::tab *players{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Playerlist" ), ICON_PLAYERS ) };

    gui::sub_tab   *list{ players->add_subtab( ASTRAL_XORSTR_STATIC( "Players" ) ) };
    gui::group_box *connected{ list->add_group( ICON_PLAYERS,
                                                ASTRAL_XORSTR_STATIC( "Connected players" ), gui::column_t::left ) };
    gui::group_box *player{ list->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Player" ), gui::column_t::right ) };

    connected->add< player_list_widget >( );
    player->add< player_tag_widget >( );

    s_alias_input = player->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Alias" ) );
    s_alias_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Save" ), &save_player_alias );
    s_alias_input->set_limit( 32 );

    player->add< gui::button >( ASTRAL_XORSTR_STATIC( "Toggle spectate" ),
                                &toggle_selected_spectate )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Locally spectates the selected player while alive or dead." ) );
    player->add< gui::button >( ASTRAL_XORSTR_STATIC( "Mark cheater and party" ),
                                &mark_selected_cheater_and_party )
        ->set_tooltip( ASTRAL_XORSTR_STATIC(
            "Marks the selected player as Cheater and their detected party members as Suspected cheater." ) );
    player->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Spectate in third person" ),
                                  &g_cfg.m_misc.m_enhanced_spectate_thirdperson );

    s_report_reason_input = player->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Reason" ) );
    s_report_reason_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Save to Database" ), &request_cloud_report );
    s_report_reason_input->set_render_on( &selected_is_cheater );

    gui::sub_tab   *tags{ players->add_subtab( ASTRAL_XORSTR_STATIC( "Tags" ) ) };
    gui::group_box *defined{ tags->add_group( ICON_PLAYERS, ASTRAL_XORSTR_STATIC( "Tags" ), gui::column_t::left ) };
    gui::group_box *editor{ tags->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Edit tag" ), gui::column_t::right ) };

    s_new_group_input = defined->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "New tag" ) );
    s_new_group_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Create" ), &create_player_group );
    s_new_group_input->set_limit( 16 );
    defined->add< player_group_list_widget >( );

    s_group_name_input = editor->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Name" ) );
    s_group_name_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Rename" ), &rename_selected_group );
    s_group_name_input->set_limit( 16 )->set_render_on( &has_selected_group );

    for ( usize index{ }; index < config::PLAYER_GROUP_COUNT; ++index )
    {
        editor->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Color" ), &g_cfg.m_player_groups[ index ].m_color, false )
            ->set_render_on( &s_group_shown[ index ] )
            ->set_instant_reveal( );
    }

    editor->add< player_group_members_widget >( )->set_render_on( &has_selected_group );
    editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Delete tag" ), &delete_selected_group )
        ->set_render_on( &has_selected_group );

    gui::sub_tab   *marks{ players->add_subtab( ASTRAL_XORSTR_STATIC( "Marks" ) ) };
    gui::group_box *mark_editor{ marks->add_group( ICON_PLAYERS,
                                                   ASTRAL_XORSTR_STATIC( "TF2PL marks" ), gui::column_t::left ) };
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Cheater" ), &toggle_mark_cheater );
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Suspicious" ), &toggle_mark_suspicious );
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Exploiter" ), &toggle_mark_exploiter );
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Racist" ), &toggle_mark_racist );
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Hostile" ), &toggle_mark_hostile );
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Suspected cheater" ), &toggle_mark_suspected );
    mark_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Blacklisted" ), &toggle_mark_blacklisted );
    gui::group_box *ban_editor{ marks->add_group( ICON_BAN,
                                                  ASTRAL_XORSTR_STATIC( "Ban and safety marks" ), gui::column_t::right ) };
    ban_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "VAC banned" ), &toggle_mark_vac );
    ban_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Game banned" ), &toggle_mark_game );
    ban_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "SourceBanned" ), &toggle_mark_source );
    ban_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Pedophilia" ), &toggle_mark_pedophilia );
    ban_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "UList user" ), &toggle_mark_ulist );
    ban_editor->add< gui::button >( ASTRAL_XORSTR_STATIC( "Zoophile/zoo jokes" ), &toggle_mark_zoophile );

    gui::sub_tab   *integrations{ players->add_subtab( ASTRAL_XORSTR_STATIC( "Integrations" ) ) };
    gui::group_box *api_keys{ integrations->add_group( ICON_SETTINGS,
                                                       ASTRAL_XORSTR_STATIC( "API keys" ), gui::column_t::left ) };
    s_steam_api_key_input = api_keys->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Steam Web API key" ) );
    s_steam_api_key_input->set_limit( 64 );
    s_steam_history_key_input = api_keys->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "SteamHistory API key" ) );
    s_steam_history_key_input->set_limit( 128 );
    api_keys->add< gui::button >( ASTRAL_XORSTR_STATIC( "Apply API keys" ), &save_player_integration_keys );
    sync_player_integration_keys( );
    gui::group_box *player_lists{ integrations->add_group( ICON_PLAYERS,
                                                           ASTRAL_XORSTR_STATIC( "TF2PL / TF2BD lists" ), gui::column_t::right ) };
    s_tf2pl_path_input = player_lists->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Player-list JSON path" ) );
    s_tf2pl_path_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Import" ), &import_tf2pl_player_list );
    player_lists->add< gui::button >( ASTRAL_XORSTR_STATIC( "Refetch database lists" ),
                                      &refresh_player_databases )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Fetches the Lunaris and Surface database lists once on demand. There is no refresh interval." ) );
}

static bool is_right_events_on( )
{
    return g_cfg.m_events.m_enabled && g_cfg.m_events.m_notify_targets[ config::NOTIF_EVENT_VOTE_START ] == true;
}

static void build_misc_general( gui::tab *misc )
{
    gui::sub_tab   *general{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "General" ) ) };
    gui::group_box *camera{ general->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Camera" ), gui::column_t::left ) };
    gui::group_box *events{ general->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Events" ), gui::column_t::right ) };

    gui::checkbox   *fov{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Field of view" ), &g_cfg.m_misc.m_fov_override ) };
    gui::slider_int *degrees{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Degrees" ), &g_cfg.m_misc.m_fov, 60, 140 ) };
    auto             aspect_ratio_changer{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Aspect ratio changer" ), &g_cfg.m_misc.m_aspect_ratio_changer ) };
    auto             aspect_ratio_amt{ camera->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Ratio" ), &g_cfg.m_misc.m_aspect_ratio_amt, 0.5f, 2.0f ) };
    aspect_ratio_amt->set_render_on( &g_cfg.m_misc.m_aspect_ratio_changer );
    aspect_ratio_changer->set_tooltip( ASTRAL_XORSTR_STATIC( "Lets the cheat control the viewport aspect ratio of the game. This does not affect game resolution at all or hud scaling." ) );

    fov->set_tooltip( ASTRAL_XORSTR_STATIC( "Overrides the game's field of view." ) )->set_keywords( ASTRAL_XORSTR_STATIC( "fov zoom" ) );
    degrees->set_render_on( &g_cfg.m_misc.m_fov_override );

    gui::checkbox *thirdperson{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Thirdperson" ),
                                                              &g_cfg.m_misc.m_thirdperson ) };

    thirdperson->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_thirdperson_key );

    gui::slider_int *distance{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Distance" ), &g_cfg.m_misc.m_thirdperson_distance, 30, 300 ) };
    gui::slider_int *side{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Side offset" ), &g_cfg.m_misc.m_thirdperson_side, -100, 100 ) };
    gui::slider_int *height{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Height offset" ), &g_cfg.m_misc.m_thirdperson_height, -100, 100 ) };
    gui::checkbox   *collision{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Camera collision" ), &g_cfg.m_misc.m_thirdperson_collision ) };
    gui::checkbox   *smooth{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Smooth transition" ), &g_cfg.m_misc.m_thirdperson_smooth ) };
    gui::slider_int *smooth_time{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Transition time" ), &g_cfg.m_misc.m_thirdperson_smooth_time, 0, 1000, ASTRAL_XORSTR_STATIC( "ms" ) ) };

    smooth->set_tooltip( ASTRAL_XORSTR_STATIC( "Pulls the camera out of the head over time instead of cutting straight to it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "thirdperson smooth lerp transition third person" ) );
    smooth_time->set_render_on( &g_cfg.m_misc.m_thirdperson_smooth );

    distance->set_render_on( &g_cfg.m_misc.m_thirdperson );
    side->set_render_on( &g_cfg.m_misc.m_thirdperson );
    height->set_render_on( &g_cfg.m_misc.m_thirdperson );
    collision->set_render_on( &g_cfg.m_misc.m_thirdperson );

    gui::checkbox   *freecam{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Freecam" ), &g_cfg.m_misc.m_freecam ) };
    gui::slider_int *freecam_speed{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Freecam speed" ), &g_cfg.m_misc.m_freecam_speed, 100, 3000, ASTRAL_XORSTR_STATIC( "u/s" ) ) };

    freecam->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_freecam_key );
    freecam->set_tooltip( ASTRAL_XORSTR_STATIC( "Detaches the camera and flies it with your movement keys, jump and duck. "
                                               "Your player stands still and keeps its aim until you leave." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "freecam free camera noclip spectate" ) );
    freecam_speed->set_render_on( &g_cfg.m_misc.m_freecam );

    static const char *const PROJECTILE_CAMERA_MODES[]{ ASTRAL_XORSTR_STATIC( "Follow" ), ASTRAL_XORSTR_STATIC( "Impact" ), ASTRAL_XORSTR_STATIC( "Landing" ) };

    gui::checkbox           *projectile_camera{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Projectile camera" ), &g_cfg.m_misc.m_projectile_camera ) };
    static const char *const PROJECTILE_CAMERA_HUDS[]{ ASTRAL_XORSTR_STATIC( "Crosshair" ), ASTRAL_XORSTR_STATIC( "Bomber drone" ) };

    gui::combo_box  *projectile_camera_mode{ camera->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Camera mode" ), PROJECTILE_CAMERA_MODES, config::PROJECTILE_CAMERA_COUNT, &g_cfg.m_misc.m_projectile_camera_mode ) };
    gui::combo_box  *projectile_camera_hud{ camera->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Camera HUD" ), PROJECTILE_CAMERA_HUDS, config::PROJECTILE_CAMERA_HUD_COUNT, &g_cfg.m_misc.m_projectile_camera_hud ) };
    gui::slider_int *projectile_camera_distance{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Camera distance" ), &g_cfg.m_misc.m_projectile_camera_distance, 0, 1000, ASTRAL_XORSTR_STATIC( "u" ) ) };
    gui::slider_int *projectile_camera_width{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Window width" ), &g_cfg.m_misc.m_projectile_camera_width, 160, 800, ASTRAL_XORSTR_STATIC( "px" ) ) };
    gui::checkbox   *spy_camera{ camera->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Spy camera" ), &g_cfg.m_misc.m_spy_camera ) };
    gui::slider_int *spy_camera_range{ camera->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Spy camera range" ), &g_cfg.m_misc.m_spy_camera_range, 200, 3000, ASTRAL_XORSTR_STATIC( "u" ) ) };

    projectile_camera->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows a second view in a window. Follow rides behind your newest projectile until it "
                                                         "is gone or a sticky sticks; Impact hovers over where your current aim would land; "
                                                         "Landing hovers over where your last shot lands and waits there while its projectile "
                                                         "is around. Impact and Landing look straight down, and the camera distance is their height." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "projectile camera rocket cam impact landing window" ) );
    projectile_camera_mode->set_render_on( &g_cfg.m_misc.m_projectile_camera );
    projectile_camera_hud->set_render_on( &g_cfg.m_misc.m_projectile_camera );
    projectile_camera_distance->set_render_on( &g_cfg.m_misc.m_projectile_camera );
    projectile_camera_width->set_tooltip( ASTRAL_XORSTR_STATIC( "Shared width for the projectile and Spy secondary-camera window." ) );
    spy_camera->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows the first-person view of the nearest living enemy Spy in range. It reuses the secondary camera and takes priority while a Spy is nearby." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "spy camera nearby perspective warning" ) );
    spy_camera_range->set_render_on( &g_cfg.m_misc.m_spy_camera );

    events->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_events.m_enabled )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows notifications to be pushed when certain game events happen (such as votekicks or class switches)." ) );

    static const char *const NOTIF_TARGETS[]{
        ASTRAL_XORSTR_STATIC( "Vote calls" ),
        ASTRAL_XORSTR_STATIC( "Vote casts" ),
        ASTRAL_XORSTR_STATIC( "Class changes" ),
        ASTRAL_XORSTR_STATIC( "Damage" ),
        ASTRAL_XORSTR_STATIC( "Player connect" ),
        ASTRAL_XORSTR_STATIC( "Player disconnect" ),
    };

    events->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Notification targets" ), NOTIF_TARGETS, config::NOTIF_EVENT_COUNT, g_cfg.m_events.m_notify_targets )
        ->set_render_on( &g_cfg.m_events.m_enabled );

    events->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Notify party on vote call" ), &g_cfg.m_events.m_vote_start_notify_party )
        ->set_render_on( &is_right_events_on )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Posts a message in your party's chat when a votekick is called by someone." ) );

    events->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Save crash diagnostics" ), &g_cfg.m_misc.m_crash_reports )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Stores crash addresses and failed signature diagnostics locally in the AstralMod folder. Nothing is uploaded." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "crash report diagnostics local privacy" ) );

    gui::group_box *party{ general->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Party networking" ),
                                               gui::column_t::right ) };

    gui::checkbox *enabled{ party->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ),
                                                         &g_cfg.m_misc.m_party_network ) };
    gui::keybind  *bullet{ party->add< gui::keybind >(
        ASTRAL_XORSTR_STATIC( "Bullet pop" ), &g_cfg.m_auto_vacc.m_request_bullet_key ) };
    gui::keybind  *blast{ party->add< gui::keybind >(
        ASTRAL_XORSTR_STATIC( "Blast pop" ), &g_cfg.m_auto_vacc.m_request_blast_key ) };
    gui::keybind  *fire{ party->add< gui::keybind >(
        ASTRAL_XORSTR_STATIC( "Fire pop" ), &g_cfg.m_auto_vacc.m_request_fire_key ) };

    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Exchanges Astral data through party chat." ) );
    bullet->set_tooltip( ASTRAL_XORSTR_STATIC( "Ask the Astral medic healing you to pop bullet resistance." ) );
    blast->set_tooltip( ASTRAL_XORSTR_STATIC( "Ask the Astral medic healing you to pop blast resistance." ) );
    fire->set_tooltip( ASTRAL_XORSTR_STATIC( "Ask the Astral medic healing you to pop fire resistance." ) );
    bullet->set_render_on( &g_cfg.m_misc.m_party_network );
    blast->set_render_on( &g_cfg.m_misc.m_party_network );
    fire->set_render_on( &g_cfg.m_misc.m_party_network );

    gui::group_box *watchdog{ general->add_group( ICON_BAN, ASTRAL_XORSTR_STATIC( "Watchdog" ),
                                                  gui::column_t::left ) };

    gui::checkbox *watchdog_enabled{ watchdog->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ),
                                                                     &g_cfg.m_misc.m_watchdog ) };

    watchdog_enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Conclusive cheat-only detections. Adds the Cheater mark to the local player list." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "cheater detection watchdog local mark" ) );

    static const char *const WATCHDOG_QUALITY[]{
        ASTRAL_XORSTR_STATIC( "Precision" ),
        ASTRAL_XORSTR_STATIC( "Performance" ),
    };
    watchdog->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Quality" ), WATCHDOG_QUALITY,
                                     config::misc_t::WATCHDOG_QUALITY_COUNT,
                                     &g_cfg.m_misc.m_watchdog_quality )
        ->set_render_on( &g_cfg.m_misc.m_watchdog )
        ->set_tooltip( ASTRAL_XORSTR_STATIC(
            "Precision uses full player performance samples and checks visible loadouts together. "
            "Performance uses smaller samples and staggers loadout checks to prevent frame-time spikes." ) );

    gui::checkbox *behavior_scanner{ watchdog->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Behavior scanner" ), &g_cfg.m_misc.m_behavior_scanner ) };
    behavior_scanner->set_tooltip( ASTRAL_XORSTR_STATIC(
        "Uses conservative aim and packet heuristics. It can only add the local Suspected cheater mark." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "suspected heuristic behavior detection" ) );

    gui::group_box *privacy{ general->add_group( ICON_BAN, ASTRAL_XORSTR_STATIC( "Privacy" ),
                                                 gui::column_t::left ) };

    static const char *const TELEMETRY_BLOCKER_MODES[]{
        ASTRAL_XORSTR_STATIC( "Off" ),
        ASTRAL_XORSTR_STATIC( "Lite" ),
        ASTRAL_XORSTR_STATIC( "Balanced" ),
        ASTRAL_XORSTR_STATIC( "Aggressive" ),
    };

    privacy->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Telemetry blocker" ), TELEMETRY_BLOCKER_MODES,
                                    config::TELEMETRY_BLOCKER_COUNT, &g_cfg.m_misc.m_telemetry_blocker )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Disables game ConVars that upload usage and hardware statistics. "
                                            "Balanced and Aggressive also block stats rows, sessions and tier0 telemetry; "
                                            "Aggressive additionally hides the stats uploader interface." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "telemetry stats privacy steam hardware cpu gpu upload" ) );
}

inline constexpr i32 RADIO_PANEL_HEIGHT{ 38 };
inline constexpr i32 RADIO_PANEL_PAD{ 5 };
inline constexpr i32 RADIO_PANEL_GUTTER{ 19 };
inline constexpr i32 RADIO_PANEL_NAME_ROW{ 15 };
inline constexpr i32 RADIO_PANEL_STATUS_ROW{ 12 };
inline constexpr i32 RADIO_PANEL_TRACK{ 2 };

inline constexpr i32   RADIO_GLOW_REACH{ 16 };
inline constexpr u32   RADIO_GLOW_PERIOD{ 1400 };
inline constexpr i32   RADIO_GLOW_ALPHA{ 150 };
inline constexpr float RADIO_GLOW_FLOOR{ 0.45f };
inline constexpr float RADIO_GLOW_SWING{ 0.55f };

static void draw_panel_glow( i32 left, i32 right, i32 bottom, color_t glow, u32 wave )
{
    const float turn{ static_cast< float >( wave ) / static_cast< float >( RADIO_GLOW_PERIOD ) };
    const float swell{ ( math::sin( turn * PI * 2.0f ) + 1.0f ) * 0.5f };

    const i32 reach{ g_gui.scaled( RADIO_GLOW_REACH ) };
    const i32 height{ static_cast< i32 >(
        static_cast< float >( reach ) *
        ( RADIO_GLOW_FLOOR + RADIO_GLOW_SWING * swell ) ) };

    for ( i32 row{ }; row < height; ++row )
    {
        const i32 remaining{ height - row };
        const u8  alpha{ static_cast< u8 >( RADIO_GLOW_ALPHA * remaining *
                                            remaining / ( height * height ) ) };

        g_render.box( rect_t{ left, bottom - row - 1, right, bottom - row },
                      g_gui.color( gui::with_alpha( glow, alpha ) ),
                      true );
    }
}

static color_t radio_glyph_color( radio_phase_t phase )
{
    switch ( phase )
    {
        case radio_phase_t::playing: return g_theme.m_accent;
        case radio_phase_t::failed: return RADIO_FAILED_COLOR;
        case radio_phase_t::off: return MENU_TEXT_FAINT;
        default: return MENU_TEXT_MUTED;
    }
}

static void radio_status_line( char *out, usize capacity, radio_phase_t phase )
{
    switch ( phase )
    {
        case radio_phase_t::off:
            str::format_to( out, capacity, ASTRAL_XORSTR( "Off" ).get( ) );
            return;

        case radio_phase_t::connecting:
            str::format_to( out, capacity, ASTRAL_XORSTR( "Connecting" ).get( ) );
            return;

        case radio_phase_t::buffering:
            str::format_to( out, capacity, ASTRAL_XORSTR( "Buffering {}%" ).get( ),
                            g_radio.buffer_percent( ) );
            return;

        case radio_phase_t::paused:
            str::format_to( out, capacity, ASTRAL_XORSTR( "Paused" ).get( ) );
            return;

        case radio_phase_t::failed:
            str::format_to( out, capacity, ASTRAL_XORSTR( "Connection failed" ).get( ) );
            return;

        default:
            str::format_to( out, capacity, ASTRAL_XORSTR( "Playing at {}%" ).get( ),
                            g_cfg.m_radio.m_volume );
            return;
    }
}

class radio_panel : public gui::baseobject
{
public:
    i32 measure( ) const override
    {
        return g_gui.scaled( RADIO_PANEL_HEIGHT );
    }

    void think( ) override
    {
        this->m_wave = ( this->m_wave + gui::animation_delta( this->m_wave_tick ) ) %
                       RADIO_GLOW_PERIOD;
    }

    void draw( ) const override
    {
        const radio_phase_t phase{ g_radio.phase( ) };

        g_render.box( this->m_bounds, g_gui.color( CONTROL_BACKGROUND ), true );

        const i32 inner_left{ this->m_bounds.left + 1 };
        const i32 inner_right{ this->m_bounds.right - 1 };
        const i32 inner_bottom{ this->m_bounds.bottom - 1 };
        const i32 track_top{ inner_bottom - g_gui.scaled( RADIO_PANEL_TRACK ) };
        const i32 filled{ ( inner_right - inner_left ) *
                          math::clamp( g_radio.buffer_percent( ), 0, 100 ) / 100 };

        g_render.box( rect_t{ inner_left, track_top, inner_right, inner_bottom },
                      g_gui.color( CONTROL_BACKGROUND_HOVER ), true );
        g_render.box( rect_t{ inner_left, track_top, inner_left + filled, inner_bottom },
                      g_gui.color( g_theme.m_accent ), true );

        if ( phase == radio_phase_t::connecting || phase == radio_phase_t::failed )
        {
            draw_panel_glow( inner_left, inner_right, inner_bottom,
                             phase == radio_phase_t::failed ? RADIO_FAILED_COLOR : g_theme.m_accent,
                             this->m_wave );
        }

        const i32 left{ this->m_bounds.left + g_gui.scaled( RADIO_PANEL_PAD ) };
        const i32 top{ this->m_bounds.top + g_gui.scaled( RADIO_PANEL_PAD ) };
        const i32 text_left{ this->m_bounds.left + g_gui.scaled( RADIO_PANEL_GUTTER ) };

        gui::draw_icon_at( ICON_ZAP, point_t{ left, top },
                           g_gui.color( radio_glyph_color( phase ) ),
                           g_gui.scaled( MENU_ICON_SIZE ) );

        g_render.text( MENU_FONT( ),
                       point_t{ text_left,
                                g_gui.text_center_y( top, g_gui.scaled( RADIO_PANEL_NAME_ROW ) ) },
                       g_gui.color( MENU_TEXT ), g_radio.now_playing( ) );

        char status[ 48 ]{ };
        radio_status_line( status, sizeof( status ), phase );

        g_render.text( MENU_FONT_TINY( ),
                       point_t{ text_left,
                                g_gui.text_center_y( top + g_gui.scaled( RADIO_PANEL_NAME_ROW ),
                                                     g_gui.scaled( RADIO_PANEL_STATUS_ROW ) ) },
                       g_gui.color( MENU_TEXT_DIM ), status );

        g_render.box( rect_t{ this->m_bounds.left, this->m_bounds.top, this->m_bounds.right - 1,
                              this->m_bounds.bottom - 1 },
                      g_gui.color( CONTROL_BORDER ) );
    }

private:
    u32 m_wave{ };
    u64 m_wave_tick{ };
};

inline constexpr usize RADIO_TRANSPORT_COUNT{ 3 };
class radio_transport : public gui::baseobject
{
public:
    i32 measure( ) const override
    {
        return g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) +
               g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) + g_gui.scaled( WIDGET_SEGMENT_GAP );
    }

    void think( ) override
    {
        const u32 delta{ gui::animation_delta( this->m_last_tick ) };

        for ( usize index{ }; index < RADIO_TRANSPORT_COUNT; ++index )
        {
            const bool hovered{ g_gui.hit_test( this->segment_bounds( index ) ) };
            if ( hovered )
            {
                set_cursor( );
            }

            gui::step_mix( this->m_segments[ index ].m_hover_mix, hovered, delta,
                           WIDGET_HOVER_DURATION );

            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                press( index );
            }
        }
    }

    void draw( ) const override
    {
        for ( usize index{ }; index < RADIO_TRANSPORT_COUNT; ++index )
        {
            const rect_t segment{ this->segment_bounds( index ) };
            const u8     hover{ gui::eased_mix( this->m_segments[ index ].m_hover_mix ) };

            g_render.box( segment,
                          g_gui.color( gui::mix_color( CHIP_IDLE_BACKGROUND,
                                                       CHIP_HOVER_BACKGROUND, hover ) ),
                          true );

            g_render.text( MENU_FONT_TINY( ),
                           point_t{ ( segment.left + segment.right ) / 2,
                                    g_gui.tiny_center_y( segment.top,
                                                         segment.bottom - segment.top ) },
                           g_gui.color( gui::mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT,
                                                        hover ) ),
                           label( index ), false, render::text_align::center );
        }
    }

private:
    rect_t segment_bounds( usize index ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) };
        const i32 span{ this->m_bounds.right - this->m_bounds.left };
        const i32 count{ static_cast< i32 >( RADIO_TRANSPORT_COUNT ) };

        return rect_t{ this->m_bounds.left + static_cast< i32 >( index ) * span / count,
                       top,
                       this->m_bounds.left + static_cast< i32 >( index + 1 ) * span / count,
                       top + g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) };
    }

    static const char *label( usize index )
    {
        switch ( index )
        {
            case 0: return ASTRAL_XORSTR_STATIC( "Prev" );
            case 1:
                return g_radio.paused( ) ? ASTRAL_XORSTR_STATIC( "Play" )
                                         : ASTRAL_XORSTR_STATIC( "Pause" );
            default: return ASTRAL_XORSTR_STATIC( "Next" );
        }
    }

    static void press( usize index )
    {
        switch ( index )
        {
            case 0: g_radio.step_station( -1 ); return;
            case 1: g_radio.toggle_paused( ); return;
            default: g_radio.step_station( 1 ); return;
        }
    }

    gui::row_state_t m_segments[ RADIO_TRANSPORT_COUNT ]{ };
};

static bool radio_indicator_style_shown( )
{
    return g_cfg.m_radio.m_enabled && g_cfg.m_radio.m_indicator;
}

static bool radio_stream_source_shown( )
{
    return g_cfg.m_radio.m_enabled && g_cfg.m_radio.m_source == config::RADIO_SOURCE_STREAM;
}

static bool radio_stream_duck_shown( )
{
    return radio_stream_source_shown( ) && g_cfg.m_radio.m_duck_on_voice;
}

static gui::list_box   *s_radio_station_list{ };
static gui::text_input *s_radio_name_input{ };
static gui::text_input *s_radio_url_input{ };

static void refresh_radio_station_list( )
{
    if ( s_radio_station_list )
        s_radio_station_list->set_items( radio_station_names( ), radio_station_count( ) );
}

static void add_radio_station( )
{
    const bool added{ radio_station_add( s_radio_name_input->value( ), s_radio_url_input->value( ) ) };
    push_notification( added ? notify_kind_t::success : notify_kind_t::error, ICON_ZAP,
                       ASTRAL_XORSTR_STATIC( "Web radio" ),
                       added ? ASTRAL_XORSTR_STATIC( "Station saved locally" )
                             : ASTRAL_XORSTR_STATIC( "Enter a unique name and an HTTP or HTTPS stream URL" ) );
    if ( added )
    {
        s_radio_name_input->set_value( { } );
        s_radio_url_input->set_value( { } );
        g_cfg.m_radio.m_station = radio_station_count( ) - 1;
        refresh_radio_station_list( );
    }
}

static void remove_radio_station( )
{
    const usize selected{ g_cfg.m_radio.m_station };
    if ( selected < RADIO_BUILTIN_STATION_COUNT )
    {
        push_notification( notify_kind_t::warning, ICON_ZAP, ASTRAL_XORSTR_STATIC( "Web radio" ),
                           ASTRAL_XORSTR_STATIC( "Built-in stations cannot be removed" ) );
        return;
    }
    g_radio.on_level_shutdown( );
    if ( !radio_station_remove( selected ) )
    {
        push_notification( notify_kind_t::error, ICON_ZAP, ASTRAL_XORSTR_STATIC( "Web radio" ),
                           ASTRAL_XORSTR_STATIC( "Could not update the local station list" ) );
        return;
    }
    const usize count{ radio_station_count( ) };
    g_cfg.m_radio.m_station = count ? math::smaller( selected, count - 1 ) : 0;
    refresh_radio_station_list( );
    push_notification( notify_kind_t::success, ICON_ZAP, ASTRAL_XORSTR_STATIC( "Web radio" ),
                       ASTRAL_XORSTR_STATIC( "Custom station removed" ) );
}

static void build_radio( gui::tab *misc )
{
    gui::sub_tab *tab{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Radio" ) ) };

    gui::group_box *player{ tab->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Radio" ),
                                            gui::column_t::left ) };
    gui::group_box *mixing{ tab->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Mixing" ),
                                            gui::column_t::left ) };
    gui::group_box *stations{ tab->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Station" ),
                                              gui::column_t::right ) };
    gui::group_box *binds{ tab->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Binds" ),
                                           gui::column_t::right ) };

    player->add< radio_panel >( );

    radio_transport *transport{ player->add< radio_transport >( ) };
    gui::checkbox   *stream{ player->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ),
                                                           &g_cfg.m_radio.m_enabled ) };

    stream->add_module< gui::keybind >( string_view{ }, &g_cfg.m_radio.m_key );

    static const char *const RADIO_SOURCES[]{
        ASTRAL_XORSTR_STATIC( "Internet radio" ), ASTRAL_XORSTR_STATIC( "System media" )
    };
    gui::combo_box *source{ player->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Source" ), RADIO_SOURCES, config::RADIO_SOURCE_COUNT,
        &g_cfg.m_radio.m_source ) };

    gui::slider_int *volume{ player->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Volume" ), &g_cfg.m_radio.m_volume, 0, 100,
        ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::checkbox   *follow{ player->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Follow game volume" ), &g_cfg.m_radio.m_scale_to_game_volume ) };

    static const char *const INDICATOR_STYLES[]{
        ASTRAL_XORSTR_STATIC( "Compact" ), ASTRAL_XORSTR_STATIC( "Detailed" )
    };

    gui::checkbox  *indicator{ player->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Indicator" ),
                                                             &g_cfg.m_radio.m_indicator ) };
    gui::combo_box *indicator_style{ player->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Indicator style" ), INDICATOR_STYLES, config::RADIO_INDICATOR_COUNT,
        &g_cfg.m_radio.m_indicator_style ) };

    stream->set_tooltip( ASTRAL_XORSTR_STATIC( "Streams an internet radio station straight into "
                                              "the game, or follows the active Windows media session." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio music stream station audio" ) );

    source->set_tooltip( ASTRAL_XORSTR_STATIC( "System media follows Spotify, a browser, or any player "
                                              "that publishes Windows media controls." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "spotify browser windows media session gsmtc external music" ) );

    follow->set_tooltip( ASTRAL_XORSTR_STATIC( "Scales the radio by the game's master volume, so "
                                              "muting the game mutes the radio too." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio volume game master follow" ) );

    indicator->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows the station, its state and an equalizer on screen. "
                                                 "Detailed adds the volume and the track the station is "
                                                 "playing. Open the menu to drag it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio indicator hud equalizer track title ducked" ) );

    indicator_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Compact is one row with the equalizer. Detailed "
                                                       "shows the volume, and a second row with the "
                                                       "track title when the station sends one." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio indicator style compact detailed" ) );

    transport->set_render_on( &g_cfg.m_radio.m_enabled );
    source->set_render_on( &g_cfg.m_radio.m_enabled );
    volume->set_render_on( &radio_stream_source_shown );
    follow->set_render_on( &radio_stream_source_shown );
    indicator->set_render_on( &g_cfg.m_radio.m_enabled );
    indicator_style->set_render_on( &radio_indicator_style_shown );

    gui::checkbox   *duck{ mixing->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Duck on voice chat" ), &g_cfg.m_radio.m_duck_on_voice ) };
    gui::checkbox   *duck_self{ mixing->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "When you talk" ), &g_cfg.m_radio.m_duck_on_self ) };
    gui::checkbox   *duck_others{ mixing->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "When others talk" ), &g_cfg.m_radio.m_duck_on_others ) };
    gui::slider_int *duck_level{ mixing->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Duck to" ), &g_cfg.m_radio.m_duck_level, 0, 100, ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::checkbox   *unfocused{ mixing->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Mute when unfocused" ), &g_cfg.m_radio.m_mute_unfocused ) };
    gui::slider_int *crossfade{ mixing->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Crossfade" ), &g_cfg.m_radio.m_crossfade, 0, 2000, ASTRAL_XORSTR_STATIC( "ms" ) ) };

    duck->set_tooltip( ASTRAL_XORSTR_STATIC( "Drops the radio down while someone is talking in "
                                            "voice chat." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio duck voice chat mixing" ) );

    duck_self->set_tooltip( ASTRAL_XORSTR_STATIC( "Counts your own transmission, so the radio "
                                                 "gets out of the way while you speak." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio duck voice self mic" ) );

    duck_others->set_tooltip( ASTRAL_XORSTR_STATIC( "Counts anyone else on the server, so the "
                                                   "radio gets out of the way to hear them." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio duck voice teammates others" ) );

    duck_level->set_tooltip( ASTRAL_XORSTR_STATIC( "Where the volume sits while a voice is "
                                                  "heard." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio duck level volume" ) );

    unfocused->set_tooltip( ASTRAL_XORSTR_STATIC( "Silences the stream while the game is not the "
                                                 "window you are using." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio mute focus alt tab window" ) );

    crossfade->set_tooltip( ASTRAL_XORSTR_STATIC( "Fades the station you left out and the new "
                                                 "one in over this long." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "radio crossfade fade station switch" ) );

    duck->set_render_on( &radio_stream_source_shown );
    duck_self->set_render_on( &radio_stream_duck_shown );
    duck_others->set_render_on( &radio_stream_duck_shown );
    duck_level->set_render_on( &radio_stream_duck_shown );
    unfocused->set_render_on( &radio_stream_source_shown );
    crossfade->set_render_on( &radio_stream_source_shown );

    s_radio_station_list = stations->add< gui::list_box >( radio_station_names( ), radio_station_count( ),
                                                           &g_cfg.m_radio.m_station );
    s_radio_station_list->set_render_on( &radio_stream_source_shown );
    s_radio_name_input = stations->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Station name" ) );
    s_radio_name_input->set_limit( 64 )->set_render_on( &radio_stream_source_shown );
    s_radio_url_input = stations->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Stream URL" ) );
    s_radio_url_input->set_limit( 1024 )->set_render_on( &radio_stream_source_shown );
    gui::button *add_station{ stations->add< gui::button >( ASTRAL_XORSTR_STATIC( "Add station" ), &add_radio_station ) };
    gui::button *remove_station{ stations->add< gui::button >( ASTRAL_XORSTR_STATIC( "Remove custom station" ), &remove_radio_station ) };
    add_station->set_render_on( &radio_stream_source_shown );
    remove_station->set_render_on( &radio_stream_source_shown );

    binds->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Next station" ), &g_cfg.m_radio.m_next_key );
    binds->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Previous station" ), &g_cfg.m_radio.m_previous_key );
    binds->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Volume up" ), &g_cfg.m_radio.m_louder_key );
    binds->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Volume down" ), &g_cfg.m_radio.m_quieter_key );
}

static gui::list_box *s_soundboard_list{ };

static bool soundboard_has_sounds( )
{
    return g_soundboard.count( ) != 0;
}

static bool soundboard_no_sounds( )
{
    return g_soundboard.count( ) == 0;
}

static const char *soundboard_empty_text( )
{
    return ASTRAL_XORSTR_STATIC( "Put .wav, .mp3 or .ogg files in the game's AstralMod/Sounds folder" );
}

static void soundboard_play( )
{
    g_soundboard.play( *g_soundboard.selected( ) );
}

static void soundboard_stop( )
{
    g_soundboard.stop( );
}

static void soundboard_refresh( )
{
    g_soundboard.refresh( );
    s_soundboard_list->set_items( g_soundboard.labels( ), g_soundboard.count( ) );
}

static void build_soundboard( gui::tab *misc )
{
    gui::sub_tab *tab{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Soundboard" ) ) };

    gui::group_box *sounds{ tab->add_group( ICON_MUSIC, ASTRAL_XORSTR_STATIC( "Sounds" ), gui::column_t::left ) };
    gui::group_box *playback{ tab->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Playback" ), gui::column_t::right ) };

    g_soundboard.refresh( );
    s_soundboard_list = sounds->add< gui::list_box >( g_soundboard.labels( ), g_soundboard.count( ), g_soundboard.selected( ) );

    gui::label *empty{ sounds->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ), &soundboard_empty_text ) };
    gui::label *actions{ sounds->add< gui::label >( ASTRAL_XORSTR_STATIC( "Sound" ), nullptr ) };

    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Play" ), &soundboard_play );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Stop" ), &soundboard_stop );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &soundboard_refresh );

    actions->set_tooltip( ASTRAL_XORSTR_STATIC( "Plays the selected file through your microphone. Refresh picks up "
                                               "files added to the folder while the game is open." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "soundboard voice spam mic microphone play sound" ) );

    s_soundboard_list->set_render_on( &soundboard_has_sounds );
    empty->set_render_on( &soundboard_no_sounds );

    gui::keybind    *key{ playback->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Play selected" ), &g_cfg.m_soundboard.m_key ) };
    gui::checkbox   *loop{ playback->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Loop" ), &g_cfg.m_soundboard.m_loop ) };
    gui::slider_int *volume{ playback->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Volume" ), &g_cfg.m_soundboard.m_volume, 0, 500, ASTRAL_XORSTR_STATIC( "%" ) ) };

    key->set_tooltip( ASTRAL_XORSTR_STATIC( "Plays the selected sound, or stops the one that is playing." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "soundboard voice spam key bind" ) );

    loop->set_tooltip( ASTRAL_XORSTR_STATIC( "Starts the sound over when it ends instead of letting go of the "
                                            "microphone." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "soundboard voice spam loop repeat" ) );

    volume->set_tooltip( ASTRAL_XORSTR_STATIC( "Scales the sound before Steam encodes it. Above 100% clips loud "
                                              "files." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "soundboard voice spam volume gain" ) );
}

static bool air_blast_aims( )
{
    return g_cfg.m_air_blast.m_enabled && g_cfg.m_air_blast.m_aim;
}

static void build_automations( gui::tab *misc )
{
    gui::sub_tab *automations{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Automations" ) ) };

    gui::group_box *detonate{ automations->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Detonate" ), gui::column_t::left ) };
    gui::group_box *airblast{ automations->add_group( ICON_FLAME, ASTRAL_XORSTR_STATIC( "Airblast" ), gui::column_t::left ) };
    gui::group_box *medic{ automations->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Medic" ), gui::column_t::right ) };
    gui::group_box *jump{ automations->add_group( ICON_CHEVRON_UP, ASTRAL_XORSTR_STATIC( "Rocket jump" ), gui::column_t::right ) };
    gui::group_box *mvm{ automations->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Mann vs. Machine" ), gui::column_t::left ) };
    gui::group_box *server{ automations->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Server" ), gui::column_t::left ) };

    static const char *const AUTO_JOIN_CLASSES[]{
        ASTRAL_XORSTR_STATIC( "Any" ), ASTRAL_XORSTR_STATIC( "Scout" ), ASTRAL_XORSTR_STATIC( "Sniper" ),
        ASTRAL_XORSTR_STATIC( "Soldier" ), ASTRAL_XORSTR_STATIC( "Demoman" ), ASTRAL_XORSTR_STATIC( "Medic" ),
        ASTRAL_XORSTR_STATIC( "Heavy" ), ASTRAL_XORSTR_STATIC( "Pyro" ), ASTRAL_XORSTR_STATIC( "Spy" ),
        ASTRAL_XORSTR_STATIC( "Engineer" )
    };
    gui::checkbox *auto_join{ server->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto join" ),
                                                             &g_cfg.m_misc.m_auto_join ) };
    gui::combo_box *auto_class{ server->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Preferred class" ), AUTO_JOIN_CLASSES, 10,
        &g_cfg.m_misc.m_auto_join_class ) };
    gui::checkbox *auto_vote{ server->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto vote" ),
                                                             &g_cfg.m_misc.m_auto_vote ) };
    gui::checkbox *protect_votes{ server->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Protect friends and party" ),
        &g_cfg.m_misc.m_auto_vote_protect_friends ) };
    gui::checkbox *marked_votes{ server->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Vote against marked players" ),
        &g_cfg.m_misc.m_auto_vote_marked_players ) };
    auto_join->set_tooltip( ASTRAL_XORSTR_STATIC( "Joins an available team and keeps the selected class." ) );
    auto_class->set_render_on( &g_cfg.m_misc.m_auto_join );
    auto_vote->set_tooltip( ASTRAL_XORSTR_STATIC( "Handles kick votes from local friend, party, and player-mark data." ) );
    protect_votes->set_render_on( &g_cfg.m_misc.m_auto_vote );
    marked_votes->set_render_on( &g_cfg.m_misc.m_auto_vote );

    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Instant respawn" ), &g_cfg.m_mvm.m_instant_respawn );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto ready-up" ), &g_cfg.m_mvm.m_auto_ready );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto Mann Up queue" ), &g_cfg.m_mvm.m_auto_mann_up_queue );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto Boot Camp queue" ), &g_cfg.m_mvm.m_auto_boot_camp_queue );
    mvm->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Mann Up tour" ), &g_cfg.m_mvm.m_mann_up_tour, 0, 32 );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Only uncompleted missions" ), &g_cfg.m_mvm.m_mann_up_uncompleted );
    mvm->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Boot Camp mission mask" ), reinterpret_cast< i32 * >( &g_cfg.m_mvm.m_boot_camp_missions ), 0, INT_MAX );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Buybot" ), &g_cfg.m_mvm.m_buybot );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Buybot auto class" ), &g_cfg.m_mvm.m_buybot_auto_class );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Rent required items" ), &g_cfg.m_mvm.m_rent_items );
    mvm->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Respec recovery" ), &g_cfg.m_mvm.m_respec_recovery );
    mvm->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Buybot class" ), &g_cfg.m_mvm.m_buybot_class, 1, 9 );
    mvm->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Buybot cash limit" ), &g_cfg.m_mvm.m_buybot_max_cash,
                                 0, 100000 );

    gui::checkbox   *det{ detonate->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_auto_det.m_enabled ) };
    gui::slider_int *det_radius{ detonate->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Blast radius" ), &g_cfg.m_auto_det.m_radius_scale, 40, 100,
        ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::slider_int *det_targets{ detonate->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Minimum targets" ), &g_cfg.m_auto_det.m_min_targets, 1, 6 ) };
    gui::checkbox   *det_self{ detonate->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Avoid self damage" ), &g_cfg.m_auto_det.m_avoid_self ) };
    gui::checkbox   *det_buildings{ detonate->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Target buildings" ), &g_cfg.m_auto_det.m_target_buildings ) };
    gui::checkbox   *det_dormant{ detonate->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Dormant targets" ), &g_cfg.m_auto_det.m_dormant ) };

    det->add_module< gui::keybind >( string_view{ }, &g_cfg.m_auto_det.m_key );

    det->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets off your stickies or Detonator flare the "
                                           "command an enemy walks into a blast that would "
                                           "actually reach them. With no key bound it runs "
                                           "constantly." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "auto detonate sticky stickybomb demoman pipe trap "
                                             "detonator flare pyro" ) );

    det_radius->set_tooltip( ASTRAL_XORSTR_STATIC( "Share of the real blast radius a target has "
                                                  "to be inside where the server will have them "
                                                  "when the command lands. Lower values keep a "
                                                  "margin for one that turns around in the "
                                                  "meantime." ) );

    det_targets->set_tooltip( ASTRAL_XORSTR_STATIC( "How many enemies the trap has to cover "
                                                   "before it goes off." ) );

    det_self->set_tooltip( ASTRAL_XORSTR_STATIC( "Holds the trigger while a blast would also "
                                                "catch you. Turn it off to keep sticky jumps." ) );

    det_buildings->set_tooltip( ASTRAL_XORSTR_STATIC( "Counts sentries, dispensers and "
                                                     "teleporters as targets too." ) );

    det_dormant->set_tooltip( ASTRAL_XORSTR_STATIC( "Also counts enemies and buildings you "
                                                   "cannot see, at the last position the "
                                                   "dormant ESP remembers for them." ) );

    det_radius->set_render_on( &g_cfg.m_auto_det.m_enabled );
    det_targets->set_render_on( &g_cfg.m_auto_det.m_enabled );
    det_self->set_render_on( &g_cfg.m_auto_det.m_enabled );
    det_buildings->set_render_on( &g_cfg.m_auto_det.m_enabled );
    det_dormant->set_render_on( &g_cfg.m_auto_det.m_enabled );

    static const char *const PUSH_MODES[]{
        ASTRAL_XORSTR_STATIC( "Off" ), ASTRAL_XORSTR_STATIC( "Melee" ), ASTRAL_XORSTR_STATIC( "All" )
    };

    gui::checkbox  *blast{ airblast->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_air_blast.m_enabled ) };
    gui::checkbox  *blast_aim{ airblast->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Aim at target" ), &g_cfg.m_air_blast.m_aim ) };
    gui::checkbox  *blast_redirect{ airblast->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Redirect at enemies" ), &g_cfg.m_air_blast.m_redirect ) };
    gui::checkbox  *blast_crits{ airblast->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Prefer crits" ), &g_cfg.m_air_blast.m_prefer_crits ) };
    gui::checkbox  *blast_extinguish{ airblast->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Extinguish teammates" ), &g_cfg.m_air_blast.m_extinguish ) };
    gui::combo_box *blast_push{ airblast->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Push enemies" ), PUSH_MODES, config::air_blast_t::PUSH_COUNT,
        &g_cfg.m_air_blast.m_push ) };

    blast->add_module< gui::keybind >( string_view{ }, &g_cfg.m_air_blast.m_key );

    blast->set_tooltip( ASTRAL_XORSTR_STATIC( "Airblasts the command an enemy projectile is "
                                             "inside the deflect sphere where the server will "
                                             "have it. With no key bound it runs constantly." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "auto airblast reflect deflect compression blast "
                                             "pyro flamethrower" ) );

    blast_aim->set_tooltip( ASTRAL_XORSTR_STATIC( "Turns the command toward a projectile or player "
                                                 "that is in reach but outside the blast, without "
                                                 "moving your view. Off, it only fires on what "
                                                 "your crosshair already covers." ) );

    blast_redirect->set_tooltip( ASTRAL_XORSTR_STATIC( "Aims the blast so a rocket, arrow or "
                                                      "flare flies at where an enemy will be "
                                                      "instead of where you are looking." ) );

    blast_crits->set_tooltip( ASTRAL_XORSTR_STATIC( "With several projectiles in reach, goes for a "
                                                   "critical one before the nearest." ) );

    blast_extinguish->set_tooltip( ASTRAL_XORSTR_STATIC( "Airblasts a burning teammate in reach "
                                                        "when no projectile needs the blast." ) );

    blast_push->set_tooltip( ASTRAL_XORSTR_STATIC( "Airblasts enemies in reach when no projectile "
                                                  "needs the blast. Melee only pushes the ones "
                                                  "holding a melee weapon." ) );

    blast_aim->set_render_on( &g_cfg.m_air_blast.m_enabled );
    blast_redirect->set_render_on( air_blast_aims );
    blast_crits->set_render_on( &g_cfg.m_air_blast.m_enabled );
    blast_extinguish->set_render_on( &g_cfg.m_air_blast.m_enabled );
    blast_push->set_render_on( &g_cfg.m_air_blast.m_enabled );

    static const char *const PREFER_MODES[]{
        ASTRAL_XORSTR_STATIC( "Heal all" ),
        ASTRAL_XORSTR_STATIC( "Prefer friends" ),
        ASTRAL_XORSTR_STATIC( "Only friends" ),
    };
    static const char *const IDLE_RESISTS[]{
        ASTRAL_XORSTR_STATIC( "Auto" ), ASTRAL_XORSTR_STATIC( "Bullet" ),
        ASTRAL_XORSTR_STATIC( "Blast" ), ASTRAL_XORSTR_STATIC( "Fire" )
    };
    static const char *const VACC_REACTIVITY[]{
        ASTRAL_XORSTR_STATIC( "Conservative" ), ASTRAL_XORSTR_STATIC( "Balanced" ),
        ASTRAL_XORSTR_STATIC( "Reactive" )
    };

    gui::checkbox  *auto_heal{ medic->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-heal" ), &g_cfg.m_medibot.m_enabled ) };
    gui::combo_box *prefer_mode{ medic->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Prefer mode" ), PREFER_MODES, config::MEDBOT_COUNT, &g_cfg.m_medibot.m_prefer_mode ) };
#if defined( ASTRAL_DEV )
    gui::checkbox *auto_uber{ medic->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-uber" ), &g_cfg.m_medibot.m_auto_uber ) };
    gui::checkbox *auto_kritz{ medic->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-kritz" ), &g_cfg.m_medibot.m_auto_kritz ) };
#endif // ASTRAL_DEV
    gui::checkbox  *vacc{ medic->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto-vaccinator" ), &g_cfg.m_auto_vacc.m_enabled ) };
    gui::checkbox  *auto_pop{ medic->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Pop vaccinator charge" ), &g_cfg.m_auto_vacc.m_auto_pop ) };
    gui::combo_box *idle{ medic->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Idle resistance" ), IDLE_RESISTS, 4, &g_cfg.m_auto_vacc.m_idle_resist ) };
    gui::combo_box *reactivity{ medic->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Reactivity" ), VACC_REACTIVITY, 3, &g_cfg.m_auto_vacc.m_reactivity ) };

    auto_heal->add_module< gui::keybind >( string_view{ }, &g_cfg.m_medibot.m_activate_key );
    auto_heal->set_tooltip( ASTRAL_XORSTR_STATIC( "Automatically heals teammates using your medigun when enabled. Selection scales dynamically from target classes, health and uber conditions." ) );
    prefer_mode->set_render_on( &g_cfg.m_medibot.m_enabled );

#if defined( ASTRAL_DEV )
    auto_uber->set_render_on( &g_cfg.m_medibot.m_enabled );
    auto_kritz->set_render_on( &g_cfg.m_medibot.m_enabled );
#endif // ASTRAL_DEV

    vacc->set_tooltip( ASTRAL_XORSTR_STATIC( "Swaps the vaccinator to the resistance that matches incoming damage." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "vaccinator vacc medic resist uber charge" ) );

    auto_pop->set_tooltip( ASTRAL_XORSTR_STATIC( "Allow the auto-vaccinator to automatically pop resistances for you if a sufficient threat is found." ) );

    idle->set_tooltip( ASTRAL_XORSTR_STATIC( "Which resistance to hold while nothing is threatening you." ) );
    reactivity->set_tooltip( ASTRAL_XORSTR_STATIC( "Controls how early a charge is popped against an incoming blow. "
                                                  "Conservative pops on the last moment that still lands in time, "
                                                  "Balanced once you can no longer walk out of it, "
                                                  "Reactive as soon as the hit is predicted." ) );
    auto_pop->set_render_on( &g_cfg.m_auto_vacc.m_enabled );
    idle->set_render_on( &g_cfg.m_auto_vacc.m_enabled );
    reactivity->set_render_on( &g_cfg.m_auto_vacc.m_enabled );
    static const char *const RJUMP_PRIORITIES[]{ ASTRAL_XORSTR_STATIC( "Height" ), ASTRAL_XORSTR_STATIC( "Impulse" ) };

    gui::checkbox  *rjump{ jump->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ),
                                                       &g_cfg.m_rjump.m_enabled ) };
    gui::combo_box *priority{ jump->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Jump priority" ), RJUMP_PRIORITIES,
                                                           config::RJUMP_PRIORITY_COUNT, &g_cfg.m_rjump.m_priority ) };
    gui::checkbox  *land_shot{ jump->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Landing shot" ), &g_cfg.m_rjump.m_land_shot ) };
    gui::checkbox  *pogo{ jump->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Pogo" ), &g_cfg.m_rjump.m_pogo ) };
    gui::checkbox  *pogo_crouch{ jump->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Crouch in air" ),
                                                             &g_cfg.m_rjump.m_pogo_crouch ) };

    rjump->add_module< gui::keybind >( string_view{ }, &g_cfg.m_rjump.m_key );

    rjump->set_tooltip( ASTRAL_XORSTR_STATIC( "Hold the bind on the ground with a rocket "
                                             "launcher out: aims into your feet, crouches, "
                                             "jumps and fires in one command." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "rocket jump blast ctap" ) );

    priority->set_tooltip( ASTRAL_XORSTR_STATIC( "Height puts the blast under you: the highest jump, "
                                                "your run speed carries on. Impulse puts it behind "
                                                "you while moving, where it sends you the farthest." ) );
    priority->set_render_on( &g_cfg.m_rjump.m_enabled );

    land_shot->set_tooltip( ASTRAL_XORSTR_STATIC( "Rocket Jumper only. While you fall, times a "
                                                 "rocket into the landing spot so the blast "
                                                 "arrives right before touchdown and cancels "
                                                 "the fall damage." ) );
    land_shot->set_render_on( &g_cfg.m_rjump.m_enabled );

    pogo->set_tooltip( ASTRAL_XORSTR_STATIC( "Rocket Jumper only. Keep holding the bind in the air "
                                            "and every landing gets a rocket that bounces you off "
                                            "the floor. Skipped when the fall could kill you." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "pogo bounce mge" ) );
    pogo->set_render_on( &g_cfg.m_rjump.m_enabled );

    pogo_crouch->set_tooltip( ASTRAL_XORSTR_STATIC( "Rocket Jumper bounce. Crouched: about 270 units "
                                                   "high, but every landing comes in just over the "
                                                   "safe fall speed and costs about 20 health. "
                                                   "Standing: about 110 units, no fall damage." ) );
    pogo_crouch->set_render_on( &pogo_shown );
}

static const char *checkpoint_save_text( ) { return ASTRAL_XORSTR_STATIC( "Save checkpoint" ); }
static const char *checkpoint_load_text( ) { return ASTRAL_XORSTR_STATIC( "Load checkpoint" ); }
static const char *route_add_text( ) { return ASTRAL_XORSTR_STATIC( "Add guide marker" ); }
static const char *route_delete_text( ) { return ASTRAL_XORSTR_STATIC( "Delete guide marker" ); }
static const char *route_clear_text( ) { return ASTRAL_XORSTR_STATIC( "Clear guide markers" ); }

static void build_movement( gui::tab *misc )
{
    gui::sub_tab *movement{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Movement" ) ) };

    gui::group_box *ground{ movement->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Movement" ),
                                                 gui::column_t::left ) };

    static const char *const AUTO_STRAFE_MODES[]{ ASTRAL_XORSTR_STATIC( "Viewangles" ),
                                                  ASTRAL_XORSTR_STATIC( "WASD" ) };

    gui::checkbox     *bhop{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Bunny hop" ),
                                                           &g_cfg.m_misc.m_bhop ) };
    gui::checkbox     *strafe{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto strafe" ),
                                                             &g_cfg.m_misc.m_auto_strafe ) };
    gui::combo_box    *mode{ ground->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Strafe towards" ),
                                                            AUTO_STRAFE_MODES, 2,
                                                            &g_cfg.m_misc.m_auto_strafe_mode ) };
    gui::slider_float *retrack{ ground->add< gui::slider_float >(
        ASTRAL_XORSTR_STATIC( "Retrack" ), &g_cfg.m_misc.m_auto_strafe_retrack, 10.0f, 100.0f, 0,
        ASTRAL_XORSTR_STATIC( "%" ) ) };

    bhop->set_tooltip( ASTRAL_XORSTR_STATIC( "Jumps again the tick you land, while jump is "
                                            "held. Releases the button for you on every "
                                            "command that could not jump anyway, so the "
                                            "landing jump never gets refused." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "bhop auto jump blast" ) );

    strafe->set_tooltip( ASTRAL_XORSTR_STATIC( "Steers the air strafe for you to keep speed." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "air strafe strafer" ) );

    mode->set_tooltip( ASTRAL_XORSTR_STATIC( "Whether the strafe follows where you look or the "
                                            "movement keys you hold." ) );
    mode->set_render_on( &g_cfg.m_misc.m_auto_strafe );
    retrack->set_render_on( &strafing_with_wasd );

    gui::checkbox *edge_jump{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Edge jump" ),
                                                            &g_cfg.m_misc.m_edge_jump ) };

    edge_jump->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_edge_jump_key );
    edge_jump->set_tooltip( ASTRAL_XORSTR_STATIC( "Jumps on the last command that still touches the "
                                                 "ground when you walk off a ledge, so you leave it "
                                                 "with a full jump instead of a drop. Without a key "
                                                 "it is always on." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "edge jump ledge auto jump" ) );

    gui::checkbox *jump_bug{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Jump bug" ),
                                                           &g_cfg.m_misc.m_jump_bug ) };

    jump_bug->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_jump_bug_key );
    jump_bug->set_tooltip( ASTRAL_XORSTR_STATIC( "During a fall that would hurt, uncrouches and jumps on the "
                                                "command where uncrouching touches the ground, so the landing "
                                                "never happens. That command only exists when the fall lines "
                                                "up with the ground within 2 units; when it does, this crouches "
                                                "one command before it, and otherwise it leaves your keys alone. "
                                                "Without a key it is always on." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "jump bug jumpbug fall damage crouch" ) );

    gui::checkbox *jump_bug_indicator{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Jump bug indicator" ),
                                                                     &g_cfg.m_misc.m_jump_bug_indicator ) };

    jump_bug_indicator->set_tooltip( ASTRAL_XORSTR_STATIC( "During a fall that would hurt, shows under the "
                                                          "crosshair whether it comes down through a jump bug "
                                                          "window, with your current movement keys. Green works, "
                                                          "red will not; it does not need Jump bug enabled." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "jump bug indicator jumpbug fall damage hud" ) );

    gui::checkbox *edge_bug{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Edge bug" ),
                                                           &g_cfg.m_misc.m_edge_bug ) };

    edge_bug->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_edge_bug_key );
    edge_bug->set_tooltip( ASTRAL_XORSTR_STATIC( "During a fall that would hurt, plans the last fraction of a "
                                                "second so the hull clips the top of an edge and slides off it in "
                                                "the same command, which zeroes the fall. Needs sideways speed and "
                                                "an edge near where you come down; to line it up it takes over "
                                                "your movement keys to brake or let go, and may crouch or "
                                                "uncrouch. Without a key it is always on." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "edge bug edgebug fall damage ledge" ) );

    static const char *const EDGE_BUG_STANCES[]{ ASTRAL_XORSTR_STATIC( "Automatic" ),
                                                  ASTRAL_XORSTR_STATIC( "Duck" ),
                                                  ASTRAL_XORSTR_STATIC( "Stand" ) };
    auto edge_bug_ticks{ ground->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Edge prediction" ),
                                                          &g_cfg.m_misc.m_edge_bug_predict_ticks, 4, 128,
                                                          ASTRAL_XORSTR_STATIC( "{} ticks" ) ) };
    auto edge_bug_stance{ ground->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Edge stance" ),
                                                          EDGE_BUG_STANCES, 3, &g_cfg.m_misc.m_edge_bug_stance ) };
    auto edge_bug_steer{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Edge steering" ),
                                                        &g_cfg.m_misc.m_edge_bug_steer ) };
    auto edge_bug_indicator{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Edge plan indicator" ),
                                                            &g_cfg.m_misc.m_edge_bug_indicator ) };
    edge_bug_ticks->set_render_on( &g_cfg.m_misc.m_edge_bug );
    edge_bug_stance->set_render_on( &g_cfg.m_misc.m_edge_bug );
    edge_bug_steer->set_render_on( &g_cfg.m_misc.m_edge_bug );
    edge_bug_indicator->set_render_on( &g_cfg.m_misc.m_edge_bug );

    auto edge_skip{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Edge skip" ),
                                                   &g_cfg.m_misc.m_edge_skip ) };
    edge_skip->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_edge_skip_key );
    edge_skip->set_tooltip( ASTRAL_XORSTR_STATIC( "Searches for a crouch and braking plan that clips a ledge instead of completing the landing." ) );

    gui::checkbox *fan_jump{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Force-A-Nature jump" ),
                                                           &g_cfg.m_misc.m_fan_jump ) };

    fan_jump->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_fan_jump_key );
    fan_jump->set_tooltip( ASTRAL_XORSTR_STATIC( "While the key is held, jumps and fires the "
                                                "Force-A-Nature at the ground along your velocity, "
                                                "with the pitch that turns the knockback into the "
                                                "most height for your speed. Already airborne, it "
                                                "only fires." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fan jump force a nature scattergun high jump" ) );

    gui::checkbox *fast_stop{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Fast stop" ),
                                                            &g_cfg.m_misc.m_fast_stop ) };
    gui::checkbox *fast_accel{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Fast acceleration" ),
                                                              &g_cfg.m_misc.m_fast_accel ) };

    fast_stop->set_tooltip( ASTRAL_XORSTR_STATIC( "Once you let go of the movement keys, pushes "
                                                 "against your own velocity to kill it in one or "
                                                 "two commands instead of sliding out the "
                                                 "friction. The push scales down at low speed so "
                                                 "you stop on the spot instead of walking back." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fast stop auto stop brake accuracy" ) );
    fast_accel->set_tooltip( ASTRAL_XORSTR_STATIC( "Uses alternating rolled movement commands to reach ground speed faster while movement keys are held." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fast acceleration accel movement" ) );

    auto air_stop{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Air stop" ), &g_cfg.m_misc.m_air_stop ) };
    air_stop->set_tooltip( ASTRAL_XORSTR_STATIC( "Extends fast stop into the air to counter current horizontal momentum." ) );
    air_stop->set_render_on( &g_cfg.m_misc.m_fast_stop );

    gui::checkbox   *taunt_spin{ ground->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Taunt spin" ),
                                                               &g_cfg.m_misc.m_taunt_spin ) };
    gui::slider_int *taunt_spin_speed{ ground->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Spin speed" ),
                                                                       &g_cfg.m_misc.m_taunt_spin_speed, -60,
                                                                       60 ) };

    taunt_spin->set_tooltip( ASTRAL_XORSTR_STATIC( "Spins your model while you taunt. Only taunts you can "
                                                  "move in turn the model for everyone else, like the "
                                                  "Conga or the Kazotsky Kick." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "taunt spin rotate conga kazotsky" ) );

    taunt_spin_speed->set_tooltip( ASTRAL_XORSTR_STATIC( "Degrees turned per command. Negative spins "
                                                        "the other way." ) )
        ->set_render_on( &g_cfg.m_misc.m_taunt_spin );

    gui::group_box *lab{ movement->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Movement Laboratory" ),
                                              gui::column_t::right ) };

    auto nulls{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Null movement" ), &g_cfg.m_misc.m_null_movement ) };
    nulls->set_tooltip( ASTRAL_XORSTR_STATIC( "Cancels opposing movement keys so W+S and A+D cannot fight each other." ) );

    auto optimizer{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Strafe optimizer" ), &g_cfg.m_misc.m_strafe_optimizer ) };
    auto optimizer_gain{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Optimizer gain" ), &g_cfg.m_misc.m_strafe_optimizer_gain,
                                                        0.0f, 100.0f, 0, ASTRAL_XORSTR_STATIC( "%" ) ) };
    auto optimizer_speed{ lab->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Minimum speed" ), &g_cfg.m_misc.m_strafe_optimizer_min_speed,
                                                       0, 520, ASTRAL_XORSTR_STATIC( "u/s" ) ) };
    optimizer->set_tooltip( ASTRAL_XORSTR_STATIC( "Corrects manual air movement toward the acceleration-efficient strafe angle." ) );
    optimizer_gain->set_render_on( &g_cfg.m_misc.m_strafe_optimizer );
    optimizer_speed->set_render_on( &g_cfg.m_misc.m_strafe_optimizer );

    auto wall_avoid{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Avoid walls" ), &g_cfg.m_misc.m_strafe_avoid_walls ) };
    auto wall_distance{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Wall lookahead" ), &g_cfg.m_misc.m_strafe_avoid_distance,
                                                       8.0f, 200.0f, 0, ASTRAL_XORSTR_STATIC( "u" ) ) };
    wall_avoid->set_tooltip( ASTRAL_XORSTR_STATIC( "Looks ahead while airborne and redirects the strafe along an approaching wall." ) );
    wall_distance->set_render_on( &g_cfg.m_misc.m_strafe_avoid_walls );

    auto half_sideways{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Half-sideways" ), &g_cfg.m_misc.m_half_sideways ) };
    half_sideways->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_half_sideways_key );
    half_sideways->set_tooltip( ASTRAL_XORSTR_STATIC( "Constrains manual air input to a diagonal HSW pair while preserving the chosen side." ) );

    auto double_jump_strafe{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Double-jump strafe" ), &g_cfg.m_misc.m_double_jump_strafe ) };
    double_jump_strafe->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_double_jump_strafe_key );
    double_jump_strafe->set_tooltip( ASTRAL_XORSTR_STATIC( "After Scout's air dash, steers along the carried horizontal velocity to retain speed." ) );

    auto delayed_hop{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Delayed hop" ), &g_cfg.m_misc.m_delayed_hop ) };
    delayed_hop->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_delayed_hop_key );
    auto delayed_hop_ticks{ lab->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Hop delay" ), &g_cfg.m_misc.m_delayed_hop_ticks,
                                                         1, 32, ASTRAL_XORSTR_STATIC( "{} ticks" ) ) };
    delayed_hop->set_tooltip( ASTRAL_XORSTR_STATIC( "Waits the configured number of grounded commands before allowing a held jump." ) );
    delayed_hop_ticks->set_render_on( &g_cfg.m_misc.m_delayed_hop );

    auto long_jump{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Long jump" ), &g_cfg.m_misc.m_long_jump ) };
    long_jump->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_long_jump_key );
    long_jump->set_tooltip( ASTRAL_XORSTR_STATIC( "On a walk-off, injects the jump and two-command duck sequence used for a long jump." ) );
    auto long_adaptive{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Adaptive long jump" ), &g_cfg.m_misc.m_long_jump_adaptive ) };
    auto long_release{ lab->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Maximum duck time" ), &g_cfg.m_misc.m_long_jump_release_ticks,
                                                    1, 64, ASTRAL_XORSTR_STATIC( "{} ticks" ) ) };
    long_adaptive->set_tooltip( ASTRAL_XORSTR_STATIC( "Keeps the long-jump crouch through ascent, then releases on descent or at the safety limit." ) );
    long_adaptive->set_render_on( &g_cfg.m_misc.m_long_jump );
    long_release->set_render_on( &g_cfg.m_misc.m_long_jump_adaptive );

    auto mini_jump{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Mini jump" ), &g_cfg.m_misc.m_mini_jump ) };
    mini_jump->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_mini_jump_key );
    mini_jump->set_tooltip( ASTRAL_XORSTR_STATIC( "Pulses duck only on a grounded jump command to produce a shorter jump." ) );
    auto mini_hold{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Hold mini-jump duck" ), &g_cfg.m_misc.m_mini_jump_hold_duck ) };
    auto mini_edge{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Mini jump at edge" ), &g_cfg.m_misc.m_mini_jump_edge ) };
    mini_hold->set_render_on( &g_cfg.m_misc.m_mini_jump );
    mini_edge->set_render_on( &g_cfg.m_misc.m_mini_jump );

    auto auto_crouch{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Landing crouch" ), &g_cfg.m_misc.m_auto_crouch ) };
    auto_crouch->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_auto_crouch_key );
    auto auto_crouch_ticks{ lab->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Landing lookahead" ), &g_cfg.m_misc.m_auto_crouch_ticks,
                                                         1, 32, ASTRAL_XORSTR_STATIC( "t" ) ) };
    auto_crouch->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts a landing while falling and crouches within the configured window." ) );
    auto_crouch_ticks->set_render_on( &g_cfg.m_misc.m_auto_crouch );

    auto jump_stats{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Jump stats" ), &g_cfg.m_misc.m_jump_stats ) };
    jump_stats->set_tooltip( ASTRAL_XORSTR_STATIC( "Reports distance, starting and maximum speed, height and strafe count after each jump." ) );

    auto blockbot{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Blockbot" ), &g_cfg.m_misc.m_blockbot ) };
    blockbot->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_blockbot_key );
    auto blockbot_teammates{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Block teammates" ),
                                                        &g_cfg.m_misc.m_blockbot_teammates ) };
    blockbot->set_tooltip( ASTRAL_XORSTR_STATIC( "Steers into the nearest player's path and matches their motion while standing above them." ) );
    blockbot_teammates->set_render_on( &g_cfg.m_misc.m_blockbot );

    static const char *const AUTO_BOUNCE_MODES[]{ ASTRAL_XORSTR_STATIC( "Drop in" ),
                                                   ASTRAL_XORSTR_STATIC( "Keep speed" ),
                                                   ASTRAL_XORSTR_STATIC( "Stop" ) };
    auto auto_bounce{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Auto bounce" ), &g_cfg.m_misc.m_auto_bounce ) };
    auto_bounce->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_auto_bounce_key );
    auto bounce_mode{ lab->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Bounce approach" ), AUTO_BOUNCE_MODES, 3,
                                                  &g_cfg.m_misc.m_auto_bounce_mode ) };
    auto bounce_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Bounce reach" ), &g_cfg.m_misc.m_auto_bounce_reach,
                                                       16.0f, 512.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    auto bounce_gain{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Minimum bounce gain" ), &g_cfg.m_misc.m_auto_bounce_min_gain,
                                                      0.0f, 500.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f} u/s" ) ) };
    auto bounce_jump{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Jump on bounce" ), &g_cfg.m_misc.m_auto_bounce_jump ) };
    auto bounce_crouch{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Crouch on bounce" ), &g_cfg.m_misc.m_auto_bounce_crouch ) };
    auto_bounce->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts the first descending ramp contact and steers the approach when the clipped exit meets the requested gain." ) );
    bounce_mode->set_render_on( &g_cfg.m_misc.m_auto_bounce );
    bounce_reach->set_render_on( &g_cfg.m_misc.m_auto_bounce );
    bounce_gain->set_render_on( &g_cfg.m_misc.m_auto_bounce );
    bounce_jump->set_render_on( &g_cfg.m_misc.m_auto_bounce );
    bounce_crouch->set_render_on( &g_cfg.m_misc.m_auto_bounce );

    auto goomba{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Goomba assist" ), &g_cfg.m_misc.m_goomba ) };
    goomba->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_goomba_key );
    auto goomba_teammates{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Target teammates" ),
                                                      &g_cfg.m_misc.m_goomba_teammates ) };
    auto goomba_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Goomba reach" ), &g_cfg.m_misc.m_goomba_reach,
                                                      32.0f, 400.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    auto goomba_lead{ lab->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Goomba timing" ), &g_cfg.m_misc.m_goomba_lead_ticks,
                                                   -8, 16, ASTRAL_XORSTR_STATIC( "{} ticks" ) ) };
    goomba->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts when your fall reaches a player below and steers toward their extrapolated position." ) );
    goomba_reach->set_render_on( &g_cfg.m_misc.m_goomba );
    goomba_lead->set_render_on( &g_cfg.m_misc.m_goomba );
    goomba_teammates->set_render_on( &g_cfg.m_misc.m_goomba );

    auto wall_climb{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Wall climb" ), &g_cfg.m_misc.m_wall_climb ) };
    wall_climb->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_wall_climb_key );
    auto wall_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Wall reach" ), &g_cfg.m_misc.m_wall_climb_reach,
                                                    2.0f, 48.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    auto wall_duck{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Climb crouch" ), &g_cfg.m_misc.m_wall_climb_duck ) };
    auto wall_slow{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Prevent climb slowdown" ), &g_cfg.m_misc.m_wall_climb_prevent_slow ) };
    auto wall_seam{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Steer to seams" ), &g_cfg.m_misc.m_wall_climb_seam_steer ) };
    auto wall_scale{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Seam steering" ), &g_cfg.m_misc.m_wall_climb_steer_scale,
                                                    0.25f, 2.0f, 2, ASTRAL_XORSTR_STATIC( "{:.2f}x" ) ) };
    wall_climb->set_tooltip( ASTRAL_XORSTR_STATIC( "Scans nearby steep faces and keeps the air wish pinned into the closest wall while preserving along-wall speed." ) );
    wall_reach->set_render_on( &g_cfg.m_misc.m_wall_climb );
    wall_duck->set_render_on( &g_cfg.m_misc.m_wall_climb );
    wall_slow->set_render_on( &g_cfg.m_misc.m_wall_climb );
    wall_seam->set_render_on( &g_cfg.m_misc.m_wall_climb );
    wall_scale->set_render_on( &g_cfg.m_misc.m_wall_climb_seam_steer );

    static const char *const PIXEL_SURF_STRAFE[]{ ASTRAL_XORSTR_STATIC( "Off" ),
                                                  ASTRAL_XORSTR_STATIC( "At contact" ),
                                                  ASTRAL_XORSTR_STATIC( "During ride" ) };
    auto pixel_surf{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Pixel surf" ), &g_cfg.m_misc.m_pixel_surf ) };
    pixel_surf->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_pixel_surf_key );
    auto pixel_ticks{ lab->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Surf prediction" ),
                                                   &g_cfg.m_misc.m_pixel_surf_predict_ticks, 1, 32,
                                                   ASTRAL_XORSTR_STATIC( "{} ticks" ) ) };
    auto pixel_strafe{ lab->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Surf wall strafe" ),
                                                   PIXEL_SURF_STRAFE, 3,
                                                   &g_cfg.m_misc.m_pixel_surf_wall_strafe ) };
    auto pixel_park{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Park at surf end" ),
                                                &g_cfg.m_misc.m_pixel_surf_park ) };
    pixel_surf->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts a steep slowing contact, pins into its face, and carries the ride across short facet gaps." ) );
    pixel_ticks->set_render_on( &g_cfg.m_misc.m_pixel_surf );
    pixel_strafe->set_render_on( &g_cfg.m_misc.m_pixel_surf );
    pixel_park->set_render_on( &g_cfg.m_misc.m_pixel_surf );

    static const char *const TEXTURE_STANCES[]{ ASTRAL_XORSTR_STATIC( "Automatic" ),
                                                ASTRAL_XORSTR_STATIC( "Duck" ),
                                                ASTRAL_XORSTR_STATIC( "Stand" ) };
    auto texture_bug{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Texture bug" ), &g_cfg.m_misc.m_texture_bug ) };
    texture_bug->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_texture_bug_key );
    auto texture_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Texture reach" ), &g_cfg.m_misc.m_texture_bug_reach,
                                                       1.0f, 48.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    auto texture_stance{ lab->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Texture stance" ), TEXTURE_STANCES, 3,
                                                     &g_cfg.m_misc.m_texture_bug_stance ) };
    auto texture_duck{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Automatic texture duck" ),
                                                  &g_cfg.m_misc.m_texture_bug_auto_duck ) };
    auto texture_strafe{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Texture wall strafe" ),
                                                    &g_cfg.m_misc.m_texture_bug_wall_strafe ) };
    texture_bug->set_tooltip( ASTRAL_XORSTR_STATIC( "Compares upper and lower face samples to find wall seams, then pins the selected hull stance into the seam." ) );
    texture_reach->set_render_on( &g_cfg.m_misc.m_texture_bug );
    texture_stance->set_render_on( &g_cfg.m_misc.m_texture_bug );
    texture_duck->set_render_on( &g_cfg.m_misc.m_texture_bug );
    texture_strafe->set_render_on( &g_cfg.m_misc.m_texture_bug );

    auto pixel_jump{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Pixel jump" ), &g_cfg.m_misc.m_pixel_jump ) };
    pixel_jump->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_pixel_jump_key );
    auto pixel_jump_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Pixel lip reach" ), &g_cfg.m_misc.m_pixel_jump_reach,
                                                          0.5f, 12.0f, 1, ASTRAL_XORSTR_STATIC( "{:.1f}u" ) ) };
    auto pixel_jump_crouch{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Crouched pixel jump" ),
                                                       &g_cfg.m_misc.m_pixel_jump_crouch ) };
    auto pixel_jump_hop{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Chain pixel jumps" ),
                                                    &g_cfg.m_misc.m_pixel_jump_hop ) };
    pixel_jump->set_tooltip( ASTRAL_XORSTR_STATIC( "Probes just beyond the grounded hull for a support transition and times the jump while moving toward that lip." ) );
    pixel_jump_reach->set_render_on( &g_cfg.m_misc.m_pixel_jump );
    pixel_jump_crouch->set_render_on( &g_cfg.m_misc.m_pixel_jump );
    pixel_jump_hop->set_render_on( &g_cfg.m_misc.m_pixel_jump );

    auto movement_debug{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Movement diagnostics" ),
                                                     &g_cfg.m_misc.m_movement_debug ) };
    movement_debug->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows which movement planner owns the current command and draws its selected contact point and surface normal." ) );

    auto air_stuck{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Air-stuck assist" ), &g_cfg.m_misc.m_air_stuck ) };
    air_stuck->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_air_stuck_key );
    auto air_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Air-stuck reach" ), &g_cfg.m_misc.m_air_stuck_reach,
                                                   1.0f, 32.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    auto air_crouch{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Crouched air-stuck" ), &g_cfg.m_misc.m_air_stuck_crouch ) };
    air_stuck->set_tooltip( ASTRAL_XORSTR_STATIC( "Finds a nearby face or seam, presses into it, and counters along-wall drift to retain the pin." ) );
    air_reach->set_render_on( &g_cfg.m_misc.m_air_stuck );
    air_crouch->set_render_on( &g_cfg.m_misc.m_air_stuck );

    auto finder{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Movement point finder" ), &g_cfg.m_misc.m_movement_finder ) };
    auto finder_radius{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Point spacing" ), &g_cfg.m_misc.m_movement_finder_radius,
                                                       4.0f, 128.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    auto finder_distance{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Point draw distance" ),
                                                         &g_cfg.m_misc.m_movement_finder_draw_distance,
                                                         100.0f, 5000.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    finder->set_tooltip( ASTRAL_XORSTR_STATIC( "Accumulates nearby surf faces, texture seams, and unsupported lips as color-coded per-map points." ) );
    finder_radius->set_render_on( &g_cfg.m_misc.m_movement_finder );
    finder_distance->set_render_on( &g_cfg.m_misc.m_movement_finder );

    auto checkpoint_save{ lab->add< gui::label >( ASTRAL_XORSTR_STATIC( "Save checkpoint" ), &checkpoint_save_text ) };
    checkpoint_save->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_checkpoint_save_key );
    auto checkpoint_load{ lab->add< gui::label >( ASTRAL_XORSTR_STATIC( "Load checkpoint" ), &checkpoint_load_text ) };
    checkpoint_load->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_checkpoint_load_key );
    checkpoint_load->set_tooltip( ASTRAL_XORSTR_STATIC( "Restores the saved position and view only on a local/listen server." ) );

    auto route_add{ lab->add< gui::label >( ASTRAL_XORSTR_STATIC( "Add guide marker" ), &route_add_text ) };
    route_add->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_route_add_key );
    auto route_delete{ lab->add< gui::label >( ASTRAL_XORSTR_STATIC( "Delete guide marker" ), &route_delete_text ) };
    route_delete->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_route_delete_key );
    auto route_clear{ lab->add< gui::label >( ASTRAL_XORSTR_STATIC( "Clear guide markers" ), &route_clear_text ) };
    route_clear->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_route_clear_key );
    auto route_follow{ lab->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Follow guide markers" ), &g_cfg.m_misc.m_route_follow ) };
    route_follow->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_route_follow_key );
    auto route_reach{ lab->add< gui::slider_float >( ASTRAL_XORSTR_STATIC( "Guide marker reach" ), &g_cfg.m_misc.m_route_reach,
                                                     8.0f, 128.0f, 0, ASTRAL_XORSTR_STATIC( "{:.0f}u" ) ) };
    route_add->set_tooltip( ASTRAL_XORSTR_STATIC( "Places an ordered per-map guide marker on the surface beneath the crosshair. Recorded routes remain in Movement recorder." ) );
    route_follow->set_tooltip( ASTRAL_XORSTR_STATIC( "Guides movement through the ordered markers; geometry catches retain higher command priority." ) );
    route_reach->set_render_on( &g_cfg.m_misc.m_route_follow );
}

inline constexpr usize RECORDER_TRANSPORT_COUNT{ 3 };
inline constexpr i32   RECORDER_DOT_SIZE{ 7 };

static recorder_status_t s_recorder_status{ };
static recorder_entry_t  s_recorder_entries[ RECORDER_ENTRY_CAPACITY ]{ };
static const char       *s_recorder_labels[ RECORDER_ENTRY_CAPACITY ]{ };
static usize             s_recorder_count{ };
static usize             s_recorder_selected{ };
static u32               s_recorder_serial{ };
static gui::list_box    *s_recorder_list{ };
static gui::text_input  *s_recorder_name{ };

static fixed_string< RECORDER_NAME_LENGTH > s_recorder_announced{ };

static char config_name_filter( char value );

static const recorder_entry_t *recorder_entry( )
{
    return s_recorder_selected < s_recorder_count ? &s_recorder_entries[ s_recorder_selected ] : nullptr;
}

static string_view recorder_selected_name( )
{
    const recorder_entry_t *entry{ recorder_entry( ) };
    return entry ? entry->m_name.view( ) : string_view{ };
}

static void sync_recorder( )
{
    s_recorder_status = g_recorder.status( );

    fixed_string< RECORDER_NAME_LENGTH > previous{ };
    previous.assign( recorder_selected_name( ) );

    usize focus{ };
    if ( g_recorder.catalog( s_recorder_serial, s_recorder_entries, s_recorder_count, focus ) )
    {
        usize selected{ focus };

        for ( usize index{ }; index < s_recorder_count; ++index )
        {
            s_recorder_labels[ index ] = s_recorder_entries[ index ].m_name.c_str( );

            if ( selected == RECORDER_NO_ENTRY && s_recorder_entries[ index ].m_name.view( ) == previous.view( ) )
            {
                selected = index;
            }
        }

        s_recorder_selected = selected < s_recorder_count ? selected : 0;
        s_recorder_list->set_items( s_recorder_labels, s_recorder_count );
    }

    const string_view selected{ recorder_selected_name( ) };
    if ( selected != s_recorder_announced.view( ) )
    {
        s_recorder_announced.assign( selected );
        g_recorder.request( recorder_request_t::select, selected );
    }
}

static void recorder_panel_text( const recorder_status_t &status, char ( &name )[ 48 ], char ( &line )[ 64 ] )
{
    switch ( status.m_phase )
    {
        case recorder_phase_t::waiting:
            str::copy( name, sizeof( name ), ASTRAL_XORSTR( "Recording" ).get( ) );
            str::copy( line, sizeof( line ), ASTRAL_XORSTR( "Starts with your first movement input" ).get( ) );
            return;

        case recorder_phase_t::recording:
            str::copy( name, sizeof( name ), ASTRAL_XORSTR( "Recording" ).get( ) );
            str::format_to( line, sizeof( line ), ASTRAL_XORSTR( "{:.2f} s, {} commands" ).get( ),
                            static_cast< f32 >( status.m_frames ) * status.m_interval, status.m_frames );
            return;

        case recorder_phase_t::armed:
        {
            char wait[ 32 ]{ };
            recorder_wait_text( status.m_wait, status.m_start_distance, status.m_start_height, wait );

            str::copy( name, sizeof( name ), status.m_route.c_str( ) );
            str::format_to( line, sizeof( line ), ASTRAL_XORSTR( "Armed, {}" ).get( ), wait );
            return;
        }

        case recorder_phase_t::aligning:
            str::copy( name, sizeof( name ), status.m_route.c_str( ) );
            str::copy( line, sizeof( line ), ASTRAL_XORSTR( "Aligning on the start" ).get( ) );
            return;

        case recorder_phase_t::replaying:
            str::copy( name, sizeof( name ), status.m_route.c_str( ) );
            str::format_to( line, sizeof( line ), ASTRAL_XORSTR( "Replaying {:.2f} / {:.2f} s" ).get( ),
                            static_cast< f32 >( status.m_frame ) * status.m_interval,
                            static_cast< f32 >( status.m_frames ) * status.m_interval );
            return;

        default: break;
    }

    str::copy( name, sizeof( name ), status.m_map.empty( ) ? ASTRAL_XORSTR( "Not in a map" ).get( ) : status.m_map.c_str( ) );

    if ( status.m_has_take )
    {
        str::copy( line, sizeof( line ), ASTRAL_XORSTR( "Take ready, name it and save it" ).get( ) );
    } else if ( !status.m_routes )
    {
        str::copy( line, sizeof( line ), ASTRAL_XORSTR( "No recordings on this map yet" ).get( ) );
    } else
    {
        str::format_to( line, sizeof( line ), ASTRAL_XORSTR( "{} {} on this map" ).get( ), status.m_routes,
                        status.m_routes == 1 ? ASTRAL_XORSTR_STATIC( "recording" ) : ASTRAL_XORSTR_STATIC( "recordings" ) );
    }
}

class recorder_panel : public gui::baseobject
{
public:
    i32 measure( ) const override
    {
        return g_gui.scaled( RADIO_PANEL_HEIGHT );
    }

    void think( ) override
    {
        sync_recorder( );
        this->m_wave = ( this->m_wave + gui::animation_delta( this->m_wave_tick ) ) % RADIO_GLOW_PERIOD;
    }

    void draw( ) const override
    {
        const recorder_status_t &status{ s_recorder_status };
        const bool               recording{ status.m_phase == recorder_phase_t::waiting ||
                                            status.m_phase == recorder_phase_t::recording };

        g_render.box( this->m_bounds, g_gui.color( CONTROL_BACKGROUND ), true );

        const i32 inner_left{ this->m_bounds.left + 1 };
        const i32 inner_right{ this->m_bounds.right - 1 };
        const i32 inner_bottom{ this->m_bounds.bottom - 1 };
        const i32 track_top{ inner_bottom - g_gui.scaled( RADIO_PANEL_TRACK ) };
        const i32 filled{ status.m_phase == recorder_phase_t::replaying && status.m_frames
                              ? ( inner_right - inner_left ) * status.m_frame / status.m_frames
                              : 0 };

        g_render.box( rect_t{ inner_left, track_top, inner_right, inner_bottom },
                      g_gui.color( CONTROL_BACKGROUND_HOVER ), true );
        g_render.box( rect_t{ inner_left, track_top, inner_left + filled, inner_bottom },
                      g_gui.color( g_theme.m_accent ), true );

        if ( recording )
        {
            draw_panel_glow( inner_left, inner_right, inner_bottom, g_theme.m_accent, this->m_wave );
        }

        const i32 left{ this->m_bounds.left + g_gui.scaled( RADIO_PANEL_PAD ) };
        const i32 top{ this->m_bounds.top + g_gui.scaled( RADIO_PANEL_PAD ) };
        const i32 text_left{ this->m_bounds.left + g_gui.scaled( RADIO_PANEL_GUTTER ) };
        const i32 icon{ g_gui.scaled( MENU_ICON_SIZE ) };

        switch ( status.m_phase )
        {
            case recorder_phase_t::waiting:
            case recorder_phase_t::recording:
            {
                const i32 dot{ g_gui.scaled( RECORDER_DOT_SIZE ) };
                const i32 dot_left{ left + ( icon - dot ) / 2 };
                const i32 dot_top{ top + ( icon - dot ) / 2 };

                gui::fill_rounded_box( rect_t{ dot_left, dot_top, dot_left + dot, dot_top + dot }, dot / 2,
                                       g_gui.color( g_theme.m_accent ) );
                break;
            }

            case recorder_phase_t::armed:
            case recorder_phase_t::aligning:
                gui::draw_icon_at( ICON_CHEVRON_UP, point_t{ left, top }, g_gui.color( STATUS_WARNING ), icon );
                break;

            case recorder_phase_t::replaying:
                gui::draw_icon_at( ICON_ZAP, point_t{ left, top }, g_gui.color( g_theme.m_accent ), icon );
                break;

            default:
                gui::draw_icon_at( ICON_RULER, point_t{ left, top }, g_gui.color( MENU_TEXT_FAINT ), icon );
                break;
        }

        char name[ 48 ]{ };
        char line[ 64 ]{ };
        recorder_panel_text( status, name, line );

        g_render.text( MENU_FONT( ),
                       point_t{ text_left, g_gui.text_center_y( top, g_gui.scaled( RADIO_PANEL_NAME_ROW ) ) },
                       g_gui.color( MENU_TEXT ), name );

        g_render.text( MENU_FONT_TINY( ),
                       point_t{ text_left,
                                g_gui.text_center_y( top + g_gui.scaled( RADIO_PANEL_NAME_ROW ),
                                                     g_gui.scaled( RADIO_PANEL_STATUS_ROW ) ) },
                       g_gui.color( MENU_TEXT_DIM ), line );

        g_render.box( rect_t{ this->m_bounds.left, this->m_bounds.top, this->m_bounds.right - 1,
                              this->m_bounds.bottom - 1 },
                      g_gui.color( CONTROL_BORDER ) );
    }

private:
    u32 m_wave{ };
    u64 m_wave_tick{ };
};

class recorder_transport : public gui::baseobject
{
public:
    i32 measure( ) const override
    {
        return g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) +
               g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) + g_gui.scaled( WIDGET_SEGMENT_GAP );
    }

    void think( ) override
    {
        const u32 delta{ gui::animation_delta( this->m_last_tick ) };

        this->m_hovered = false;

        for ( usize index{ }; index < RECORDER_TRANSPORT_COUNT; ++index )
        {
            const bool hovered{ g_gui.hit_test( this->segment_bounds( index ) ) };
            if ( hovered )
            {
                set_cursor( );
                this->m_hovered = true;
            }

            gui::step_mix( this->m_segments[ index ].m_hover_mix, hovered, delta, WIDGET_HOVER_DURATION );
            gui::step_mix( this->m_segments[ index ].m_active_mix, running( index ), delta, WIDGET_TOGGLE_DURATION );

            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                press( index );
            }
        }
    }

    void draw( ) const override
    {
        const i32 bar{ g_gui.scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < RECORDER_TRANSPORT_COUNT; ++index )
        {
            const rect_t segment{ this->segment_bounds( index ) };
            const u8     hover{ gui::eased_mix( this->m_segments[ index ].m_hover_mix ) };
            const u8     active{ gui::eased_mix( this->m_segments[ index ].m_active_mix ) };

            color_t background{ gui::mix_color( CHIP_IDLE_BACKGROUND, CHIP_HOVER_BACKGROUND, hover ) };
            background = gui::mix_color( background, g_theme.m_row_selected_background, active );

            color_t label_color{ gui::mix_color( CHIP_IDLE_TEXT, CHIP_ACTIVE_TEXT, hover ) };
            label_color = gui::mix_color( label_color, LIST_TEXT_ACTIVE, active );

            g_render.box( segment, g_gui.color( background ), true );

            g_render.text( MENU_FONT_TINY( ),
                           point_t{ ( segment.left + segment.right ) / 2,
                                    g_gui.tiny_center_y( segment.top, segment.bottom - segment.top ) },
                           g_gui.color( label_color ), label( index ), false, render::text_align::center );

            if ( active )
            {
                g_render.box( rect_t{ segment.left, segment.bottom - bar, segment.right, segment.bottom },
                              g_gui.color( gui::with_alpha( g_theme.m_accent, active ) ), true );
            }

            const bool last{ index + 1 == RECORDER_TRANSPORT_COUNT };

            g_render.box( rect_t{ segment.left, segment.top, last ? segment.right - 1 : segment.right,
                                  segment.bottom - 1 },
                          g_gui.color( CONTROL_BORDER ) );
        }
    }

private:
    rect_t segment_bounds( usize index ) const
    {
        const i32 top{ this->m_bounds.top + g_gui.scaled( WIDGET_BLOCK_PADDING_TOP ) };
        const i32 span{ this->m_bounds.right - this->m_bounds.left };
        const i32 count{ static_cast< i32 >( RECORDER_TRANSPORT_COUNT ) };

        return rect_t{ this->m_bounds.left + static_cast< i32 >( index ) * span / count,
                       top,
                       this->m_bounds.left + static_cast< i32 >( index + 1 ) * span / count,
                       top + g_gui.scaled( WIDGET_SEGMENT_HEIGHT ) };
    }

    static bool running( usize index )
    {
        switch ( s_recorder_status.m_phase )
        {
            case recorder_phase_t::waiting:
            case recorder_phase_t::recording: return index == 0;
            case recorder_phase_t::armed:
            case recorder_phase_t::aligning:
            case recorder_phase_t::replaying: return index == 1;
            default: return false;
        }
    }

    static const char *label( usize index )
    {
        switch ( index )
        {
            case 0: return ASTRAL_XORSTR_STATIC( "Record" );
            case 1: return ASTRAL_XORSTR_STATIC( "Replay" );
            default: return ASTRAL_XORSTR_STATIC( "Stop" );
        }
    }

    static void press( usize index )
    {
        switch ( index )
        {
            case 0: g_recorder.request( recorder_request_t::record ); return;
            case 1: g_recorder.request( recorder_request_t::replay, recorder_selected_name( ) ); return;
            default: g_recorder.request( recorder_request_t::stop ); return;
        }
    }

    gui::row_state_t m_segments[ RECORDER_TRANSPORT_COUNT ]{ };
};

static bool recorder_has_entries( )
{
    return s_recorder_count != 0;
}

static bool recorder_no_entries( )
{
    return s_recorder_count == 0;
}

static const char *recorder_empty_text( )
{
    return s_recorder_status.m_map.empty( ) ? ASTRAL_XORSTR_STATIC( "Join a map to record" )
                                            : ASTRAL_XORSTR_STATIC( "No recordings on this map" );
}

static const char *recorder_class_text( )
{
    const recorder_entry_t *entry{ recorder_entry( ) };
    return entry ? player_class_label( entry->m_class ) : nullptr;
}

static const char *recorder_length_text( )
{
    static char text[ 48 ]{ };

    const recorder_entry_t *entry{ recorder_entry( ) };
    if ( !entry )
    {
        return nullptr;
    }

    str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "{:.2f} s, {} commands" ).get( ),
                    static_cast< f32 >( entry->m_commands - 1 ) * s_recorder_status.m_interval, entry->m_commands );
    return text;
}

static const char *recorder_route_text( )
{
    static char text[ 48 ]{ };

    const recorder_entry_t *entry{ recorder_entry( ) };
    if ( !entry )
    {
        return nullptr;
    }

    str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "{} u, {} {}" ).get( ), static_cast< i32 >( entry->m_length ),
                    entry->m_jumps, entry->m_jumps == 1 ? ASTRAL_XORSTR_STATIC( "jump" ) : ASTRAL_XORSTR_STATIC( "jumps" ) );
    return text;
}

static const char *recorder_start_text( )
{
    static char text[ 32 ]{ };

    const recorder_entry_t *entry{ recorder_entry( ) };
    if ( !entry )
    {
        return nullptr;
    }

    const f32 distance{ ( entry->m_start - s_recorder_status.m_origin ).length2d( ) };
    if ( distance <= VEC_HULL_MAX.x )
    {
        return ASTRAL_XORSTR_STATIC( "Inside the marker" );
    }

    str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "{} u away" ).get( ), static_cast< i32 >( distance ) );
    return text;
}

static void recorder_save( )
{
    g_recorder.request( recorder_request_t::save, recorder_selected_name( ), s_recorder_name->value( ) );
    s_recorder_name->set_value( string_view{ } );
}

static void recorder_replay( )
{
    g_recorder.request( recorder_request_t::replay, recorder_selected_name( ) );
}

static void recorder_delete( )
{
    g_recorder.request( recorder_request_t::remove, recorder_selected_name( ) );
}

static void recorder_refresh( )
{
    g_recorder.request( recorder_request_t::refresh );
}

static void build_recorder( gui::tab *misc )
{
    gui::sub_tab *recorder{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Recorder" ) ) };

    gui::group_box *control{ recorder->add_group( ICON_RULER, ASTRAL_XORSTR_STATIC( "Recorder" ), gui::column_t::left ) };
    gui::group_box *routes{ recorder->add_group( ICON_RULER, ASTRAL_XORSTR_STATIC( "Recordings" ), gui::column_t::right ) };
    gui::group_box *visuals{ recorder->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Visuals" ), gui::column_t::left ) };

    control->add< recorder_panel >( );

    recorder_transport *transport{ control->add< recorder_transport >( ) };
    gui::checkbox      *enabled{ control->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_recorder.m_enabled ) };
    gui::keybind       *record{ control->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Record" ), &g_cfg.m_recorder.m_record_key ) };
    gui::keybind       *replay{ control->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Replay" ), &g_cfg.m_recorder.m_replay_key ) };
    gui::checkbox      *attacks{ control->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Replay attacks" ),
                                                                &g_cfg.m_recorder.m_replay_attacks ) };
    gui::checkbox      *follow{ control->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Camera follows" ),
                                                               &g_cfg.m_recorder.m_camera_follows ) };

    transport->set_tooltip( ASTRAL_XORSTR_STATIC( "Record starts on your first movement input, so a take begins "
                                                 "standing still. Replay starts at once inside a start marker, "
                                                 "or waits for you to walk into the selected one." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder record replay stop route" ) );

    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Records your movement on this map and replays it. Recordings "
                                               "are kept per map in AstralMod/Routes, next to the game." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder route replay record" ) );

    record->set_tooltip( ASTRAL_XORSTR_STATIC( "Starts and stops a recording." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder record key bind" ) );

    replay->set_tooltip( ASTRAL_XORSTR_STATIC( "Replays the route whose start marker you stand in, or the "
                                              "selected one. Press it again to stop; any movement key also "
                                              "hands control back to you." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder replay key bind" ) );

    attacks->set_tooltip( ASTRAL_XORSTR_STATIC( "Presses attack where the recording did, for rocket and sticky "
                                               "jumps. Off, the recorded attacks are dropped." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder rocket sticky jump attack" ) );

    follow->set_tooltip( ASTRAL_XORSTR_STATIC( "Turns your view with the recording. Off, only the movement uses "
                                              "the recorded angles and you look around freely." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder camera view angles" ) );

    s_recorder_list = routes->add< gui::list_box >( s_recorder_labels, 0, &s_recorder_selected );

    gui::label *empty{ routes->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ), &recorder_empty_text ) };

    routes->add< gui::label >( ASTRAL_XORSTR_STATIC( "Class" ), &recorder_class_text )->set_pair( )->set_render_on( &recorder_has_entries );
    routes->add< gui::label >( ASTRAL_XORSTR_STATIC( "Length" ), &recorder_length_text )->set_pair( )->set_render_on( &recorder_has_entries );
    routes->add< gui::label >( ASTRAL_XORSTR_STATIC( "Route" ), &recorder_route_text )->set_pair( )->set_render_on( &recorder_has_entries );
    routes->add< gui::label >( ASTRAL_XORSTR_STATIC( "Start" ), &recorder_start_text )->set_pair( )->set_render_on( &recorder_has_entries );

    s_recorder_name = routes->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Recording name" ) );
    s_recorder_name->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Save" ), &recorder_save );

    gui::label *actions{ routes->add< gui::label >( ASTRAL_XORSTR_STATIC( "Recording" ), nullptr ) };

    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Replay" ), &recorder_replay );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Delete" ), &recorder_delete );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &recorder_refresh );

    s_recorder_list->set_render_on( &recorder_has_entries );
    empty->set_render_on( &recorder_no_entries );

    s_recorder_name->set_filter( &config_name_filter )
        ->set_limit( RECORDER_NAME_LENGTH )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Lowercase letters, digits, - and _. Saving under a name that "
                                            "exists replaces it, and a saved recording saved under a new "
                                            "name is renamed." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder save name rename" ) );

    actions->set_tooltip( ASTRAL_XORSTR_STATIC( "Replay waits for you in the selected start marker. Delete "
                                               "removes its file. Refresh reads the folder again, for "
                                               "recordings copied in from someone else." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder delete refresh share" ) );

    static const char *const ROUTE_STYLES[]{ ASTRAL_XORSTR_STATIC( "Line" ), ASTRAL_XORSTR_STATIC( "Separators" ),
                                             ASTRAL_XORSTR_STATIC( "Outlined" ), ASTRAL_XORSTR_STATIC( "Timeline" ),
                                             ASTRAL_XORSTR_STATIC( "Comet" ), ASTRAL_XORSTR_STATIC( "Flow" ) };

    gui::checkbox     *markers{ visuals->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Start markers" ),
                                                               &g_cfg.m_recorder.m_markers ) };
    gui::slider_int   *distance{ visuals->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Marker distance" ),
                                                                  &g_cfg.m_recorder.m_marker_distance, 500, 6000,
                                                                  ASTRAL_XORSTR_STATIC( "u" ) ) };
    gui::checkbox     *route{ visuals->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Route" ), &g_cfg.m_recorder.m_route ) };
    gui::color_picker *traveled{ visuals->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Traveled" ),
                                                                    &g_cfg.m_recorder.m_traveled_color, false ) };
    gui::combo_box    *style{ visuals->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Route style" ), ROUTE_STYLES,
                                                              config::PATH_STYLE_COUNT, &g_cfg.m_recorder.m_route_style ) };
    gui::checkbox     *keys{ visuals->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Key overlay" ),
                                                            &g_cfg.m_recorder.m_key_overlay ) };

    markers->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Marker" ), &g_cfg.m_recorder.m_marker_color, false );
    route->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Route" ), &g_cfg.m_recorder.m_route_color, false );

    markers->set_tooltip( ASTRAL_XORSTR_STATIC( "A ring at the start of every route on this map, with its "
                                               "name, class and length." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder marker ring label" ) );

    distance->set_tooltip( ASTRAL_XORSTR_STATIC( "How far away start markers stay visible. They fade out over "
                                                "the last quarter." ) );

    route->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the route: the next second in the route style, the rest "
                                             "up to the end faded, and what you already ran in the traveled "
                                             "colour." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder route path line" ) );

    traveled->set_tooltip( ASTRAL_XORSTR_STATIC( "Colour of the stretch already run during a replay." ) );

    style->set_tooltip( ASTRAL_XORSTR_STATIC( "How the next second of the route is drawn while it replays." ) );

    keys->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows W, A, S, D, jump, duck and attack while recording or "
                                            "replaying. Open the menu to drag it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "movement recorder keys wasd overlay hud" ) );
}

static bool tickbase_on( )
{
    return g_cfg.m_exploits.m_rapid_fire && g_cfg.m_exploits.m_warp;
}

static void build_exploits( gui::tab *misc )
{
    gui::sub_tab *exploits{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Exploits" ) ) };

    gui::group_box *tickbase{ exploits->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Tickbase" ), gui::column_t::left ) };
    gui::group_box *accuracy{ exploits->add_group( ICON_CROSSHAIR, ASTRAL_XORSTR_STATIC( "Accuracy" ), gui::column_t::right ) };
    gui::group_box *latency{ exploits->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Latency" ), gui::column_t::left ) };
    gui::group_box *consumables{ exploits->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Consumables" ), gui::column_t::right ) };

    auto rapid_fire{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Rapid fire" ), &g_cfg.m_exploits.m_rapid_fire ) };
    rapid_fire->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_exploits.m_rapid_fire_key );
    rapid_fire->set_tooltip( ASTRAL_XORSTR_STATIC( "Exploits how the player clock is managed on the server to allow you to fire your weapon faster." ) );

    auto anti_warp{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Anti-warp" ), &g_cfg.m_exploits.m_anti_warp ) };
    anti_warp->set_tooltip( ASTRAL_XORSTR_STATIC( "Brakes while a rapid fire shift runs so it doesn't warp you forward, then gives your speed back." ) );
    anti_warp->set_render_on( &g_cfg.m_exploits.m_rapid_fire );

    auto indicator{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Tick indicator" ), &g_cfg.m_exploits.m_indicator ) };
    indicator->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws the banked ticks and whether a shift can go. Open the menu to drag it." ) );
    indicator->set_render_on( &g_cfg.m_exploits.m_rapid_fire );

    auto recharge_key{ tickbase->add< gui::keybind >( ASTRAL_XORSTR_STATIC( "Recharge key" ), &g_cfg.m_exploits.m_recharge_key ) };
    recharge_key->set_tooltip( ASTRAL_XORSTR_STATIC( "Press this key to hold commands back to fill your tick bucket." ) );
    recharge_key->set_render_on( &g_cfg.m_exploits.m_rapid_fire );

    auto warp{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Warp" ), &g_cfg.m_exploits.m_warp ) };
    warp->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_exploits.m_warp_key );
    warp->set_tooltip( ASTRAL_XORSTR_STATIC( "Utilize the stored ticks you have to gain a boost in speed." ) );
    warp->set_render_on( &g_cfg.m_exploits.m_rapid_fire );

    auto warp_ticks{ tickbase->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Warp ticks" ), &g_cfg.m_exploits.m_warp_ticks, 2, 22, ASTRAL_XORSTR_STATIC( " tpt" ) ) };
    warp_ticks->set_tooltip( ASTRAL_XORSTR_STATIC( "The amount of ticks per command frame for this warp batch." ) );
    warp_ticks->set_render_on( &tickbase_on );

    auto passive_recharge{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Passive recharge" ), &g_cfg.m_exploits.m_passive_recharge ) };
    passive_recharge->set_tooltip( ASTRAL_XORSTR_STATIC( "Passively recharges ticks slowly over time. You may still manually recharge if you wish. Useful on low framerates or unstable connections." ) );
    passive_recharge->set_render_on( &g_cfg.m_exploits.m_rapid_fire );

    auto speedhack{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Speedhack" ), &g_cfg.m_exploits.m_speedhack ) };
    speedhack->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_exploits.m_speedhack_key );
    speedhack->set_tooltip( ASTRAL_XORSTR_STATIC( "Allows you to speedhack on servers that support it, usually Hack vs Hack servers." ) );

    auto speedhack_factor{ tickbase->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Speedhack factor" ), &g_cfg.m_exploits.m_speedhack_factor, 2, 22, ASTRAL_XORSTR_STATIC( " tpt" ) ) };
    speedhack_factor->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets how many commands per tick should you send to the server every client tick." ) );
    speedhack_factor->set_render_on( &g_cfg.m_exploits.m_speedhack );

    auto fake_duck{ tickbase->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Fake duck" ), &g_cfg.m_exploits.m_fake_duck ) };
    fake_duck->add_module< gui::keybind >( atl::string_view{ }, &g_cfg.m_exploits.m_fake_duck_key );
    fake_duck->set_tooltip( ASTRAL_XORSTR_STATIC( "Alternates the server-side duck state while held. Best used while stationary and on the ground." ) );

    consumables->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Infinite noisemaker spam" ), &g_cfg.m_misc.m_noisemaker_spam );
    auto auto_retry{ consumables->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "AutoRetry" ), &g_cfg.m_misc.m_auto_retry ) };
    auto_retry->set_tooltip( ASTRAL_XORSTR_STATIC( "Runs retry when TF2 announces a pending autobalance team change." ) );

    static const char *const NOISEMAKER_SPAM_MODES[]{ ASTRAL_XORSTR_STATIC( "On kill" ), ASTRAL_XORSTR_STATIC( "Always" ) };

    auto *spam_mode{ consumables->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Noisemaker spam mode" ), NOISEMAKER_SPAM_MODES, config::NOISEMAKER_SPAM_COUNT, &g_cfg.m_misc.m_noisemaker_spam_mode ) };
    spam_mode->set_render_on( &g_cfg.m_misc.m_noisemaker_spam );

    gui::checkbox   *fake_latency{ latency->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Fake latency" ), &g_cfg.m_exploits.m_fake_latency ) };
    gui::slider_int *fake_latency_ms{ latency->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Latency" ), &g_cfg.m_exploits.m_fake_latency_ms, 50, 900 ) };
    auto             extended_bt{ latency->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Extended Backtrack" ), &g_cfg.m_exploits.m_extended_backtrack ) };

    fake_latency->add_module< gui::keybind >( string_view{ }, &g_cfg.m_exploits.m_fake_latency_key );

    fake_latency->set_tooltip( ASTRAL_XORSTR_STATIC( "Acknowledges the server's packets late, so it "
                                                    "measures your ping that much higher and rewinds "
                                                    "targets that far back, without delaying anything "
                                                    "you see." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fake latency ping backtrack lagcomp" ) );

    fake_latency_ms->set_tooltip( ASTRAL_XORSTR_STATIC( "Milliseconds added on top of your real ping." ) )
        ->set_render_on( &g_cfg.m_exploits.m_fake_latency );

    extended_bt->set_tooltip( ASTRAL_XORSTR_STATIC( "Enforces a minimum fake latency value to extend your backtrack window past 200 ms." ) );
    extended_bt->set_render_on( &g_cfg.m_exploits.m_fake_latency );

    gui::checkbox   *ping_reducer{ latency->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Ping reducer" ),
                                                                  &g_cfg.m_exploits.m_ping_reducer ) };
    gui::slider_int *reported_cmdrate{ latency->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Reported cmdrate" ), &g_cfg.m_exploits.m_ping_reducer_cmdrate, 1, 66 ) };

    ping_reducer->set_tooltip( ASTRAL_XORSTR_STATIC( "Reports a lower cl_cmdrate to the server to "
                                                    "reduce the ping shown on the scoreboard. "
                                                    "Your actual command rate stays unchanged." ) );
    reported_cmdrate->set_tooltip( ASTRAL_XORSTR_STATIC( "Lower values subtract more from the "
                                                        "server's scoreboard ping calculation; "
                                                        "1 gives the largest reduction." ) )
        ->set_render_on( &g_cfg.m_exploits.m_ping_reducer );

    gui::checkbox *seed{ accuracy->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Seed prediction" ),
                                                         &g_cfg.m_exploits.m_seed_prediction ) };
    gui::checkbox *seed_indicator{ accuracy->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Seed indicator" ),
                                                                   &g_cfg.m_exploits.m_seed_indicator ) };
    gui::checkbox *crits{ accuracy->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Crits" ),
                                                          &g_cfg.m_crits.m_enabled ) };
    gui::checkbox *force_crits{ accuracy->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Force crits" ),
                                                                &g_cfg.m_crits.m_force ) };
    gui::checkbox *avoid_crits{ accuracy->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Avoid random crits" ), &g_cfg.m_crits.m_avoid ) };
    gui::checkbox *crit_indicator{ accuracy->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Crit indicator" ), &g_cfg.m_crits.m_indicator ) };
#if defined( ASTRAL_DEV )
    gui::checkbox *crit_debug{ accuracy->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Crit debug log" ), &g_cfg.m_crits.m_debug ) };
#endif
    static const char *const PILL_CRIT_MODES[]{ ASTRAL_XORSTR_STATIC( "No rotation" ),
                                                ASTRAL_XORSTR_STATIC( "Critical" ) };

    gui::checkbox  *seed_spin{ accuracy->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "No pill rotation" ), &g_cfg.m_pills.m_seed_spin ) };
    gui::combo_box *pill_crit{ accuracy->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "On crit" ), PILL_CRIT_MODES, 2, &g_cfg.m_pills.m_crit_priority ) };

    crits->add_module< gui::keybind >( string_view{ }, &g_cfg.m_crits.m_key );

    seed->set_tooltip( ASTRAL_XORSTR_STATIC( "Predicts the server's spread roll so shots land "
                                            "on the crosshair." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "nospread no spread seed" ) );

    seed_indicator->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows whether the seed clock is locked, how tight the estimate is "
                                                      "against the server's seed step, and the step itself. Open the menu to drag it." ) )
        ->set_render_on( &g_cfg.m_exploits.m_seed_prediction );

    crits->set_tooltip( ASTRAL_XORSTR_STATIC( "Controls random crit rolls. The server decides "
                                             "whether each requested crit is allowed." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "crit crits crithack" ) );

    avoid_crits->set_tooltip( ASTRAL_XORSTR_STATIC( "Selects non-crit rolls when a crit is not being requested. "
                                                   "Round-history evidence is reconciled as damage lands." ) );
    force_crits->set_tooltip( ASTRAL_XORSTR_STATIC( "Requests a critical roll when the bucket can pay and ranged crits are not banned. "
                                                   "The server decides the final result." ) );
    force_crits->set_render_on( &g_cfg.m_crits.m_enabled );
    avoid_crits->set_render_on( &g_cfg.m_crits.m_enabled );
    crit_indicator->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows available crits, bucket state and the ranged observed-rate estimate. "
                                                      "Recovery damage assumes non-crit ranged hits at the current crit chance." ) )
        ->set_render_on( &g_cfg.m_crits.m_enabled );
#if defined( ASTRAL_DEV )
    crit_debug->set_tooltip( ASTRAL_XORSTR_STATIC( "Logs crit attempts, round-history evidence, observed samples and damage attribution." ) )
        ->set_render_on( &g_cfg.m_crits.m_enabled );
#endif
    seed_spin->set_tooltip( ASTRAL_XORSTR_STATIC( "Stops pipes tumbling, so where they land "
                                                 "matches the prediction." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "pill pills pipe rotation spin seed demoman" ) );

    pill_crit->set_tooltip( ASTRAL_XORSTR_STATIC( "One command number cannot hold a crit seed and "
                                                 "a spinless pill, so pick which one wins." ) )
        ->set_render_on( &pills_fighting_crits )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "pill crit priority rotation spin demoman" ) );
}

static void build_queue( gui::tab *misc )
{
    gui::sub_tab *queue{ misc->add_subtab( ASTRAL_XORSTR_STATIC( "Queue" ) ) };

    gui::group_box  *matchmaking{ queue->add_group( ICON_ZAP,
                                                    ASTRAL_XORSTR_STATIC( "Matchmaking" ),
                                                    gui::column_t::left ) };
    gui::group_box  *regions{ queue->add_group( ICON_CODE,
                                                ASTRAL_XORSTR_STATIC( "Desired regions" ),
                                                gui::column_t::right ) };
    gui::checkbox   *enabled{ matchmaking->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Fast queue" ), &g_cfg.m_queue.m_enabled ) };
    gui::slider_int *max_ping{ matchmaking->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Maximum ping" ), &g_cfg.m_queue.m_max_ping, 10, 1000,
        ASTRAL_XORSTR_STATIC( "ms" ) ) };

    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Reports a near zero ping for every relay you would "
                                               "accept, so matchmaking stops waiting on the ping "
                                               "sweep and takes the first game it finds." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "fast queue matchmaking casual ping region "
                                             "datacenter server" ) );

    max_ping->set_tooltip( ASTRAL_XORSTR_STATIC( "Relays slower than this keep their real ping and "
                                                "are left for last." ) )
        ->set_render_on( &g_cfg.m_queue.m_enabled );

    gui::checkbox   *delay_join{ matchmaking->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Delay join timer" ), &g_cfg.m_queue.m_delay_join ) };
    gui::slider_int *delay_join_time{ matchmaking->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "Join time" ), &g_cfg.m_queue.m_delay_join_time, 15, 1000,
        ASTRAL_XORSTR_STATIC( "s" ) ) };

    delay_join->set_tooltip( ASTRAL_XORSTR_STATIC( "Stretches the countdown on the match found popup, "
                                                  "which otherwise joins for you after ten seconds." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "delay join timer match found accept popup countdown "
                                             "matchmaking casual" ) );

    delay_join_time->set_tooltip( ASTRAL_XORSTR_STATIC( "Seconds you get to accept before the game "
                                                       "joins the match for you." ) )
        ->set_render_on( &g_cfg.m_queue.m_delay_join );

    static const char *const EUROPE_REGIONS[]{
        ASTRAL_XORSTR_STATIC( "Netherlands" ), ASTRAL_XORSTR_STATIC( "Germany" ),
        ASTRAL_XORSTR_STATIC( "United Kingdom" ), ASTRAL_XORSTR_STATIC( "France" ),
        ASTRAL_XORSTR_STATIC( "Spain" ), ASTRAL_XORSTR_STATIC( "Sweden" ),
        ASTRAL_XORSTR_STATIC( "Finland" ), ASTRAL_XORSTR_STATIC( "Austria" ),
        ASTRAL_XORSTR_STATIC( "Poland" )
    };

    static const char *const ASIA_REGIONS[]{
        ASTRAL_XORSTR_STATIC( "India" ), ASTRAL_XORSTR_STATIC( "United Arab Emirates" ),
        ASTRAL_XORSTR_STATIC( "Hong Kong" ), ASTRAL_XORSTR_STATIC( "Japan" ),
        ASTRAL_XORSTR_STATIC( "South Korea" ), ASTRAL_XORSTR_STATIC( "Singapore" ),
        ASTRAL_XORSTR_STATIC( "Guam" )
    };

    static const char *const NORTH_AMERICA_REGIONS[]{
        ASTRAL_XORSTR_STATIC( "Virginia" ), ASTRAL_XORSTR_STATIC( "Georgia" ),
        ASTRAL_XORSTR_STATIC( "Illinois" ), ASTRAL_XORSTR_STATIC( "Texas" ),
        ASTRAL_XORSTR_STATIC( "California" ), ASTRAL_XORSTR_STATIC( "Washington" )
    };

    static const char *const SOUTH_AMERICA_REGIONS[]{
        ASTRAL_XORSTR_STATIC( "Brazil" ), ASTRAL_XORSTR_STATIC( "Peru" ),
        ASTRAL_XORSTR_STATIC( "Chile" ), ASTRAL_XORSTR_STATIC( "Argentina" )
    };

    static const char *const REST_REGIONS[]{
        ASTRAL_XORSTR_STATIC( "South Africa" ), ASTRAL_XORSTR_STATIC( "Australia" )
    };

    struct region_group_t
    {
        const char        *m_label{ };
        const char *const *m_items{ };
        usize              m_count{ };
    };

    // @ same order as the region enum, which is what lets the running offset below line each
    // group up with its flags. No group may pass COMBO_POPUP_MAX_ROWS, the popup does not scroll.
    const region_group_t GROUPS[]{
        { ASTRAL_XORSTR_STATIC( "Europe" ), EUROPE_REGIONS,
          sizeof( EUROPE_REGIONS ) / sizeof( EUROPE_REGIONS[ 0 ] ) },
        { ASTRAL_XORSTR_STATIC( "Asia" ), ASIA_REGIONS,
          sizeof( ASIA_REGIONS ) / sizeof( ASIA_REGIONS[ 0 ] ) },
        { ASTRAL_XORSTR_STATIC( "North America" ), NORTH_AMERICA_REGIONS,
          sizeof( NORTH_AMERICA_REGIONS ) / sizeof( NORTH_AMERICA_REGIONS[ 0 ] ) },
        { ASTRAL_XORSTR_STATIC( "South America" ), SOUTH_AMERICA_REGIONS,
          sizeof( SOUTH_AMERICA_REGIONS ) / sizeof( SOUTH_AMERICA_REGIONS[ 0 ] ) },
        { ASTRAL_XORSTR_STATIC( "Rest of world" ), REST_REGIONS,
          sizeof( REST_REGIONS ) / sizeof( REST_REGIONS[ 0 ] ) }
    };

    usize first{ };

    for ( const region_group_t &group : GROUPS )
    {
        gui::combo_box *desired{ regions->add< gui::combo_box >(
            group.m_label, group.m_items, group.m_count, &g_cfg.m_queue.m_desired[ first ] ) };

        desired->set_tooltip( ASTRAL_XORSTR_STATIC( "Countries you are willing to play in. Clearing "
                                                   "one hands its relays an unplayable ping, so "
                                                   "matchmaking drops them." ) )
            ->set_keywords( ASTRAL_XORSTR_STATIC( "region country relay datacenter allow" ) )
            ->set_render_on( &g_cfg.m_queue.m_enabled );

        first += group.m_count;
    }
}

static void build_misc( )
{
    gui::tab *misc{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Misc" ), ICON_WRENCH ) };

    build_misc_general( misc );
    build_automations( misc );
    build_radio( misc );
    build_soundboard( misc );
    build_movement( misc );
    build_recorder( misc );
    build_exploits( misc );
    build_queue( misc );
}

template< usize kind >
static bool holding_kind( )
{
    return g_skins.editing( ) && g_skins.held_kind( ) == kind;
}

static bool skins_editing( )
{
    return g_skins.editing( );
}

static bool skins_idle( )
{
    return !g_skins.editing( );
}

static bool skins_editing_wearable( )
{
    return g_skins.editing_wearable( );
}

static bool skins_idle_wearable( )
{
    return !g_skins.editing_wearable( );
}

static bool skins_paint_shown( )
{
    return g_skins.editing( ) && g_skins.held_paintkit_capable( );
}

static bool skins_unpaintable( )
{
    return g_skins.editing( ) && !g_skins.held_paintkit_capable( );
}

static bool skins_painted( )
{
    return skins_paint_shown( ) && g_cfg.m_skins.m_weapon_edit.m_paintkit != 0;
}

static const char *skins_unpaintable_text( )
{
    return ASTRAL_XORSTR_STATIC( "No war paint exists for this weapon." );
}

// the game's own kit tiers: specialized brings the sheen, professional the eye effect on top
static bool skins_sheen_shown( )
{
    return g_skins.editing( ) && g_cfg.m_skins.m_weapon_edit.m_killstreak >= 2;
}

static bool skins_killstreaker_shown( )
{
    return g_skins.editing( ) && g_cfg.m_skins.m_weapon_edit.m_killstreak >= 3;
}

static bool skins_australium_shown( )
{
    return g_skins.editing( ) && g_skins.held_australium_capable( );
}

static bool skins_festive_shown( )
{
    return g_skins.editing( ) && g_skins.held_festive_capable( );
}

static const char *skins_held_text( )
{
    if ( !g_skins.in_game( ) )
    {
        return ASTRAL_XORSTR_STATIC( "You must be in-game\nto use the skin changer." );
    }

    return g_skins.editing( ) ? g_skins.held_name( ) : ASTRAL_XORSTR_STATIC( "No weapon in hand." );
}

static const char *skins_wearable_text( )
{
    if ( !g_skins.in_game( ) )
    {
        return ASTRAL_XORSTR_STATIC( "You must be in-game\nto use the skin changer." );
    }

    return g_skins.editing_wearable( ) ? g_skins.wearable_name( ) : ASTRAL_XORSTR_STATIC( "Nothing worn in this slot." );
}

static void skins_reset_weapon( )
{
    g_cfg.m_skins.m_weapon_edit = config::skin_t{ };
}

static void build_skins_weapons( gui::tab *skins_tab )
{
    // one model list per weapon kind, bound to the same row; only the kind in hand shows
    static bool ( *const KIND_PREDICATES[ skins::KIND_COUNT ] )( ){
        &holding_kind< 0 >, &holding_kind< 1 >, &holding_kind< 2 >, &holding_kind< 3 >,
        &holding_kind< 4 >, &holding_kind< 5 >, &holding_kind< 6 >, &holding_kind< 7 >,
        &holding_kind< 8 >, &holding_kind< 9 >, &holding_kind< 10 >, &holding_kind< 11 >,
        &holding_kind< 12 >, &holding_kind< 13 >, &holding_kind< 14 >, &holding_kind< 15 >,
        &holding_kind< 16 >, &holding_kind< 17 >, &holding_kind< 18 >, &holding_kind< 19 >,
        &holding_kind< 20 >, &holding_kind< 21 >, &holding_kind< 22 >, &holding_kind< 23 >,
        &holding_kind< 24 >, &holding_kind< 25 >, &holding_kind< 26 >, &holding_kind< 27 >,
        &holding_kind< 28 >, &holding_kind< 29 >, &holding_kind< 30 >, &holding_kind< 31 >,
        &holding_kind< 32 >, &holding_kind< 33 >, &holding_kind< 34 >, &holding_kind< 35 >,
        &holding_kind< 36 >
    };

    gui::sub_tab *weapons{ skins_tab->add_subtab( ASTRAL_XORSTR_STATIC( "Weapons" ) ) };

    gui::group_box *weapon{ weapons->add_group( ICON_DOLLAR, ASTRAL_XORSTR_STATIC( "Weapon" ),
                                                gui::column_t::left ) };
    gui::group_box *look{ weapons->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Appearance" ),
                                              gui::column_t::right ) };

    static const char *const SKIN_MODES[]{ ASTRAL_XORSTR_STATIC( "Simple" ),
                                           ASTRAL_XORSTR_STATIC( "Advanced" ) };
    gui::segment_bar        *mode{ weapon->add< gui::segment_bar >(
        SKIN_MODES, config::skins_t::MODE_COUNT, &g_cfg.m_skins.m_mode ) };
    mode->set_tooltip( ASTRAL_XORSTR_STATIC(
                           "Simple uses the existing held-item skin changer. Advanced uses the separate synthetic inventory manager. Only one mode can run at a time." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "skin inventory simple advanced mode" ) );

    gui::checkbox *enabled{ weapon->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ),
                                                          &g_cfg.m_skins.m_enabled ) };
    gui::checkbox *shared{ weapon->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Share with Astral users" ),
                                                         &g_cfg.m_skins.m_shared ) };
    gui::label    *held{ weapon->add< gui::label >( ASTRAL_XORSTR_STATIC( "Weapon" ), &skins_held_text, usize{ 2 } ) };
    gui::label    *holding{ weapon->add< gui::label >( ASTRAL_XORSTR_STATIC( "Weapon" ), &skins_held_text ) };

    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Dresses the items you carry with the looks in "
                                               "this tab. Only you see them unless you share them." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "skins skinchanger australium festive unusual "
                                             "killstreak sheen war paint model attributes" ) );
    shared->set_tooltip( ASTRAL_XORSTR_STATIC( "Sends your weapon looks and unusual hats to other "
                                              "Astral users on your server, and shows theirs on them." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "skins share shared cloud friends others" ) );

    held->set_render_on( &skins_idle );
    holding->set_render_on( &skins_editing );

    for ( usize kind{ }; kind < skins::KIND_COUNT; ++kind )
    {
        gui::dynamic_list_box *models{ weapon->add< gui::dynamic_list_box >(
            &g_cfg.m_skins.m_weapon_edit.m_model ) };

        g_skins.bind_model_list( kind, models );

        models->set_keywords( ASTRAL_XORSTR_STATIC( "model reskin weapon" ) )
            ->set_render_on( KIND_PREDICATES[ kind ] );
    }

    gui::button *reset{ weapon->add< gui::button >( ASTRAL_XORSTR_STATIC( "Reset weapon" ),
                                                    &skins_reset_weapon ) };

    reset->set_tooltip( ASTRAL_XORSTR_STATIC( "Puts the weapon in hand back to how the server "
                                             "sent it." ) )
        ->set_render_on( &skins_editing );

    static const char *const UNUSUALS[]{
        ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Community Sparkle" ),
        ASTRAL_XORSTR_STATIC( "Hot" ), ASTRAL_XORSTR_STATIC( "Isotope" ),
        ASTRAL_XORSTR_STATIC( "Cool" ), ASTRAL_XORSTR_STATIC( "Energy Orb" )
    };

    static const char *const KILLSTREAKS[]{
        ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Killstreak" ),
        ASTRAL_XORSTR_STATIC( "Specialized" ), ASTRAL_XORSTR_STATIC( "Professional" )
    };

    static const char *const SHEENS[]{
        ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Team Shine" ),
        ASTRAL_XORSTR_STATIC( "Deadly Daffodil" ), ASTRAL_XORSTR_STATIC( "Manndarin" ),
        ASTRAL_XORSTR_STATIC( "Mean Green" ), ASTRAL_XORSTR_STATIC( "Agonizing Emerald" ),
        ASTRAL_XORSTR_STATIC( "Villainous Violet" ), ASTRAL_XORSTR_STATIC( "Hot Rod" )
    };

    static const char *const KILLSTREAKERS[]{
        ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Fire Horns" ),
        ASTRAL_XORSTR_STATIC( "Cerebral Discharge" ), ASTRAL_XORSTR_STATIC( "Tornado" ),
        ASTRAL_XORSTR_STATIC( "Flames" ), ASTRAL_XORSTR_STATIC( "Singularity" ),
        ASTRAL_XORSTR_STATIC( "Incinerator" ), ASTRAL_XORSTR_STATIC( "Hypno-Beam" )
    };

    gui::checkbox  *australium{ look->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Australium" ),
                                                            &g_cfg.m_skins.m_weapon_edit.m_australium ) };
    gui::checkbox  *festivized{ look->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Festivized" ),
                                                            &g_cfg.m_skins.m_weapon_edit.m_festivized ) };
    gui::combo_box *unusual{ look->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Unusual effect" ),
                                                          UNUSUALS, skins::UNUSUAL_COUNT,
                                                          &g_cfg.m_skins.m_weapon_edit.m_unusual ) };
    gui::combo_box *killstreak{ look->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Killstreak kit" ),
                                                             KILLSTREAKS, skins::KILLSTREAK_COUNT,
                                                             &g_cfg.m_skins.m_weapon_edit.m_killstreak ) };
    gui::combo_box *sheen{ look->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Sheen" ), SHEENS,
                                                        skins::SHEEN_COUNT,
                                                        &g_cfg.m_skins.m_weapon_edit.m_sheen ) };
    gui::combo_box *killstreaker{ look->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Killstreaker" ),
                                                               KILLSTREAKERS, skins::KILLSTREAKER_COUNT,
                                                               &g_cfg.m_skins.m_weapon_edit.m_killstreaker ) };
    gui::checkbox  *sheen_tint{ look->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Custom sheen colour" ),
                                                            &g_cfg.m_skins.m_sheen_custom ) };

    australium->set_tooltip( ASTRAL_XORSTR_STATIC( "Gold skin. Offered for the weapons that "
                                                  "ship one." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "australium gold skin" ) )
        ->set_render_on( &skins_australium_shown );

    festivized->set_tooltip( ASTRAL_XORSTR_STATIC( "Hangs the festive lights. Offered for the "
                                                  "weapons that have them." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "festive festivized lights christmas" ) )
        ->set_render_on( &skins_festive_shown );

    unusual->set_tooltip( ASTRAL_XORSTR_STATIC( "Particle effect attached to the weapon." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "unusual effect particle hot isotope cool energy orb sparkle" ) )
        ->set_render_on( &skins_editing );

    killstreak->set_tooltip( ASTRAL_XORSTR_STATIC( "Kit tier. Kills with the weapon are counted "
                                                  "for the feed and the HUD; specialized adds "
                                                  "the sheen, professional the eye effect at "
                                                  "five." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "killstreak kit tier specialized professional counter" ) )
        ->set_render_on( &skins_editing );

    sheen->set_keywords( ASTRAL_XORSTR_STATIC( "killstreak sheen colour color shine" ) )
        ->set_render_on( &skins_sheen_shown );

    killstreaker->set_keywords( ASTRAL_XORSTR_STATIC( "killstreaker eye effect fire horns tornado flames" ) )
        ->set_render_on( &skins_killstreaker_shown );

    sheen_tint->set_tooltip( ASTRAL_XORSTR_STATIC( "Repaints every sheen you can see, yours and "
                                                  "everyone else's, in this colour." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "sheen tint colour killstreak shine" ) )
        ->set_render_on( &skins_editing )
        ->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Sheen" ),
                                           &g_cfg.m_skins.m_sheen_color, false );
}

static void build_skins_paint( gui::tab *skins_tab )
{
    gui::sub_tab *paint{ skins_tab->add_subtab( ASTRAL_XORSTR_STATIC( "War paint" ) ) };

    gui::group_box *kits{ paint->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "War paint" ),
                                            gui::column_t::left ) };
    gui::group_box *finish{ paint->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Finish" ),
                                              gui::column_t::right ) };

    static const char *const WEARS[]{
        ASTRAL_XORSTR_STATIC( "Factory New" ), ASTRAL_XORSTR_STATIC( "Minimal Wear" ),
        ASTRAL_XORSTR_STATIC( "Field-Tested" ), ASTRAL_XORSTR_STATIC( "Well-Worn" ),
        ASTRAL_XORSTR_STATIC( "Battle Scarred" )
    };

    gui::label            *idle{ kits->add< gui::label >( ASTRAL_XORSTR_STATIC( "Weapon" ), &skins_held_text, usize{ 2 } ) };
    gui::label            *holding{ kits->add< gui::label >( ASTRAL_XORSTR_STATIC( "Weapon" ), &skins_held_text ) };
    gui::label            *none{ kits->add< gui::label >( ASTRAL_XORSTR_STATIC( "Weapon" ), &skins_unpaintable_text ) };
    gui::dynamic_list_box *list{ kits->add< gui::dynamic_list_box >(
        &g_cfg.m_skins.m_weapon_edit.m_paintkit ) };
    gui::combo_box        *wear{ finish->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Wear" ), WEARS,
                                                                skins::WEAR_COUNT,
                                                                &g_cfg.m_skins.m_weapon_edit.m_wear ) };
    gui::slider_int       *seed{ finish->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Seed" ),
                                                                 &g_cfg.m_skins.m_weapon_edit.m_seed, 0, 10000 ) };

    g_skins.bind_paintkit_list( list );

    idle->set_render_on( &skins_idle );
    holding->set_render_on( &skins_editing );
    none->set_render_on( &skins_unpaintable );

    list->set_keywords( ASTRAL_XORSTR_STATIC( "war paint paintkit skin decorated" ) )
        ->set_render_on( &skins_paint_shown );

    wear->set_tooltip( ASTRAL_XORSTR_STATIC( "How worn the paint looks. Paints only exist for "
                                            "the weapons Valve made them for." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "wear factory new battle scarred" ) )
        ->set_render_on( &skins_painted );

    seed->set_tooltip( ASTRAL_XORSTR_STATIC( "Picks where the paint's pattern lands on the weapon. "
                                            "0 keeps the item's own pattern." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "seed pattern war paint paintkit" ) )
        ->set_render_on( &skins_painted );
}

static void build_skins_wearables( gui::tab *skins_tab )
{
    gui::sub_tab *wearables{ skins_tab->add_subtab( ASTRAL_XORSTR_STATIC( "Cosmetics" ) ) };

    gui::group_box *worn{ wearables->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Cosmetic" ),
                                                gui::column_t::left ) };
    gui::group_box *effect{ wearables->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Unusual effect" ),
                                                  gui::column_t::right ) };

    static const char *const SLOTS[]{ ASTRAL_XORSTR_STATIC( "1" ), ASTRAL_XORSTR_STATIC( "2" ),
                                      ASTRAL_XORSTR_STATIC( "3" ), ASTRAL_XORSTR_STATIC( "4" ) };

    gui::segment_bar      *slot{ worn->add< gui::segment_bar >( SLOTS, config::SKIN_WEARABLE_SLOTS,
                                                                &g_cfg.m_skins.m_wearable_slot ) };
    gui::label            *idle{ worn->add< gui::label >( ASTRAL_XORSTR_STATIC( "Cosmetic" ), &skins_wearable_text, usize{ 2 } ) };
    gui::label            *wearing{ worn->add< gui::label >( ASTRAL_XORSTR_STATIC( "Cosmetic" ), &skins_wearable_text ) };
    gui::dynamic_list_box *list{ effect->add< gui::dynamic_list_box >(
        &g_cfg.m_skins.m_wearable_edit.m_unusual ) };

    g_skins.bind_effect_list( list );

    slot->set_keywords( ASTRAL_XORSTR_STATIC( "cosmetic hat wearable slot" ) );
    idle->set_render_on( &skins_idle_wearable );
    wearing->set_render_on( &skins_editing_wearable );

    list->set_keywords( ASTRAL_XORSTR_STATIC( "unusual effect hat cosmetic burning flames sunbeams" ) )
        ->set_render_on( &skins_editing_wearable );
}

static void build_skins_spells( gui::tab *skins_tab )
{
    gui::sub_tab *spells{ skins_tab->add_subtab( ASTRAL_XORSTR_STATIC( "Spells" ) ) };

    gui::group_box *halloween{ spells->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Halloween" ),
                                                  gui::column_t::left ) };
    gui::group_box *smissmas{ spells->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Smissmas" ),
                                                 gui::column_t::right ) };

    static const char *const FOOTPRINTS[]{
        ASTRAL_XORSTR_STATIC( "None" ), ASTRAL_XORSTR_STATIC( "Team Spirit" ),
        ASTRAL_XORSTR_STATIC( "Headless Horseshoes" ), ASTRAL_XORSTR_STATIC( "Gangreen" ),
        ASTRAL_XORSTR_STATIC( "Corpse Gray" ), ASTRAL_XORSTR_STATIC( "Violent Violet" ),
        ASTRAL_XORSTR_STATIC( "Rotten Orange" ), ASTRAL_XORSTR_STATIC( "Bruised Purple" )
    };

    static const char *const JINGLES[]{ ASTRAL_XORSTR_STATIC( "None" ),
                                        ASTRAL_XORSTR_STATIC( "Jingle" ),
                                        ASTRAL_XORSTR_STATIC( "High jingle" ) };

    gui::checkbox     *pumpkins{ halloween->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Pumpkin bombs" ),
                                                                  &g_cfg.m_skins.m_spells.m_pumpkin_bombs ) };
    gui::checkbox     *voices{ halloween->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Voices from below" ),
                                                                &g_cfg.m_skins.m_spells.m_voices_from_below ) };
    gui::checkbox     *flames{ halloween->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Halloween fire" ),
                                                                &g_cfg.m_skins.m_spells.m_green_flames ) };
    gui::combo_box    *footprints{ halloween->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Footprints" ),
                                                                     FOOTPRINTS, skins::FOOTPRINT_COUNT,
                                                                     &g_cfg.m_skins.m_spells.m_footprints ) };
    gui::checkbox     *custom{ halloween->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Custom colour" ),
                                                                &g_cfg.m_skins.m_spells.m_footprint_custom ) };
    gui::color_picker *print_tint{ halloween->add< gui::color_picker >(
        ASTRAL_XORSTR_STATIC( "Footprints" ), &g_cfg.m_skins.m_spells.m_footprint_color, false ) };
    gui::checkbox     *full_moon{ halloween->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Full moon" ),
                                                                   &g_cfg.m_skins.m_spells.m_full_moon ) };
    gui::combo_box    *jingle{ smissmas->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Footsteps" ),
                                                                JINGLES, skins::JINGLE_COUNT,
                                                                &g_cfg.m_skins.m_spells.m_jingle ) };
    gui::checkbox     *replace{ smissmas->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Replace step sound" ),
                                                                &g_cfg.m_skins.m_spells.m_replace_step ) };

    pumpkins->set_tooltip( ASTRAL_XORSTR_STATIC( "Your rockets, pipes and stickies burst like "
                                                "pumpkin bombs." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "halloween spell pumpkin bombs explosions" ) );

    voices->set_tooltip( ASTRAL_XORSTR_STATIC( "Pitches your voice lines down the way the "
                                              "Voices From Below spell does." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "halloween spell voice modulation spooky" ) );

    flames->set_tooltip( ASTRAL_XORSTR_STATIC( "Green flamethrower fire and burn effects." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "halloween spell green flames fire pyro" ) );

    footprints->set_tooltip( ASTRAL_XORSTR_STATIC( "Coloured footprints left behind you." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "halloween spell footprints footsteps colour" ) );

    custom->set_tooltip( ASTRAL_XORSTR_STATIC( "Paints the prints in a colour of your own instead "
                                              "of the one the spell above carries." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "footprints custom colour tint" ) );

    print_tint->set_keywords( ASTRAL_XORSTR_STATIC( "footprints custom colour tint" ) )
        ->set_render_on( &g_cfg.m_skins.m_spells.m_footprint_custom );

    full_moon->set_tooltip( ASTRAL_XORSTR_STATIC( "Keeps the full moon up all year, so everyone's Halloween "
                                                 "spells, zombie souls and spooky visuals show outside the "
                                                 "season. Holiday restricted cosmetics still need the server." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "full moon bypass halloween holiday spells zombie soul spooky" ) );

    jingle->set_tooltip( ASTRAL_XORSTR_STATIC( "Jingle bells on every step you take." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "smissmas christmas jingle footsteps bells" ) );

    replace->set_tooltip( ASTRAL_XORSTR_STATIC( "Silences your own footstep so only the jingle is "
                                               "heard." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "footstep sound mute silence replace" ) )
        ->set_render_on( &footsteps_audible );
}

static void build_skins_ragdolls( gui::tab *skins_tab )
{
    gui::sub_tab *ragdolls{ skins_tab->add_subtab( ASTRAL_XORSTR_STATIC( "Ragdolls" ) ) };

    gui::group_box *corpses{ ragdolls->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Corpses" ),
                                                  gui::column_t::left ) };
    gui::group_box *effects{ ragdolls->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Effects" ),
                                                  gui::column_t::right ) };

    static const char *const TARGET_LABELS[]{ ASTRAL_XORSTR_STATIC( "Enemies" ),
                                              ASTRAL_XORSTR_STATIC( "Teammates" ) };

    config::ragdolls_t &cfg{ g_cfg.m_skins.m_ragdolls };

    gui::checkbox  *enabled{ corpses->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &cfg.m_enabled ) };
    gui::combo_box *targets{ corpses->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Targets" ), TARGET_LABELS,
                                                             config::RAGDOLL_TARGET_COUNT, cfg.m_targets ) };
    gui::checkbox  *gold{ effects->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Gold" ), &cfg.m_gold ) };
    gui::checkbox  *ice{ effects->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Ice" ), &cfg.m_ice ) };
    gui::checkbox  *dissolve{ effects->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Dissolve" ), &cfg.m_dissolve ) };
    gui::checkbox  *burning{ effects->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Burning" ), &cfg.m_burning ) };
    gui::checkbox  *electrocuted{ effects->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Electrocuted" ), &cfg.m_electrocuted ) };
    gui::checkbox  *ash{ effects->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Ash" ), &cfg.m_ash ) };

    enabled->set_tooltip( ASTRAL_XORSTR_STATIC( "Gives the corpses you see the effects on the "
                                               "right. A kill with a gilded weapon makes a gold "
                                               "statue either way." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll corpse effects gold ice dissolve burning ash" ) );

    targets->set_tooltip( ASTRAL_XORSTR_STATIC( "Whose corpses the effects are given to." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll corpse enemies teammates targets" ) )
        ->set_render_on( &cfg.m_enabled );

    gold->set_tooltip( ASTRAL_XORSTR_STATIC( "Gold statue, the Golden Frying Pan's." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll gold statue" ) )
        ->set_render_on( &cfg.m_enabled );

    ice->set_tooltip( ASTRAL_XORSTR_STATIC( "Ice statue, the Spy-cicle's." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll ice statue frozen" ) )
        ->set_render_on( &cfg.m_enabled );

    dissolve->set_tooltip( ASTRAL_XORSTR_STATIC( "Cow Mangler dissolve. Ash wins over it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll dissolve plasma" ) )
        ->set_render_on( &cfg.m_enabled );

    burning->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll burning fire" ) )
        ->set_render_on( &cfg.m_enabled );

    electrocuted->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll electrocuted shock" ) )
        ->set_render_on( &cfg.m_enabled );

    ash->set_tooltip( ASTRAL_XORSTR_STATIC( "The corpse crumbles to ash a moment after it lands." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "ragdoll ash crumble" ) )
        ->set_render_on( &cfg.m_enabled );
}

static gui::list_box *s_advanced_inventory_list{ };
static usize          s_advanced_inventory_selected{ };
static usize          s_advanced_inventory_count{ };
static usize          s_advanced_inventory_slots[ config::ADVANCED_ITEM_COUNT ]{ };
static char           s_advanced_inventory_text[ config::ADVANCED_ITEM_COUNT ][ 96 ]{ };
static const char    *s_advanced_inventory_labels[ config::ADVANCED_ITEM_COUNT ]{ };
static usize          s_advanced_loadout_class{ };
static usize          s_advanced_loadout_slot{ };
static usize          s_advanced_catalog_selected{ };

static bool advanced_selection_valid( )
{
    return s_advanced_inventory_selected < s_advanced_inventory_count;
}

static usize advanced_selection_slot( )
{
    return advanced_selection_valid( ) ? s_advanced_inventory_slots[ s_advanced_inventory_selected ] :
                                          config::ADVANCED_ITEM_COUNT;
}

static bool advanced_weapon_properties_shown( )
{
    return advanced_selection_valid( ) && !g_skins.advanced_item_is_cosmetic( advanced_selection_slot( ) );
}

static bool advanced_cosmetic_properties_shown( )
{
    return advanced_selection_valid( ) && g_skins.advanced_item_is_cosmetic( advanced_selection_slot( ) );
}

static bool advanced_australium_shown( )
{
    return advanced_weapon_properties_shown( ) &&
           g_skins.advanced_item_australium_capable( advanced_selection_slot( ) );
}

static bool advanced_festive_shown( )
{
    return advanced_weapon_properties_shown( ) &&
           g_skins.advanced_item_festive_capable( advanced_selection_slot( ) );
}

static bool advanced_sheen_shown( )
{
    return advanced_weapon_properties_shown( ) && g_cfg.m_skins.m_weapon_edit.m_killstreak >= 2;
}

static bool advanced_killstreaker_shown( )
{
    return advanced_weapon_properties_shown( ) && g_cfg.m_skins.m_weapon_edit.m_killstreak >= 3;
}

static constexpr const char *ADVANCED_CLASSES[]{
    "Scout", "Sniper", "Soldier", "Demoman", "Medic", "Heavy", "Pyro", "Spy", "Engineer"
};
static constexpr const char *ADVANCED_SLOTS[]{
    "Primary", "Secondary", "Melee", "Utility", "Building", "PDA", "PDA 2", "Head",
    "Misc", "Action", "Misc 2"
};
static constexpr const char *ADVANCED_QUALITIES[]{
    "Normal", "Genuine", "Vintage", "Unusual", "Unique", "Community", "Self-Made",
    "Strange", "Haunted", "Collector's", "Decorated"
};
static constexpr const char *ADVANCED_WEAPON_UNUSUALS[]{
    "None", "Community Sparkle", "Hot", "Isotope", "Cool", "Energy Orb"
};
static constexpr const char *ADVANCED_KILLSTREAKS[]{
    "None", "Killstreak", "Specialized", "Professional"
};
static constexpr const char *ADVANCED_SHEENS[]{
    "None", "Team Shine", "Deadly Daffodil", "Manndarin", "Mean Green",
    "Agonizing Emerald", "Villainous Violet", "Hot Rod"
};
static constexpr const char *ADVANCED_KILLSTREAKERS[]{
    "None", "Fire Horns", "Cerebral Discharge", "Tornado", "Flames", "Singularity",
    "Incinerator", "Hypno-Beam"
};

static void refresh_advanced_inventory( )
{
    s_advanced_inventory_count = 0;
    for ( usize slot{ }; slot < config::ADVANCED_ITEM_COUNT; ++slot )
    {
        const config::skins_t::advanced_item_t &item{ g_cfg.m_skins.m_advanced_items[ slot ] };
        if ( !skins::synthetic_item_id( item.m_id ) )
        {
            continue;
        }

        char *text{ s_advanced_inventory_text[ s_advanced_inventory_count ] };
        const usize quality{ math::smaller( item.m_quality, config::skins_t::QUALITY_COUNT - 1 ) };
        atl::format( text, 96, "#%llu  %s  def %d  class %d  slot %d",
                     static_cast< unsigned long long >( item.m_id & 0xFFFFFFFFull ),
                     ADVANCED_QUALITIES[ quality ], item.m_definition, item.m_player_class, item.m_slot );
        s_advanced_inventory_labels[ s_advanced_inventory_count ] = text;
        s_advanced_inventory_slots[ s_advanced_inventory_count ]  = slot;
        ++s_advanced_inventory_count;
    }

    if ( s_advanced_inventory_selected >= s_advanced_inventory_count )
    {
        s_advanced_inventory_selected = s_advanced_inventory_count ? s_advanced_inventory_count - 1 : 0;
    }
    if ( s_advanced_inventory_list )
    {
        s_advanced_inventory_list->set_items( s_advanced_inventory_labels, s_advanced_inventory_count );
    }
}

static void select_advanced_item_id( u64 id )
{
    for ( usize row{ }; row < s_advanced_inventory_count; ++row )
    {
        const usize slot{ s_advanced_inventory_slots[ row ] };
        if ( g_cfg.m_skins.m_advanced_items[ slot ].m_id == id )
        {
            s_advanced_inventory_selected = row;
            g_skins.load_advanced_appearance( slot );
            return;
        }
    }
}

static void add_advanced_held( )
{
    const u32 serial{ g_cfg.m_skins.m_advanced_next_id };
    const bool added{ g_skins.add_advanced_held( ) };
    refresh_advanced_inventory( );
    if ( added )
    {
        select_advanced_item_id( skins::make_synthetic_item_id( serial ) );
    }
    push_notification( added ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced" ),
                       added ? ASTRAL_XORSTR_STATIC( "Added the held item to the synthetic backpack." )
                             : ASTRAL_XORSTR_STATIC( "Hold a supported item, select Advanced mode, and ensure the backpack has room." ) );
}

static void add_advanced_catalog_selected( )
{
    const u32 serial{ g_cfg.m_skins.m_advanced_next_id };
    const bool added{ g_skins.add_advanced_catalog_item( s_advanced_catalog_selected ) };
    refresh_advanced_inventory( );
    if ( added )
    {
        select_advanced_item_id( skins::make_synthetic_item_id( serial ) );
    }
    push_notification( added ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced catalog" ),
                       added ? ASTRAL_XORSTR_STATIC( "Added the selected TF2 item to the synthetic backpack." )
                             : ASTRAL_XORSTR_STATIC( "Select Advanced mode, choose a catalog item, and ensure the backpack has room." ) );
}

static void remove_advanced_selected( )
{
    const bool valid{ s_advanced_inventory_selected < s_advanced_inventory_count };
    const bool removed{ valid && g_skins.remove_advanced_item(
                                     s_advanced_inventory_slots[ s_advanced_inventory_selected ] ) };
    refresh_advanced_inventory( );
    push_notification( removed ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced" ),
                       removed ? ASTRAL_XORSTR_STATIC( "Removed the synthetic item." )
                               : ASTRAL_XORSTR_STATIC( "Select a synthetic item first." ) );
}

static void load_advanced_selected_appearance( )
{
    const bool valid{ s_advanced_inventory_selected < s_advanced_inventory_count };
    const bool loaded{ valid && g_skins.load_advanced_appearance(
                                    s_advanced_inventory_slots[ s_advanced_inventory_selected ] ) };
    push_notification( loaded ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced appearance" ),
                       loaded ? ASTRAL_XORSTR_STATIC( "Loaded the selected item into the weapon appearance editor." )
                              : ASTRAL_XORSTR_STATIC( "Select a synthetic item in Advanced mode first." ) );
}

static void save_advanced_selected_appearance( )
{
    const bool valid{ s_advanced_inventory_selected < s_advanced_inventory_count };
    const bool saved{ valid && g_skins.save_advanced_appearance(
                                   s_advanced_inventory_slots[ s_advanced_inventory_selected ] ) };
    refresh_advanced_inventory( );
    push_notification( saved ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced appearance" ),
                       saved ? ASTRAL_XORSTR_STATIC( "Saved the editor appearance to the selected synthetic item." )
                             : ASTRAL_XORSTR_STATIC( "Select a synthetic item in Advanced mode first." ) );
}

static void clear_advanced_inventory( )
{
    const bool cleared{ g_skins.clear_advanced_inventory( ) };
    refresh_advanced_inventory( );
    push_notification( cleared ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced" ),
                       cleared ? ASTRAL_XORSTR_STATIC( "Cleared all Astral synthetic items and loadout assignments." )
                               : ASTRAL_XORSTR_STATIC( "Switch to Advanced mode before clearing its inventory." ) );
}

static void equip_advanced_selected( )
{
    const bool valid{ s_advanced_inventory_selected < s_advanced_inventory_count };
    const bool equipped{ valid && g_skins.equip_advanced_item(
                                      s_advanced_inventory_slots[ s_advanced_inventory_selected ],
                                      static_cast< i32 >( s_advanced_loadout_class + 1 ),
                                      static_cast< i32 >( s_advanced_loadout_slot ) ) };
    refresh_advanced_inventory( );
    push_notification( equipped ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced loadout" ),
                       equipped ? ASTRAL_XORSTR_STATIC( "Equipped the synthetic item in the selected class slot." )
                                : ASTRAL_XORSTR_STATIC( "Enable Advanced inventory and select a synthetic item." ) );
}

static void unequip_advanced_selected( )
{
    const bool removed{ g_skins.unequip_advanced_item(
        static_cast< i32 >( s_advanced_loadout_class + 1 ),
        static_cast< i32 >( s_advanced_loadout_slot ) ) };
    refresh_advanced_inventory( );
    push_notification( removed ? notify_kind_t::success : notify_kind_t::warning, ICON_DOLLAR,
                       ASTRAL_XORSTR_STATIC( "Advanced loadout" ),
                       removed ? ASTRAL_XORSTR_STATIC( "Cleared the selected synthetic loadout slot." )
                               : ASTRAL_XORSTR_STATIC( "That slot does not contain an Astral synthetic item." ) );
}

static void build_skins_advanced( gui::tab *skins_tab )
{
    gui::sub_tab   *advanced{ skins_tab->add_subtab( ASTRAL_XORSTR_STATIC( "Advanced" ) ) };
    gui::group_box *backpack{ advanced->add_group( ICON_DOLLAR, ASTRAL_XORSTR_STATIC( "Backpack" ), gui::column_t::left ) };
    gui::group_box *safety{ advanced->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Safety" ), gui::column_t::right ) };
    gui::group_box *loadout{ advanced->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Loadout" ), gui::column_t::right ) };
    gui::group_box *properties{ advanced->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Selected item properties" ), gui::column_t::right ) };
    gui::group_box *catalog{ advanced->add_group( ICON_SEARCH, ASTRAL_XORSTR_STATIC( "TF2 item catalog" ), gui::column_t::left ) };

    backpack->add< gui::label >( ASTRAL_XORSTR_STATIC( "State" ), []( ) -> const char *
    {
        return g_skins.advanced_status( );
    } )->set_pair( );
    refresh_advanced_inventory( );
    s_advanced_inventory_list = backpack->add< gui::list_box >(
        s_advanced_inventory_labels, s_advanced_inventory_count, &s_advanced_inventory_selected );
    gui::label *actions{ backpack->add< gui::label >( ASTRAL_XORSTR_STATIC( "Actions" ), nullptr ) };
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Add held" ), &add_advanced_held );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Remove" ), &remove_advanced_selected );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &refresh_advanced_inventory );
    gui::label *appearance_actions{ backpack->add< gui::label >( ASTRAL_XORSTR_STATIC( "Appearance" ), nullptr ) };
    appearance_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Load editor" ), &load_advanced_selected_appearance );
    appearance_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Save editor" ), &save_advanced_selected_appearance );

    properties->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Quality" ), ADVANCED_QUALITIES,
                                        config::skins_t::QUALITY_COUNT,
                                        &g_cfg.m_skins.m_advanced_edit_quality )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the synthetic item's inventory quality and name colour. Strange selects Strange quality; the displayed counter remains game-managed." ) );
    properties->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Item level" ),
                                         &g_cfg.m_skins.m_advanced_edit_level, 0, 100 );
    auto advanced_australium{ properties->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Australium" ),
                                                                 &g_cfg.m_skins.m_weapon_edit.m_australium ) };
    auto advanced_festive{ properties->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Festivized lights" ),
                                                              &g_cfg.m_skins.m_weapon_edit.m_festivized ) };
    advanced_festive
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Adds festive lights when the selected definition supports them. Original Festive weapons remain separate catalog definitions." ) );
    advanced_australium->set_render_on( &advanced_australium_shown );
    advanced_festive->set_render_on( &advanced_festive_shown );
    auto weapon_unusual{ properties->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Weapon unusual" ),
                                                             ADVANCED_WEAPON_UNUSUALS, skins::UNUSUAL_COUNT,
                                                             &g_cfg.m_skins.m_weapon_edit.m_unusual ) };
    weapon_unusual->set_render_on( &advanced_weapon_properties_shown );
    auto cosmetic_unusual_label{ properties->add< gui::label >( ASTRAL_XORSTR_STATIC( "Cosmetic unusual" ), []( ) -> const char *
    { return ASTRAL_XORSTR_STATIC( "Supported cosmetic effects" ); } ) };
    gui::dynamic_list_box *cosmetic_unusual{ properties->add< gui::dynamic_list_box >(
        &g_cfg.m_skins.m_advanced_edit_cosmetic_unusual ) };
    g_skins.bind_advanced_effect_list( cosmetic_unusual );
    cosmetic_unusual->set_keywords( ASTRAL_XORSTR_STATIC( "cosmetic unusual effect hat burning flames sunbeams" ) );
    cosmetic_unusual_label->set_render_on( &advanced_cosmetic_properties_shown );
    cosmetic_unusual->set_render_on( &advanced_cosmetic_properties_shown );
    properties->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Killstreak tier" ),
                                        ADVANCED_KILLSTREAKS, skins::KILLSTREAK_COUNT,
                                        &g_cfg.m_skins.m_weapon_edit.m_killstreak )
        ->set_render_on( &advanced_weapon_properties_shown );
    properties->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Sheen" ), ADVANCED_SHEENS,
                                        skins::SHEEN_COUNT, &g_cfg.m_skins.m_weapon_edit.m_sheen )
        ->set_render_on( &advanced_sheen_shown );
    properties->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Killstreaker" ),
                                        ADVANCED_KILLSTREAKERS, skins::KILLSTREAKER_COUNT,
                                        &g_cfg.m_skins.m_weapon_edit.m_killstreaker )
        ->set_render_on( &advanced_killstreaker_shown );
    properties->add< gui::label >( ASTRAL_XORSTR_STATIC( "Apply changes" ), []( ) -> const char *
    { return ASTRAL_XORSTR_STATIC( "Load the selected item, edit it here, then Apply properties." ); }, 3 );
    properties->add< gui::button >( ASTRAL_XORSTR_STATIC( "Apply properties" ),
                                     &save_advanced_selected_appearance );

    gui::dynamic_list_box *catalog_list{ catalog->add< gui::dynamic_list_box >(
        &s_advanced_catalog_selected ) };
    g_skins.bind_advanced_catalog( catalog_list );
    catalog_list->set_visible_rows( 12 )
        ->set_action( ASTRAL_XORSTR_STATIC( "Add selected" ), &add_advanced_catalog_selected );
    catalog_list->set_keywords( ASTRAL_XORSTR_STATIC( "inventory schema item weapon cosmetic catalog" ) );

    loadout->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Class" ), ADVANCED_CLASSES,
                                    9, &s_advanced_loadout_class );
    loadout->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Slot" ), ADVANCED_SLOTS,
                                    config::ADVANCED_LOADOUT_SLOTS, &s_advanced_loadout_slot );
    gui::label *loadout_actions{ loadout->add< gui::label >( ASTRAL_XORSTR_STATIC( "Actions" ), nullptr ) };
    loadout_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Equip selected" ), &equip_advanced_selected );
    loadout_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Clear slot" ), &unequip_advanced_selected );

    safety->add< gui::label >( ASTRAL_XORSTR_STATIC( "Isolation" ),
                               []( ) -> const char *
    { return ASTRAL_XORSTR_STATIC( "Only Astral synthetic IDs may be managed. Genuine Steam items always remain on TF2's original paths." ); }, 5 );
    safety->add< gui::button >( ASTRAL_XORSTR_STATIC( "Clear synthetic inventory" ),
                                &clear_advanced_inventory )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Removes only reserved Astral synthetic IDs and their assignments." ) );
}

static void build_skins( )
{
    gui::tab *skins_tab{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Skins" ), ICON_DOLLAR ) };

    build_skins_weapons( skins_tab );
    build_skins_paint( skins_tab );
    build_skins_wearables( skins_tab );
    build_skins_spells( skins_tab );
    build_skins_ragdolls( skins_tab );
    build_skins_advanced( skins_tab );
}

constexpr i32 LUA_IDE_WIDTH{ 600 };

static script::ide     *s_material_ide{ };
static gui::text_input *s_material_name_input{ };
static gui::list_box   *s_material_files{ };
static usize            s_material_file{ };

static constexpr const char *MATERIAL_STRESS_TEST{ R"vmt(// AstralMod custom-chams stress test. Uses only stock TF2 textures.
VertexLitGeneric
{
    "$basetexture" "white"
    "$detail" "effects/tiledfire/fireLayeredSlowTiled512"
    "$detailframe" "0"
    "$detailblendmode" "0"
    "$detailblendfactor" "1.0"
    "$detailscale" "1.35"
    "$envmap" "cubemaps/cubemap_sheen001"
    "$envmaptint" "[0.55 0.18 0.95]"
    "$envmapcontrast" "1"
    "$envmapsaturation" "1"
    "$phong" "1"
    "$phongexponent" "18"
    "$phongboost" "2.5"
    "$phongfresnelranges" "[0.05 0.45 1]"
    "$rimlight" "1"
    "$rimlightexponent" "3"
    "$rimlightboost" "2"
    "$selfillum" "1"
    "$selfillumfresnel" "1"
    "$selfillumfresnelminmaxexp" "[0.05 0.65 2.25]"
    "$selfillumtint" "[0.45 0.08 1.0]"
    "$model" "1"
    "$nodecal" "1"
    "$nofog" "1"
    "Proxies"
    {
        "AnimatedTexture"
        {
            "animatedtexturevar" "$detail"
            "animatedtextureframenumvar" "$detailframe"
            "animatedtextureframerate" "24"
        }
        "TextureScroll"
        {
            "texturescrollvar" "$detailtexturetransform"
            "texturescrollrate" "0.32"
            "texturescrollangle" "35"
        }
        "Sine"
        {
            "sineperiod" "1.6"
            "sinemin" "0.35"
            "sinemax" "1.0"
            "resultvar" "$detailblendfactor"
        }
        "Sine"
        {
            "sineperiod" "2.35"
            "sinemin" "0.8"
            "sinemax" "5.5"
            "resultvar" "$phongboost"
        }
        "Sine"
        {
            "sineperiod" "1.15"
            "sinemin" "0.4"
            "sinemax" "4.0"
            "resultvar" "$rimlightboost"
        }
    }
}
)vmt" };

static char material_name_filter( char value )
{
    value = value == ' ' ? '-' : str::to_lower( value );
    return str::is_alpha( value ) || str::is_digit( value ) || value == '-' || value == '_' ? value : 0;
}

static usize validate_vmt( string_view source )
{
    i32 depth{ };
    usize line{ 1 };
    bool quoted{ };
    bool escaped{ };
    bool shader{ };
    for ( usize index{ }; index < source.size( ); ++index )
    {
        const char c{ source[ index ] };
        if ( c == '\n' ) { ++line; escaped = false; continue; }
        if ( !quoted && c == '/' && index + 1 < source.size( ) && source[ index + 1 ] == '/' )
        {
            while ( index < source.size( ) && source[ index ] != '\n' ) ++index;
            if ( index < source.size( ) ) ++line;
            continue;
        }
        if ( c == '"' && !escaped ) { quoted = !quoted; if ( !depth ) shader = true; }
        escaped = quoted && c == '\\' && !escaped;
        if ( quoted ) continue;
        if ( c == '{' ) { ++depth; shader = true; }
        else if ( c == '}' && --depth < 0 ) return line;
        else if ( !str::is_space( c ) && !depth ) shader = true;
    }
    return quoted || depth != 0 || !shader ? line : 0;
}

static void material_notify( notify_kind_t kind, const char *text )
{
    push_notification( kind, ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Materials" ), text );
}

static void material_refresh( )
{
    g_chams.refresh_custom_materials( );
    if ( s_material_files ) s_material_files->set_items( g_chams.custom_material_labels( ), g_chams.custom_material_count( ) );
    refresh_custom_cham_slots( );
}

static void material_open( )
{
    atl::vector< char > source{ };
    if ( !s_material_ide || s_material_file >= g_chams.custom_material_count( ) ||
         !g_chams.read_custom_material( s_material_file, source ) )
    {
        material_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Select a material first" ) );
        return;
    }
    const string_view name{ g_chams.custom_material_labels( )[ s_material_file ] };
    s_material_ide->open( name, string_view{ source.data( ), source.size( ) } );
    s_material_name_input->set_value( name );
}

static void material_new( )
{
    if ( !s_material_ide ) return;
    s_material_ide->open( { }, ASTRAL_XORSTR_STATIC(
        "VertexLitGeneric\n{\n    \"$basetexture\" \"vgui/white_additive\"\n    \"$model\" \"1\"\n    \"$nodecal\" \"1\"\n}\n" ) );
    s_material_name_input->set_value( { } );
}

static void material_stress_test( )
{
    if ( !s_material_ide || !s_material_name_input ) return;
    s_material_ide->open( ASTRAL_XORSTR_STATIC( "astral-prismatic-storm" ), MATERIAL_STRESS_TEST );
    s_material_name_input->set_value( ASTRAL_XORSTR_STATIC( "astral-prismatic-storm" ) );
}

static bool material_save_impl( bool apply )
{
    if ( !s_material_ide || !s_material_name_input ) return false;
    const string_view name{ s_material_name_input->value( ).empty( )
                                ? s_material_ide->name( ) : s_material_name_input->value( ) };
    if ( name.empty( ) )
    {
        material_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Give the material a name first" ) );
        return false;
    }
    const usize bad_line{ validate_vmt( s_material_ide->text( ) ) };
    if ( bad_line )
    {
        s_material_ide->mark_line( bad_line );
        material_notify( notify_kind_t::error, ASTRAL_XORSTR_STATIC( "Invalid VMT structure; check the highlighted line" ) );
        return false;
    }
    if ( !g_chams.save_custom_material( name, s_material_ide->text( ) ) )
    {
        material_notify( notify_kind_t::error, ASTRAL_XORSTR_STATIC( "Could not save the material file" ) );
        return false;
    }
    s_material_ide->saved( name );
    material_refresh( );
    if ( apply )
    {
        g_chams.reload_custom_material( );
    }
    material_notify( notify_kind_t::success, apply ? ASTRAL_XORSTR_STATIC( "Material saved and applied" )
                                                    : ASTRAL_XORSTR_STATIC( "Material saved" ) );
    return true;
}

static void material_save( ) { material_save_impl( false ); }
static void material_apply( ) { material_save_impl( true ); }
static void material_close( ) { }
static void material_clear( ) { }

static bool material_has_files( ) { return g_chams.custom_material_count( ) != 0; }
static bool material_no_files( ) { return !material_has_files( ); }
static const char *material_empty_text( ) { return ASTRAL_XORSTR_STATIC( "No custom .vmt files saved yet" ); }

static void build_material_editor( )
{
    gui::tab *tab{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Materials" ), ICON_SPARKLES ) };
    gui::sub_tab *editor{ tab->add_subtab( ASTRAL_XORSTR_STATIC( "Editor" ) ) };
    gui::group_box *files{ editor->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Material files" ), gui::column_t::left ) };
    material_refresh( );
    s_material_files = files->add< gui::list_box >( g_chams.custom_material_labels( ), g_chams.custom_material_count( ), &s_material_file );
    s_material_files->set_render_on( &material_has_files );
    files->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ), &material_empty_text )->set_render_on( &material_no_files );
    gui::label *actions{ files->add< gui::label >( ASTRAL_XORSTR_STATIC( "File" ), nullptr ) };
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "New" ), &material_new );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Stress test" ), &material_stress_test );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Open" ), &material_open );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &material_refresh );

    gui::group_box *inspector{ editor->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Material" ), gui::column_t::right ) };
    s_material_name_input = inspector->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Material name" ) );
    s_material_name_input->set_filter( &material_name_filter )->set_limit( 63 );
    inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Format" ), []() -> const char * { return ASTRAL_XORSTR_STATIC( "Source VMT / KeyValues" ); } )->set_pair( );
    inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Storage" ), []() -> const char * { return ASTRAL_XORSTR_STATIC( "AstralMod/Materials" ); } )->set_pair( );

    s_material_ide = editor->set_panel< script::ide >( ICON_CODE, ASTRAL_XORSTR_STATIC( "VMT editor" ), LUA_IDE_WIDTH,
                                                       &material_save, &material_apply, &material_close, &material_clear,
                                                       ASTRAL_XORSTR_STATIC( "Apply" ), ASTRAL_XORSTR_STATIC( "Close" ) );
    s_material_ide->set_language( gui::code_editor::language_t::vmt );
    material_new( );
}

static script::ide     *s_lua_ide{ };
static gui::text_input *s_lua_name_input{ };
static gui::list_box   *s_lua_editor_files{ };
static gui::list_box   *s_lua_settings_files{ };
static gui::text_input *s_lua_unrestricted_ack{ };
static usize            s_lua_editor_file{ };
static usize            s_lua_settings_file{ };
static bool             s_lua_unrestricted_requested{ };

static bool lua_has_files( )
{
    return g_scripts.file_count( ) != 0;
}

static bool lua_no_files( )
{
    return g_scripts.file_count( ) == 0;
}

static const char *lua_no_files_text( )
{
    return ASTRAL_XORSTR_STATIC( "No scripts saved yet" );
}

static char lua_name_filter( char value )
{
    if ( value == ' ' )
    {
        return '-';
    }

    value = str::to_lower( value );
    return script::valid_name( string_view{ &value, 1 } ) ? value : 0;
}

static string_view lua_file( usize index )
{
    return index < g_scripts.file_count( ) ? string_view{ g_scripts.file_names( )[ index ] } : string_view{ };
}

static void lua_notify( notify_kind_t kind, const char *message )
{
    push_notification( kind, ICON_CODE, ASTRAL_XORSTR_STATIC( "Lua" ), message );
}

static void lua_refresh( )
{
    g_scripts.request_store( script::store_kind_t::list );
}

static void poll_lua( )
{
    if ( g_scripts.unrestricted( ) && !s_lua_unrestricted_requested )
    {
        g_scripts.set_unrestricted( false );
        if ( s_lua_unrestricted_ack )
        {
            s_lua_unrestricted_ack->set_value( { } );
        }
        lua_notify( notify_kind_t::success, ASTRAL_XORSTR_STATIC( "Restricted Lua restored; all scripts were unloaded" ) );
    }

    script::store_result_t result{ };

    if ( !g_scripts.poll_store( result ) )
    {
        return;
    }

    s_lua_editor_files->set_items( g_scripts.file_names( ), g_scripts.file_count( ) );
    s_lua_settings_files->set_items( g_scripts.file_names( ), g_scripts.file_count( ) );

    if ( !result.m_success )
    {
        return;
    }

    switch ( result.m_kind )
    {
        case script::store_kind_t::open:
            s_lua_ide->open( result.m_name.view( ), string_view{ result.m_source.data( ), result.m_source.size( ) } );
            s_lua_name_input->set_value( result.m_name.view( ) );
            break;

        case script::store_kind_t::save:
            s_lua_ide->saved( result.m_name.view( ) );
            lua_notify( notify_kind_t::success, ASTRAL_XORSTR_STATIC( "Script saved" ) );
            break;

        default: break;
    }
}

static bool lua_unrestricted_confirmation( )
{
    return s_lua_unrestricted_requested && !g_scripts.unrestricted( );
}

static const char *lua_security_state_text( )
{
    return g_scripts.unrestricted( ) ? ASTRAL_XORSTR_STATIC( "UNRESTRICTED (unsafe)" ) : ASTRAL_XORSTR_STATIC( "Restricted" );
}

static void lua_confirm_unrestricted( )
{
    if ( !s_lua_unrestricted_ack || s_lua_unrestricted_ack->value( ) != string_view{ "I UNDERSTAND THE RISK" } )
    {
        lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Type I UNDERSTAND THE RISK exactly" ) );
        return;
    }

    g_scripts.set_unrestricted( true );
    s_lua_unrestricted_ack->set_value( { } );
    lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Unrestricted Lua enabled for this session; all scripts were unloaded" ) );
}

static string_view lua_editor_name( )
{
    const string_view entered{ s_lua_name_input->value( ) };
    return !entered.empty( ) ? entered : s_lua_ide->name( );
}

static void lua_open( )
{
    const string_view name{ lua_file( s_lua_editor_file ) };

    if ( name.empty( ) )
    {
        lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Select a script first" ) );
        return;
    }

    g_scripts.request_store( script::store_kind_t::open, name );
}

static void lua_new( )
{
    s_lua_ide->create( );
    s_lua_name_input->set_value( { } );
}

static void lua_save( )
{
    const string_view name{ lua_editor_name( ) };

    if ( !script::valid_name( name ) )
    {
        lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Give the script a name first" ) );
        return;
    }

    g_scripts.request_store( script::store_kind_t::save, name, s_lua_ide->text( ) );
}

static void lua_run( )
{
    const string_view name{ lua_editor_name( ) };

    if ( !script::valid_name( name ) )
    {
        lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Give the script a name first" ) );
        return;
    }

    s_lua_ide->run( name );
}

static void lua_stop( )
{
    s_lua_ide->stop( );
}

static void lua_clear_console( )
{
    g_scripts.clear_log( );
}

static void lua_reload( )
{
    script::name_t name{ };
    name.assign( s_lua_ide->name( ) );

    if ( name.empty( ) )
    {
        lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Save the script first" ) );
        return;
    }

    g_scripts.request_store( script::store_kind_t::open, name.view( ) );
}

static void lua_load( )
{
    const string_view name{ lua_file( s_lua_settings_file ) };

    if ( name.empty( ) )
    {
        lua_notify( notify_kind_t::warning, ASTRAL_XORSTR_STATIC( "Select a script first" ) );
        return;
    }

    g_scripts.request_store( script::store_kind_t::load, name );
}

static void lua_unload( )
{
    const string_view name{ lua_file( s_lua_settings_file ) };

    if ( !name.empty( ) )
    {
        g_scripts.request_unload( name );
    }
}

static const char *lua_file_text( )
{
    static char text[ script::NAME_LENGTH + 8 ]{ };

    const string_view name{ s_lua_ide->name( ) };

    if ( name.empty( ) )
    {
        return ASTRAL_XORSTR_STATIC( "Unsaved" );
    }

    str::copy( text, sizeof( text ), name.data( ), name.size( ) );
    str::append( text, sizeof( text ), ASTRAL_XORSTR( ".lua" ).get( ) );
    return text;
}

static const char *lua_lines_text( )
{
    static char text[ 16 ]{ };

    str::format_to( text, sizeof( text ), "{}", s_lua_ide->lines( ) );
    return text;
}

static const char *lua_state_text( )
{
    static char text[ 32 ]{ };

    if ( const usize line{ s_lua_ide->marked_line( ) } )
    {
        str::format_to( text, sizeof( text ), ASTRAL_XORSTR( "Error on line {}" ).get( ), line );
        return text;
    }

    const string_view name{ s_lua_ide->name( ) };
    return !name.empty( ) && g_scripts.is_loaded( name ) ? ASTRAL_XORSTR_STATIC( "Running" ) : ASTRAL_XORSTR_STATIC( "Stopped" );
}

static const char *lua_loaded_text( )
{
    static char text[ 256 ]{ };

    script::name_t names[ script::SCRIPT_CAPACITY ]{ };
    const usize    count{ g_scripts.loaded_names( names, script::SCRIPT_CAPACITY ) };

    str::copy( text, sizeof( text ), count ? ASTRAL_XORSTR( "Running: " ).get( ) : ASTRAL_XORSTR( "No script running" ).get( ) );

    for ( usize index{ }; index < count; ++index )
    {
        if ( index )
        {
            str::append( text, sizeof( text ), ASTRAL_XORSTR( ", " ).get( ) );
        }

        str::append( text, sizeof( text ), names[ index ].c_str( ) );
    }

    return text;
}

static void build_lua( )
{
    gui::tab     *lua{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Lua" ), ICON_CODE ) };
    gui::sub_tab *editor{ lua->add_subtab( ASTRAL_XORSTR_STATIC( "Editor" ) ) };

    gui::group_box *files{ editor->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Scripts" ), gui::column_t::left ) };

    s_lua_editor_files = files->add< gui::list_box >( g_scripts.file_names( ), g_scripts.file_count( ), &s_lua_editor_file );

    gui::label *empty{ files->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ), &lua_no_files_text ) };
    gui::label *actions{ files->add< gui::label >( ASTRAL_XORSTR_STATIC( "File" ), nullptr ) };

    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "New" ), &lua_new );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Open" ), &lua_open );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &lua_refresh );

    s_lua_editor_files->set_render_on( &lua_has_files );
    empty->set_render_on( &lua_no_files );

    gui::group_box *inspector{ editor->add_group( ICON_ZAP, ASTRAL_XORSTR_STATIC( "Script" ), gui::column_t::right ) };

    s_lua_name_input = inspector->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Script name" ) );
    inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "File" ), &lua_file_text )->set_pair( );
    inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Lines" ), &lua_lines_text )->set_pair( );
    inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "State" ), &lua_state_text )->set_pair( );

    gui::label    *control{ inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Script" ), nullptr ) };
    gui::checkbox *dock{ inspector->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Snap editor to menu" ), &g_cfg.m_misc.m_dock_editor ) };
    gui::checkbox *unrestricted{ inspector->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Unrestricted Lua" ),
                                                                  &s_lua_unrestricted_requested ) };
    inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Security" ), &lua_security_state_text )->set_pair( );
    gui::label *risk{ inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Warning" ), []() -> const char * {
        return ASTRAL_XORSTR_STATIC( "Scripts can read, write, rename, and delete files\nand access environment data.\nOnly enable this for code you trust." );
    }, 3 ) };
    s_lua_unrestricted_ack = inspector->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Type I UNDERSTAND THE RISK" ) );
    gui::label *enable_unrestricted{ inspector->add< gui::label >( ASTRAL_XORSTR_STATIC( "Confirmation" ), nullptr ) };
    enable_unrestricted->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Enable for this session" ), &lua_confirm_unrestricted );

    risk->set_render_on( &lua_unrestricted_confirmation );
    s_lua_unrestricted_ack->set_render_on( &lua_unrestricted_confirmation )->set_limit( 26 );
    enable_unrestricted->set_render_on( &lua_unrestricted_confirmation );

    s_lua_name_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Save" ), &lua_save );
    control->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Run" ), &lua_run );
    control->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Stop" ), &lua_stop );
    control->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Reload" ), &lua_reload )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Throws away unsaved changes and reads the file again." ) );

    s_lua_name_input->set_filter( &lua_name_filter )
        ->set_limit( script::NAME_LENGTH )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "The file the editor saves to and the name the script runs under. "
                                            "Leave it empty to keep the opened file's name." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "lua script editor ide save name" ) );

    control->set_tooltip( ASTRAL_XORSTR_STATIC( "Runs what is in the editor, saved or not. Running it again replaces "
                                               "the copy that is already running." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "lua script run stop execute" ) );

    dock->set_tooltip( ASTRAL_XORSTR_STATIC( "Keeps the editor beside the menu. Turn it off to drag the editor "
                                            "anywhere by its header." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "lua editor dock snap drag move" ) );

    unrestricted->set_tooltip( ASTRAL_XORSTR_STATIC( "Session-only unsafe mode. Requires typed confirmation and unloads every running script whenever the mode changes. It is never saved to a config." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "lua unrestricted unsafe os io require loadfile dofile security" ) );

    s_lua_ide = editor->set_panel< script::ide >( ICON_CODE, ASTRAL_XORSTR_STATIC( "Editor" ), LUA_IDE_WIDTH, &lua_save, &lua_run,
                                                  &lua_stop, &lua_clear_console );
    s_lua_ide->create( );
}

static gui::text_input *s_config_name_input{ };
static gui::text_input *s_menu_user_input{ };
static gui::text_input *s_streamer_alias_input{ };
static gui::text_input *s_ui_font_input{ };

inline constexpr usize BIND_EDITOR_CAPACITY{ 128 };
static gui::list_box   *s_bind_editor_list{ };
static gui::text_input *s_bind_editor_name{ };
static gui::keybind_view_t s_bind_editor_entries[ BIND_EDITOR_CAPACITY ]{ };
static char            s_bind_editor_labels[ BIND_EDITOR_CAPACITY ][ MENU_LABEL_LENGTH ]{ };
static const char     *s_bind_editor_items[ BIND_EDITOR_CAPACITY ]{ };
static usize           s_bind_editor_count{ };
static usize           s_bind_editor_selected{ };
static usize           s_bind_editor_loaded{ static_cast< usize >( -1 ) };

static void refresh_bind_editor( )
{
    s_bind_editor_count = gui::collect_keybinds( s_bind_editor_entries,
                                                  BIND_EDITOR_CAPACITY, true, true );
    for ( usize index{ }; index < s_bind_editor_count; ++index )
    {
        const string_view variable{ s_bind_editor_entries[ index ].m_variable };
        const string_view feature{ s_bind_editor_entries[ index ].m_feature };
        const string_view label{ variable.empty( ) ? feature : variable };
        str::copy( s_bind_editor_labels[ index ], sizeof( s_bind_editor_labels[ index ] ),
                   label.data( ), label.size( ) );
        s_bind_editor_items[ index ] = s_bind_editor_labels[ index ];
    }

    if ( s_bind_editor_selected >= s_bind_editor_count )
    {
        s_bind_editor_selected = 0;
    }
    s_bind_editor_loaded = static_cast< usize >( -1 );
    if ( s_bind_editor_list )
    {
        s_bind_editor_list->set_items( s_bind_editor_items, s_bind_editor_count );
    }
}

static void apply_bind_display_name( )
{
    if ( !s_bind_editor_name || s_bind_editor_selected >= s_bind_editor_count )
    {
        return;
    }
    bind_t *bind{ s_bind_editor_entries[ s_bind_editor_selected ].m_bind };
    str::copy( bind->m_display_name, sizeof( bind->m_display_name ),
               s_bind_editor_name->value( ).data( ), s_bind_editor_name->value( ).size( ) );
}

static void clear_bind_display_name( )
{
    if ( s_bind_editor_selected >= s_bind_editor_count )
    {
        return;
    }
    s_bind_editor_entries[ s_bind_editor_selected ].m_bind->m_display_name[ 0 ] = 0;
    if ( s_bind_editor_name )
    {
        s_bind_editor_name->set_value( { } );
    }
}

static void sync_bind_editor( )
{
    if ( !s_bind_editor_name || s_bind_editor_selected >= s_bind_editor_count ||
         g_gui.focused_input( ) == s_bind_editor_name ||
         s_bind_editor_loaded == s_bind_editor_selected )
    {
        return;
    }

    s_bind_editor_name->set_value(
        string_view{ s_bind_editor_entries[ s_bind_editor_selected ].m_bind->m_display_name } );
    s_bind_editor_loaded = s_bind_editor_selected;
}
static gui::slider_int *s_ui_scale_slider{ };
static i32              s_ui_scale_pending{ 100 };
static gui::list_box   *s_notification_log_list{ };
static usize            s_notification_log_selection{ };
static u32              s_notification_log_revision{ ~0u };
static usize            s_notification_log_count{ };
static char             s_notification_log_labels[ 128 ][ 672 ]{ };
static const char      *s_notification_log_items[ 128 ]{ };

namespace loader_profile
{
    constexpr wchar_t PIPE_NAME[]{ L"\\\\.\\pipe\\AstralMod.Profile.v1" };
    constexpr u32 HELLO_MAGIC{ 0x4F4C5341u };
    constexpr u32 PROFILE_MAGIC{ 0x46505341u };
    constexpr u32 ACK_MAGIC{ 0x4B435341u };
    constexpr u16 VERSION{ 1 };
    constexpr u16 AVATAR_SIZE{ 64 };

#pragma pack(push, 1)
    struct hello_t
    {
        u32 m_magic;
        u16 m_version;
        u16 m_size;
        u32 m_process_id;
    };

    struct profile_t
    {
        u32 m_magic;
        u16 m_version;
        u16 m_size;
        u16 m_avatar_size;
        u16 m_flags;
        char m_username[ 64 ];
        u8   m_avatar[ AVATAR_SIZE * AVATAR_SIZE * 4 ];
    };

    // Sent back through the same pipe connection once the profile is applied:
    // "I got the info and I am using it now." Flags: 1 = username applied,
    // 2 = avatar applied. Lets the loader close fully once it lands.
    struct ack_t
    {
        u32 m_magic;
        u16 m_version;
        u16 m_size;
        u32 m_process_id;
        u16 m_flags;
    };
#pragma pack(pop)

    static bool s_complete{ };
    static u64  s_last_attempt{ };
    static char s_username[ 64 ]{ };
    static bool s_avatar{ };

    static bool legacy_default_name( string_view name )
    {
        return name.empty( ) || name == string_view{ ASTRAL_XORSTR_STATIC( "User" ) } ||
               name == string_view{ g_api.user( ) };
    }

    // Reassert the runtime identity after config/menu initialization. The
    // loader profile is deliberately not a config field: loading a preset
    // must not silently replace it with the old API/Steam defaults.
    static void refresh_status( )
    {
        if ( !s_complete || !s_username[ 0 ] ) return;

#if defined( ASTRAL_DEV )
        constexpr const char *BUILD{ "Developer" };
#else
        constexpr const char *BUILD{ "Release" };
#endif
        const string_view configured{ g_cfg.m_misc.m_menu_user };
        const string_view shown{ legacy_default_name( configured ) ? string_view{ s_username } : configured };
        if ( s_avatar ) g_gui.set_profile_status( BUILD, shown );
        else g_gui.set_status( BUILD, shown, g_steam.local_account_id( ) );
    }

    // Returns the applied-usage flags for the confirmation ack: 1 = username
    // applied, 2 = avatar applied, 0 = nothing to use (empty profile).
    static u16 apply( profile_t &profile )
    {
        profile.m_username[ sizeof( profile.m_username ) - 1 ] = 0;
        const string_view received{ profile.m_username };
        if ( received.empty( ) ) return 0;

        str::copy( s_username, sizeof( s_username ), received.data( ), received.size( ) );

        u16 used{ };
        const string_view configured{ g_cfg.m_misc.m_menu_user };
        if ( legacy_default_name( configured ) )
        {
            // Discord is the runtime default, not a saved custom alias. Keep
            // the config at User/legacy-default so direct injection can still
            // fall back cleanly when no loader is present next time.
            used |= 1;
        }

        s_avatar = ( profile.m_flags & 1 ) && profile.m_avatar_size == AVATAR_SIZE &&
                   g_render.upload_icon_astral( render::icons::KIND_PROFILE,
                                               profile.m_avatar, AVATAR_SIZE, 1, true );
        if ( s_avatar ) used |= 2;
        s_complete = true;
        refresh_status( );

        // Make a successful loader hand-off visible; without the loader the
        // fallback above stays silent so direct injection is unchanged.
        char notice[ 128 ]{ };
        str::format_to( notice, sizeof( notice ),
                        s_avatar ? ASTRAL_XORSTR( "Discord profile received for {}." ).get( )
                                 : ASTRAL_XORSTR( "Discord name received for {}." ).get( ),
                        received );
        push_notification( notify_kind_t::success, notify_channel_t::system, ICON_USER,
                           ASTRAL_XORSTR_STATIC( "Loader profile" ), notice );
        return used;
    }

    static void poll( )
    {
        if ( s_complete || !g_gui.open( ) ) return;

        // The loader serves its side of the bridge for as long as the game runs,
        // so one cheap pipe probe every 400 ms while the menu is open is all
        // that is needed to catch it whenever the menu is finally opened.
        // Direct injection simply never finds the pipe.
        const u64 now{ GetTickCount64( ) };
        if ( now - s_last_attempt < 400 ) return;
        s_last_attempt = now;

        HANDLE pipe{ CreateFileW( PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                  OPEN_EXISTING, 0, nullptr ) };
        if ( pipe == INVALID_HANDLE_VALUE ) return;

        DWORD mode{ PIPE_READMODE_MESSAGE };
        SetNamedPipeHandleState( pipe, &mode, nullptr, nullptr );
        const hello_t hello{ HELLO_MAGIC, VERSION, sizeof( hello_t ), GetCurrentProcessId( ) };
        profile_t profile{ };
        DWORD written{ }, read{ };
        const bool ok{ WriteFile( pipe, &hello, sizeof( hello ), &written, nullptr ) &&
                       written == sizeof( hello ) &&
                       ReadFile( pipe, &profile, sizeof( profile ), &read, nullptr ) &&
                       read == sizeof( profile ) };

        if ( ok && profile.m_magic == PROFILE_MAGIC && profile.m_version == VERSION &&
             profile.m_size == sizeof( profile_t ) )
        {
            // Apply before confirming: the ack must mean the info is in use.
            const u16 used{ apply( profile ) };
            // "Got the info and I'm using it now" - lets the loader close fully.
            const ack_t ack{ ACK_MAGIC, VERSION, sizeof( ack_t ), GetCurrentProcessId( ), used };
            DWORD acked{ };
            WriteFile( pipe, &ack, sizeof( ack ), &acked, nullptr );
        }
        CloseHandle( pipe );
    }
}

static const char *notification_log_detail_title( )
{
    return s_notification_log_selection < notification_log_count( )
               ? notification_log_title( s_notification_log_selection )
               : ASTRAL_XORSTR_STATIC( "Select an entry" );
}

static const char *notification_log_detail_message( )
{
    return s_notification_log_selection < notification_log_count( )
               ? notification_log_message( s_notification_log_selection )
               : ASTRAL_XORSTR_STATIC( "Notification details appear here." );
}

static const char *notification_log_detail_kind( )
{
    static const char *const KINDS[]{ ASTRAL_XORSTR_STATIC( "Success" ),
                                      ASTRAL_XORSTR_STATIC( "Information" ),
                                      ASTRAL_XORSTR_STATIC( "Warning" ),
                                      ASTRAL_XORSTR_STATIC( "Error" ) };
    const usize              kind{ s_notification_log_selection < notification_log_count( )
                                       ? static_cast< usize >( notification_log_kind( s_notification_log_selection ) )
                                       : 1 };
    return KINDS[ kind < 4 ? kind : 1 ];
}

static const char *notification_log_detail_channel( )
{
    static const char *const CHANNELS[]{ ASTRAL_XORSTR_STATIC( "System" ),
                                         ASTRAL_XORSTR_STATIC( "Players" ),
                                         ASTRAL_XORSTR_STATIC( "Configs" ),
                                         ASTRAL_XORSTR_STATIC( "Lua" ),
                                         ASTRAL_XORSTR_STATIC( "ConVars" ),
                                         ASTRAL_XORSTR_STATIC( "Audio" ),
                                         ASTRAL_XORSTR_STATIC( "Gameplay / network" ),
                                         ASTRAL_XORSTR_STATIC( "Visuals" ) };
    const usize              channel{ s_notification_log_selection < notification_log_count( )
                                          ? static_cast< usize >( notification_log_channel( s_notification_log_selection ) )
                                          : 0 };
    return CHANNELS[ channel < static_cast< usize >( notify_channel_t::count ) ? channel : 0 ];
}

static void refresh_notification_log( )
{
    const u32 revision{ notification_log_revision( ) };
    if ( revision == s_notification_log_revision )
        return;
    s_notification_log_revision = revision;
    s_notification_log_count    = math::smaller( notification_log_count( ), static_cast< usize >( 128 ) );

    static const char *const KINDS[]{ "SUCCESS", "INFO", "WARNING", "ERROR" };
    static const char *const CHANNELS[]{ "SYSTEM", "PLAYERS", "CONFIG", "LUA", "CONVARS",
                                         "AUDIO", "GAMEPLAY", "VISUALS" };
    for ( usize index{ }; index < s_notification_log_count; ++index )
    {
        const usize kind{ static_cast< usize >( notification_log_kind( index ) ) };
        const usize channel{ static_cast< usize >( notification_log_channel( index ) ) };
        str::format_to( s_notification_log_labels[ index ], sizeof( s_notification_log_labels[ index ] ),
                        ASTRAL_XORSTR( "[{} / {}] {} - {}" ).get( ),
                        kind < 4 ? KINDS[ kind ] : KINDS[ 1 ],
                        channel < static_cast< usize >( notify_channel_t::count )
                            ? CHANNELS[ channel ]
                            : CHANNELS[ 0 ],
                        notification_log_title( index ), notification_log_message( index ) );
        s_notification_log_items[ index ] = s_notification_log_labels[ index ];
    }
    if ( s_notification_log_selection >= s_notification_log_count )
        s_notification_log_selection = 0;
    if ( s_notification_log_list )
    {
        s_notification_log_list->set_items( s_notification_log_items, s_notification_log_count );
    }
}

static void clear_notification_history( )
{
    clear_notification_log( );
    refresh_notification_log( );
}
static gui::list_box *s_config_picker{ };
static usize          s_selected_config{ };

static bool has_configs( )
{
    return configs::count( ) != 0;
}

static bool no_configs( )
{
    return configs::count( ) == 0;
}

static const char *no_configs_text( )
{
    return ASTRAL_XORSTR_STATIC( "No saved configs" );
}

static char config_name_filter( char value )
{
    if ( value >= 'A' && value <= 'Z' )
    {
        return static_cast< char >( value - 'A' + 'a' );
    }

    if ( value == ' ' )
    {
        return '-';
    }

    return configs::valid_name( string_view{ &value, 1 } ) ? value : 0;
}

static void apply_profile_names( )
{
    if ( s_menu_user_input )
    {
        const string_view value{ s_menu_user_input->value( ) };
        atl::memset( g_cfg.m_misc.m_menu_user, 0, sizeof( g_cfg.m_misc.m_menu_user ) );
        str::copy( g_cfg.m_misc.m_menu_user, sizeof( g_cfg.m_misc.m_menu_user ),
                   value.empty( ) ? ASTRAL_XORSTR_STATIC( "User" ) : value.data( ),
                   value.empty( ) ? 4 : value.size( ) );
    }
    if ( s_streamer_alias_input )
    {
        const string_view value{ s_streamer_alias_input->value( ) };
        atl::memset( g_cfg.m_streamer_mode.m_local_alias, 0,
                     sizeof( g_cfg.m_streamer_mode.m_local_alias ) );
        str::copy( g_cfg.m_streamer_mode.m_local_alias,
                   sizeof( g_cfg.m_streamer_mode.m_local_alias ),
                   value.empty( ) ? ASTRAL_XORSTR_STATIC( "You" ) : value.data( ),
                   value.empty( ) ? 3 : value.size( ) );
    }
#if defined( ASTRAL_DEV )
    g_gui.set_status( ASTRAL_XORSTR_STATIC( "Developer" ), g_cfg.m_misc.m_menu_user,
                      g_steam.local_account_id( ) );
#else
    g_gui.set_status( ASTRAL_XORSTR_STATIC( "Release" ), g_cfg.m_misc.m_menu_user,
                      g_steam.local_account_id( ) );
#endif
    loader_profile::refresh_status( );
}

static void sync_profile_name_inputs( )
{
    if ( s_menu_user_input )
    {
        s_menu_user_input->set_value( g_cfg.m_misc.m_menu_user[ 0 ]
                                          ? string_view{ g_cfg.m_misc.m_menu_user }
                                          : string_view{ ASTRAL_XORSTR_STATIC( "User" ) } );
    }
    if ( s_streamer_alias_input )
    {
        s_streamer_alias_input->set_value( g_cfg.m_streamer_mode.m_local_alias[ 0 ]
                                               ? string_view{ g_cfg.m_streamer_mode.m_local_alias }
                                               : string_view{ ASTRAL_XORSTR_STATIC( "You" ) } );
    }
    apply_profile_names( );
}

static void save_config( )
{
    const string_view entered{ s_config_name_input->value( ) };
    const string_view name{ !entered.empty( ) ? entered : configs::has_name( s_selected_config ) ? string_view{ configs::names( )[ s_selected_config ] }
                                                                                                 : string_view{ } };
    configs::request( configs::action::save, name );
}

static void quick_save_config( )
{
    const char *name{ configs::loaded_name( ) };
    if ( !name )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE,
                           ASTRAL_XORSTR_STATIC( "Configs" ),
                           ASTRAL_XORSTR_STATIC( "Load or create a config first" ) );
        return;
    }
    configs::request( configs::action::save, name );
}

static void load_config( )
{
    if ( !configs::has_name( s_selected_config ) )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ),
                           ASTRAL_XORSTR_STATIC( "Select a config first" ) );
        return;
    }

    configs::request( configs::action::load, configs::names( )[ s_selected_config ] );
}

static void delete_config( )
{
    if ( !configs::has_name( s_selected_config ) )
    {
        push_notification( notify_kind_t::warning, ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ),
                           ASTRAL_XORSTR_STATIC( "Select a config first" ) );
        return;
    }

    configs::request( configs::action::remove, configs::names( )[ s_selected_config ] );
}

static void refresh_configs( )
{
    configs::request( configs::action::list, { } );
}

static gui::list_box *s_mascot_list{ };
static gui::list_box *s_player_card_image_list{ };
static bool mascot_has_images( ) { return g_menu_mascot.count( ) != 0; }
static bool mascot_no_images( ) { return g_menu_mascot.count( ) == 0; }
static bool classic_menu_style( ) { return g_cfg.m_misc.m_menu_style == gui::MENU_STYLE_CLASSIC; }
static const char *mascot_empty_text( )
{
    return ASTRAL_XORSTR_STATIC( "Put .png, .jpg, .gif, .webp or .bmp files in AstralMod/Mascots" );
}
static void refresh_mascots( )
{
    g_menu_mascot.refresh( );
    if ( s_mascot_list ) s_mascot_list->set_items( g_menu_mascot.labels( ), g_menu_mascot.count( ) );
}

static bool player_card_has_images( ) { return g_player_card_image.count( ) != 0; }
static bool player_card_uses_custom_image( )
{
    return g_cfg.m_misc.m_astral_sense_player_card_custom_image && player_card_has_images( );
}
static void refresh_player_card_images( )
{
    g_player_card_image.refresh( );
    if ( s_player_card_image_list )
        s_player_card_image_list->set_items( g_player_card_image.labels( ), g_player_card_image.count( ) );
    if ( g_player_card_image.count( ) )
        g_cfg.m_misc.m_astral_sense_player_card_image = math::smaller(
            g_cfg.m_misc.m_astral_sense_player_card_image, g_player_card_image.count( ) - 1 );
}

static bool halloween_particle_season( )
{
    SYSTEMTIME local_time{ };
    GetLocalTime( &local_time );
    return local_time.wMonth == 10 || ( local_time.wMonth == 11 && local_time.wDay <= 2 );
}

static void build_settings( )
{
    gui::tab *settings{ g_gui.add_tab( ASTRAL_XORSTR_STATIC( "Settings" ), ICON_SETTINGS, true ) };

    gui::sub_tab *general{ settings->add_subtab( ASTRAL_XORSTR_STATIC( "General" ) ) };

    gui::group_box *appearance{ general->add_group( ICON_SPARKLES,
                                                    ASTRAL_XORSTR_STATIC( "Appearance" ),
                                                    gui::column_t::left ) };

    static const char *const PARTICLE_KINDS[]{ ASTRAL_XORSTR_STATIC( "Off" ),
                                               ASTRAL_XORSTR_STATIC( "Snow" ),
                                               ASTRAL_XORSTR_STATIC( "Network" ),
                                               ASTRAL_XORSTR_STATIC( "Halloween" ) };
    static const char *const MENU_STYLES[]{ ASTRAL_XORSTR_STATIC( "Classic" ),
                                            ASTRAL_XORSTR_STATIC( "Nextgen" ) };

    if ( g_cfg.m_misc.m_menu_style >= gui::MENU_STYLE_COUNT )
    {
        g_cfg.m_misc.m_menu_style = gui::MENU_STYLE_NEXTGEN;
    }
    if ( g_cfg.m_misc.m_watermark_mode >= 2 ) g_cfg.m_misc.m_watermark_mode = 0;
    if ( g_cfg.m_misc.m_binds_list_name_mode >= 2 )
    {
        g_cfg.m_misc.m_binds_list_name_mode = 0;
    }

    const bool halloween_available{ halloween_particle_season( ) };
    const usize particle_count{ halloween_available ? gui::MENU_PARTICLES_COUNT
                                                      : gui::MENU_PARTICLES_HALLOWEEN };
    if ( g_cfg.m_misc.m_menu_particles >= particle_count )
    {
        g_cfg.m_misc.m_menu_particles = gui::MENU_PARTICLES_NETWORK;
    }

    gui::color_picker *accent{ appearance->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Accent start" ), &g_cfg.m_misc.m_accent, false ) };
    gui::color_picker *accent_end{ appearance->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Accent end" ), &g_cfg.m_misc.m_accent_end, false ) };
    gui::combo_box    *menu_style{ appearance->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Menu style" ), MENU_STYLES, gui::MENU_STYLE_COUNT, &g_cfg.m_misc.m_menu_style ) };
    gui::color_picker *shadow{ appearance->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Shadow accent" ), &g_cfg.m_misc.m_shadow ) };
    gui::checkbox     *menu_blur{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Background blur" ), &g_cfg.m_misc.m_menu_blur ) };
    gui::checkbox     *astral_effect{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Astral effect" ), &g_cfg.m_misc.m_menu_astral_effect ) };
    s_ui_scale_pending = g_cfg.m_misc.m_ui_scale;
    s_ui_scale_slider  = appearance->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "UI scale" ),
                                                             &s_ui_scale_pending, 100, 200,
                                                             ASTRAL_XORSTR_STATIC( "%" ) );
    s_ui_font_input    = appearance->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Installed font family" ) );
    s_ui_font_input->set_limit( 48 )->set_value( g_cfg.m_misc.m_ui_font );
    g_render.set_ui_font( g_cfg.m_misc.m_ui_font );
    gui::checkbox   *snap_indicators{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Snap indicators" ), &g_cfg.m_misc.m_snap_indicators ) };
    gui::combo_box  *particles{ appearance->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Menu particles" ), PARTICLE_KINDS, particle_count, &g_cfg.m_misc.m_menu_particles ) };
    gui::checkbox   *menu_sounds{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Menu sounds" ), &g_cfg.m_misc.m_menu_sounds ) };
    gui::slider_int *sound_volume{ appearance->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Sound volume" ), &g_cfg.m_misc.m_menu_sound_volume, 0, 100, ASTRAL_XORSTR_STATIC( "%" ) ) };
    gui::checkbox   *draw_watermark{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Draw watermark" ), &g_cfg.m_misc.m_draw_watermark ) };
    static const char *const WATERMARK_MODES[]{ ASTRAL_XORSTR_STATIC( "Standard" ), ASTRAL_XORSTR_STATIC( "Island" ) };
    gui::combo_box *watermark_mode{ appearance->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Watermark mode" ), WATERMARK_MODES, 2, &g_cfg.m_misc.m_watermark_mode ) };
    gui::checkbox *watermark_user{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Watermark username" ), &g_cfg.m_misc.m_watermark_show_user ) };
    gui::checkbox *watermark_fps{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Watermark FPS" ), &g_cfg.m_misc.m_watermark_show_fps ) };
    gui::checkbox *watermark_ping{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Watermark ping" ), &g_cfg.m_misc.m_watermark_show_ping ) };
    gui::checkbox *watermark_clock{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Watermark clock" ), &g_cfg.m_misc.m_watermark_show_clock ) };
    gui::checkbox *watermark_class{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Watermark class" ), &g_cfg.m_misc.m_watermark_show_class ) };
    gui::checkbox *watermark_kd{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Watermark K/D" ), &g_cfg.m_misc.m_watermark_show_kd ) };
    gui::checkbox   *spectator_list{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Spectator list" ), &g_cfg.m_misc.m_spectator_list ) };
    gui::checkbox   *binds_list{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Binds window" ), &g_cfg.m_misc.m_binds_list ) };
    gui::checkbox   *binds_show_always{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Show always-on binds" ), &g_cfg.m_misc.m_binds_list_show_always ) };
    gui::checkbox   *astral_sense{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "AstralSense dashboard" ), &g_cfg.m_misc.m_astral_sense ) };
    gui::checkbox   *sense_player_card{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Player card" ), &g_cfg.m_misc.m_astral_sense_player_card ) };
    gui::checkbox   *sense_player_card_custom{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Custom player image" ), &g_cfg.m_misc.m_astral_sense_player_card_custom_image ) };
    g_player_card_image.refresh( );
    s_player_card_image_list = appearance->add< gui::list_box >(
        g_player_card_image.labels( ), g_player_card_image.count( ),
        &g_cfg.m_misc.m_astral_sense_player_card_image );
    s_player_card_image_list->set_render_on( &player_card_uses_custom_image );
    appearance->add< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh player images" ), &refresh_player_card_images );
    gui::checkbox   *sense_status{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Status card" ), &g_cfg.m_misc.m_astral_sense_status ) };
    static const char *const STATUS_FIELDS[]{
        ASTRAL_XORSTR_STATIC( "Health" ), ASTRAL_XORSTR_STATIC( "Clip" ),
        ASTRAL_XORSTR_STATIC( "Aimbot" ), ASTRAL_XORSTR_STATIC( "Melee aimbot" ),
        ASTRAL_XORSTR_STATIC( "Triggerbot" ), ASTRAL_XORSTR_STATIC( "Aim type" ),
        ASTRAL_XORSTR_STATIC( "Thirdperson" ), ASTRAL_XORSTR_STATIC( "Bunnyhop" ),
        ASTRAL_XORSTR_STATIC( "Anti-aim" ), ASTRAL_XORSTR_STATIC( "Seed sync" ),
        ASTRAL_XORSTR_STATIC( "Anti-bait" ), ASTRAL_XORSTR_STATIC( "Backtrack" ),
        ASTRAL_XORSTR_STATIC( "Aim FOV" ), ASTRAL_XORSTR_STATIC( "Weapon FOV" ),
        ASTRAL_XORSTR_STATIC( "Deaths" ), ASTRAL_XORSTR_STATIC( "Ping" )
    };
    gui::combo_box *sense_status_fields{ appearance->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Status fields" ), STATUS_FIELDS, 16,
        g_cfg.m_misc.m_astral_sense_status_fields ) };
    gui::checkbox   *sense_damage{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Damage graph" ), &g_cfg.m_misc.m_astral_sense_damage ) };
    gui::checkbox   *sense_speed{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Speed card" ), &g_cfg.m_misc.m_astral_sense_speed ) };
    gui::checkbox   *sense_health{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Health card" ), &g_cfg.m_misc.m_astral_sense_health ) };
    gui::checkbox   *sense_session{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Session card" ), &g_cfg.m_misc.m_astral_sense_session ) };
    gui::checkbox   *sense_trail{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Movement trail" ), &g_cfg.m_misc.m_astral_sense_trail ) };
    gui::checkbox   *sense_diamond{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Overhead diamond" ), &g_cfg.m_misc.m_astral_sense_diamond ) };
    gui::checkbox   *sense_resources{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Class resources" ), &g_cfg.m_misc.m_astral_sense_resources ) };
    gui::checkbox   *sense_buildings{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Building monitor" ), &g_cfg.m_misc.m_astral_sense_buildings ) };
    gui::checkbox   *sense_combat{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Combat tracker" ), &g_cfg.m_misc.m_astral_sense_combat ) };
    gui::checkbox   *sense_conditions{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Conditions card" ), &g_cfg.m_misc.m_astral_sense_conditions ) };
    gui::checkbox   *sense_compass{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Velocity compass" ), &g_cfg.m_misc.m_astral_sense_compass ) };
    gui::checkbox   *sense_objective{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Objective card" ), &g_cfg.m_misc.m_astral_sense_objective ) };
    gui::checkbox   *sense_events{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Event stream" ), &g_cfg.m_misc.m_astral_sense_events ) };
    gui::checkbox   *sense_alerts{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Contextual alerts" ), &g_cfg.m_misc.m_astral_sense_alerts ) };
    gui::checkbox   *sense_arsenal{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Arsenal readout" ), &g_cfg.m_misc.m_astral_sense_arsenal ) };
    gui::checkbox   *sense_telemetry{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Network telemetry" ), &g_cfg.m_misc.m_astral_sense_telemetry ) };
    gui::checkbox   *sense_readiness{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Readiness matrix" ), &g_cfg.m_misc.m_astral_sense_readiness ) };
    gui::checkbox   *sense_history{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Session history" ), &g_cfg.m_misc.m_astral_sense_history ) };
    gui::checkbox   *sense_performance{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Performance graphs" ), &g_cfg.m_misc.m_astral_sense_performance_graphs ) };
    gui::checkbox   *sense_screen_alerts{ appearance->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Screen-edge alerts" ), &g_cfg.m_misc.m_astral_sense_screen_alerts ) };
    static const char *const BIND_NAME_MODES[]{ ASTRAL_XORSTR_STATIC( "Feature names" ),
                                                ASTRAL_XORSTR_STATIC( "Variables" ) };
    gui::combo_box *bind_name_mode{ appearance->add< gui::combo_box >(
        ASTRAL_XORSTR_STATIC( "Bind labels" ), BIND_NAME_MODES, 2,
        &g_cfg.m_misc.m_binds_list_name_mode ) };

    accent->set_on_change( &apply_accent )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the primary interface accent and the left side of the Nextgen gradient." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "theme colour color accent start gradient skin" ) );

    accent_end->set_on_change( &apply_accent )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Sets the right side of the Nextgen header and HUD accent gradient." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "theme colour color accent end gradient nextgen" ) );

    menu_style->set_tooltip( ASTRAL_XORSTR_STATIC( "Nextgen uses Astral-inspired dark panels, a two-colour title treatment, and a softer control palette." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "nextgen classic layout theme astral style" ) );

    shadow->set_tooltip( ASTRAL_XORSTR_STATIC( "Colour and strength of the drop shadow the menu "
                                              "casts on the game." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "shadow drop glow colour color" ) );

    menu_blur->set_tooltip( ASTRAL_XORSTR_STATIC( "Blurs the game itself while the menu is open. "
                                                 "The frosted panel under the menu is always on." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "blur background frost menu" ) );

    astral_effect->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws an animated space field and an accent-coloured neon glow behind the menu." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "astral space shader stars neon glow background" ) )
        ->set_render_on( &classic_menu_style );

    s_ui_scale_slider->set_tooltip( ASTRAL_XORSTR_STATIC( "Scales the complete interface after the slider is released. Large values may be limited by the available screen space." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "dpi scale size zoom accessibility menu" ) );

    s_ui_font_input->set_tooltip( ASTRAL_XORSTR_STATIC( "Uses an installed Windows font family for the interface. "
                                                       "Enter its family name, such as Segoe UI or Verdana." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "font typeface family installed windows text accessibility" ) );

    snap_indicators->set_tooltip( ASTRAL_XORSTR_STATIC( "Dragged indicator cards land on a grid instead of "
                                                       "wherever the mouse lets go." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "snap grid indicator card drag align" ) );

    particles->set_tooltip( ASTRAL_XORSTR_STATIC( "Drifts over the background while the menu is "
                                                 "open." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "particles snow network constellation menu" ) );

    menu_sounds->set_tooltip( ASTRAL_XORSTR_STATIC( "Plays a sound when the menu reacts to you and when a "
                                                   "notification comes in." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "sound audio click hover ui menu notification" ) );

    sound_volume->set_render_on( &g_cfg.m_misc.m_menu_sounds )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Loudness of the menu and notification sounds." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "sound audio volume loudness ui menu notification" ) );

    spectator_list->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows players watching you, including deathcam and freezecam. Open the menu to preview and drag it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "spectators observers freezecam avatar watching" ) );

    watermark_mode->set_render_on( &g_cfg.m_misc.m_draw_watermark );
    watermark_user->set_render_on( &g_cfg.m_misc.m_draw_watermark );
    watermark_fps->set_render_on( &g_cfg.m_misc.m_draw_watermark );
    watermark_ping->set_render_on( &g_cfg.m_misc.m_draw_watermark );
    watermark_clock->set_render_on( &g_cfg.m_misc.m_draw_watermark );
    watermark_class->set_render_on( &g_cfg.m_misc.m_draw_watermark );
    watermark_kd->set_render_on( &g_cfg.m_misc.m_draw_watermark );

    binds_list->set_tooltip( ASTRAL_XORSTR_STATIC( "Shows options with an assigned key. Active entries are highlighted. Open the menu to drag the card." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "binds keybinds hotkeys active window card overlay" ) );

    astral_sense->set_tooltip( ASTRAL_XORSTR_STATIC( "Native status dashboard port. Its cards use the current menu style and can be dragged while the menu is open." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "astralsense dashboard status damage speed health session trail diamond" ) );
    sense_status->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_status_fields->set_render_on( &g_cfg.m_misc.m_astral_sense_status );
    sense_damage->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_speed->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_health->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_session->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_trail->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_diamond->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_resources->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_buildings->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_combat->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_conditions->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_compass->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_objective->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_events->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_alerts->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_arsenal->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_telemetry->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_readiness->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_history->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_performance->set_render_on( &g_cfg.m_misc.m_astral_sense );
    sense_screen_alerts->set_render_on( &g_cfg.m_misc.m_astral_sense );

    binds_show_always->set_render_on( &g_cfg.m_misc.m_binds_list )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Also lists options configured in Always mode even when they have no physical key assigned." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "binds always on no key keybinds window" ) );

    bind_name_mode->set_render_on( &g_cfg.m_misc.m_binds_list )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Uses either readable feature names or the universal config/Lua variable path. A custom name overrides both." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "binds labels variables lua config custom names" ) );

    gui::group_box *mascot{ general->add_group( ICON_SPARKLES, ASTRAL_XORSTR_STATIC( "Menu mascot" ),
                                                gui::column_t::right ) };
    static const char *const MASCOT_SIDES[]{ ASTRAL_XORSTR_STATIC( "Left" ), ASTRAL_XORSTR_STATIC( "Right" ),
                                              ASTRAL_XORSTR_STATIC( "Top" ), ASTRAL_XORSTR_STATIC( "Bottom" ) };
    g_menu_mascot.refresh( );
    mascot->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Enabled" ), &g_cfg.m_misc.m_menu_mascot );
    s_mascot_list = mascot->add< gui::list_box >( g_menu_mascot.labels( ), g_menu_mascot.count( ),
                                                  &g_cfg.m_misc.m_menu_mascot_file );
    s_mascot_list->set_render_on( &mascot_has_images );
    mascot->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ), &mascot_empty_text )
        ->set_render_on( &mascot_no_images );
    mascot->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Attached side" ), MASCOT_SIDES, 4,
                                   &g_cfg.m_misc.m_menu_mascot_side );
    mascot->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Size" ), &g_cfg.m_misc.m_menu_mascot_size,
                                    100, 400, ASTRAL_XORSTR_STATIC( " px" ) );
    mascot->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Edge offset" ), &g_cfg.m_misc.m_menu_mascot_offset,
                                    -500, 500, ASTRAL_XORSTR_STATIC( " px" ) )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Moves the mascot along the selected edge from its centered anchor." ) );
    mascot->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Menu spacing" ), &g_cfg.m_misc.m_menu_mascot_spacing,
                                    -200, 200, ASTRAL_XORSTR_STATIC( " px" ) )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Positive values add space. Negative values tuck the mascot behind the menu." ) );
    mascot->add< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh folder" ), &refresh_mascots )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Reloads images from AstralMod/Mascots. Only the selected file is loaded; 32 MB is retained as a memory-safety ceiling." ) );

    gui::group_box *palette{ general->add_group( ICON_SPARKLES,
                                                 ASTRAL_XORSTR_STATIC( "UI palette" ),
                                                 gui::column_t::left ) };
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Background" ), &g_cfg.m_misc.m_ui_background )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Surface" ), &g_cfg.m_misc.m_ui_surface )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Raised surface" ), &g_cfg.m_misc.m_ui_raised )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Borders" ), &g_cfg.m_misc.m_ui_border, false )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Primary text" ), &g_cfg.m_misc.m_ui_text, false )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Muted text" ), &g_cfg.m_misc.m_ui_muted, false )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Dim text" ), &g_cfg.m_misc.m_ui_dim, false )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Hover" ), &g_cfg.m_misc.m_ui_hover )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Selection" ), &g_cfg.m_misc.m_ui_selection, false )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Warning" ), &g_cfg.m_misc.m_ui_warning, false )
        ->set_on_change( &apply_accent );
    palette->add< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Error" ), &g_cfg.m_misc.m_ui_error, false )
        ->set_on_change( &apply_accent );
    gui::label *palette_actions{ palette->add< gui::label >( ASTRAL_XORSTR_STATIC( "Palette actions" ), nullptr ) };
    palette_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "From accent" ),
                                                &generate_palette_from_accent );

    gui::group_box *bind_editor{ general->add_group( ICON_ZAP,
                                                      ASTRAL_XORSTR_STATIC( "Bind names" ),
                                                      gui::column_t::left ) };
    s_bind_editor_list = bind_editor->add< gui::list_box >( s_bind_editor_items,
                                                            s_bind_editor_count,
                                                            &s_bind_editor_selected );
    s_bind_editor_name = bind_editor->add< gui::text_input >(
        ASTRAL_XORSTR_STATIC( "Custom name (optional)" ) );
    s_bind_editor_name->set_limit( 47 );
    s_bind_editor_name->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Apply" ),
                                                   &apply_bind_display_name );
    s_bind_editor_name->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Clear" ),
                                                   &clear_bind_display_name );
    s_bind_editor_list->set_tooltip( ASTRAL_XORSTR_STATIC( "Selects a bind by its universal config/Lua path." ) );
    s_bind_editor_name->set_tooltip( ASTRAL_XORSTR_STATIC( "Overrides the selected bind's Feature names or Variables label. Clear restores the selected display mode." ) );

    gui::group_box *saved{ general->add_group( ICON_SAVE, ASTRAL_XORSTR_STATIC( "Configs" ),
                                               gui::column_t::right ) };

    s_menu_user_input = saved->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Menu username" ) );
    s_menu_user_input->set_limit( 32 )->set_value( string_view{ g_cfg.m_misc.m_menu_user } );
    s_menu_user_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Apply" ), &apply_profile_names );

    s_streamer_alias_input = saved->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Streamer self name" ) );
    s_streamer_alias_input->set_limit( 32 )->set_value( string_view{ g_cfg.m_streamer_mode.m_local_alias } );
    s_streamer_alias_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Apply" ), &apply_profile_names );

    s_config_name_input = saved->add< gui::text_input >( ASTRAL_XORSTR_STATIC( "Config name" ) );
    s_config_picker     = saved->add< gui::list_box >( configs::names( ), configs::count( ),
                                                       &s_selected_config );

    gui::label *empty{ saved->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ),
                                                 &no_configs_text ) };
    gui::label *actions{ saved->add< gui::label >( ASTRAL_XORSTR_STATIC( "Actions" ), nullptr ) };

    s_config_name_input->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Save" ), &save_config );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Load" ), &load_config );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Delete" ), &delete_config );
    actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &refresh_configs );

    s_config_picker->set_render_on( &has_configs );
    empty->set_render_on( &no_configs );

    s_config_name_input->set_filter( &config_name_filter )
        ->set_limit( 32 )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Saves the current settings under this name. Leave it "
                                            "empty to overwrite the selected config." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "config save load preset" ) );

    gui::group_box *runtime{ general->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Runtime" ),
                                                 gui::column_t::right ) };
    static const char *const ANTICHEAT_PROFILES[]{ ASTRAL_XORSTR_STATIC( "Off" ),
                                                   ASTRAL_XORSTR_STATIC( "StAC" ),
                                                   ASTRAL_XORSTR_STATIC( "SMAC" ) };
    runtime->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "AntiCheat Profile" ), ANTICHEAT_PROFILES,
                                    config::misc_t::ANTICHEAT_PROFILE_COUNT,
                                    &g_cfg.m_misc.m_anticheat_profile )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Conservative compatibility policy. StAC removes packet/command exploits, silent or locking aim, and automatic attacks while retaining smoothed manual aim. SMAC additionally disables automated aiming and attack decisions. This is risk reduction, not a detection guarantee or bypass." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "stac smac anti cheat compatibility safe profile restrictions" ) );
    gui::label     *unload_key{ runtime->add< gui::label >( ASTRAL_XORSTR_STATIC( "Unload key" ), nullptr ) };
    unload_key->add_module< gui::keybind >( string_view{ }, &g_cfg.m_misc.m_unload_key );
    runtime->add< gui::button >( ASTRAL_XORSTR_STATIC( "Unload AstralMod" ), &request_astral_unload )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Stops AstralMod, restores its hooks, and unloads the module. Default key: F11." ) );

    gui::group_box *discord{ general->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Discord presence" ),
                                                 gui::column_t::right ) };
    gui::checkbox *discord_enabled{ discord->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Rich presence" ), &g_cfg.m_misc.m_discord_rpc ) };
    discord->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Show current map" ),
                                   &g_cfg.m_misc.m_discord_rpc_map )
        ->set_render_on( &g_cfg.m_misc.m_discord_rpc );
    discord->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Show current class" ),
                                   &g_cfg.m_misc.m_discord_rpc_class )
        ->set_render_on( &g_cfg.m_misc.m_discord_rpc );
    discord_enabled
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Publishes main-menu or in-game status, current K/D, and match time through Discord's local IPC. No Discord login or SDK is used by AstralMod." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "discord rpc rich presence status map class kd match time" ) );

    gui::group_box *scripts{ general->add_group( ICON_CODE, ASTRAL_XORSTR_STATIC( "Lua scripts" ),
                                                 gui::column_t::right ) };

    s_lua_settings_files = scripts->add< gui::list_box >( g_scripts.file_names( ), g_scripts.file_count( ),
                                                          &s_lua_settings_file );

    gui::label *no_scripts{ scripts->add< gui::label >( ASTRAL_XORSTR_STATIC( "Empty" ), &lua_no_files_text ) };
    gui::label *script_actions{ scripts->add< gui::label >( ASTRAL_XORSTR_STATIC( "Actions" ), nullptr ) };
    scripts->add< gui::label >( ASTRAL_XORSTR_STATIC( "Running" ), &lua_loaded_text );

    script_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Load" ), &lua_load );
    script_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Unload" ), &lua_unload );
    script_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Refresh" ), &lua_refresh );

    s_lua_settings_files->set_render_on( &lua_has_files )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Scripts in the AstralMod/Scripts folder next to the game. The ones "
                                            "running when a config is saved load again with it." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "lua script load unload" ) );
    no_scripts->set_render_on( &lua_no_files );

    gui::sub_tab   *notifications{ settings->add_subtab( ASTRAL_XORSTR_STATIC( "Notifications" ) ) };
    gui::group_box *toast_settings{ notifications->add_group( ICON_ZAP,
                                                              ASTRAL_XORSTR_STATIC( "Toast controls" ),
                                                              gui::column_t::left ) };
    gui::checkbox  *notifications_enabled{ toast_settings->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Show notifications" ), &g_cfg.m_misc.m_notifications ) };
    toast_settings->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Keep notification log" ),
                                          &g_cfg.m_misc.m_notification_log );

    static const char *const NOTIFICATION_KINDS[]{
        ASTRAL_XORSTR_STATIC( "Success" ), ASTRAL_XORSTR_STATIC( "Information" ),
        ASTRAL_XORSTR_STATIC( "Warning" ), ASTRAL_XORSTR_STATIC( "Error" )
    };
    static const char *const NOTIFICATION_CHANNELS[]{
        ASTRAL_XORSTR_STATIC( "System" ), ASTRAL_XORSTR_STATIC( "Players" ),
        ASTRAL_XORSTR_STATIC( "Configs" ), ASTRAL_XORSTR_STATIC( "Lua" ),
        ASTRAL_XORSTR_STATIC( "ConVars" ), ASTRAL_XORSTR_STATIC( "Audio" ),
        ASTRAL_XORSTR_STATIC( "Gameplay / network" ),
        ASTRAL_XORSTR_STATIC( "Visuals" )
    };
    static const char *const NOTIFICATION_POSITIONS[]{
        ASTRAL_XORSTR_STATIC( "Top right" ), ASTRAL_XORSTR_STATIC( "Top left" ),
        ASTRAL_XORSTR_STATIC( "Bottom right" ), ASTRAL_XORSTR_STATIC( "Bottom left" )
    };

    toast_settings->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Severities" ),
                                           NOTIFICATION_KINDS, 4,
                                           g_cfg.m_misc.m_notification_kinds )
        ->set_render_on( &g_cfg.m_misc.m_notifications );
    toast_settings->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Sources" ),
                                           NOTIFICATION_CHANNELS, 8,
                                           g_cfg.m_misc.m_notification_channels )
        ->set_render_on( &g_cfg.m_misc.m_notifications );
    toast_settings->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Position" ),
                                           NOTIFICATION_POSITIONS, 4,
                                           &g_cfg.m_misc.m_notification_position )
        ->set_render_on( &g_cfg.m_misc.m_notifications );
    toast_settings->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Lifetime" ),
                                            &g_cfg.m_misc.m_notification_lifetime,
                                            1000, 15000, ASTRAL_XORSTR_STATIC( "ms" ) )
        ->set_render_on( &g_cfg.m_misc.m_notifications );

    notifications_enabled->set_tooltip( ASTRAL_XORSTR_STATIC(
        "Controls popup toasts. Disabled severities and sources can still be retained in the log." ) );

    gui::sub_tab   *notification_history{ settings->add_subtab(
        ASTRAL_XORSTR_STATIC( "Notification log" ) ) };
    gui::group_box *history{ notification_history->add_group(
        ICON_SAVE, ASTRAL_XORSTR_STATIC( "History" ), gui::column_t::full ) };
    s_notification_log_list = history->add< gui::list_box >( s_notification_log_items,
                                                             s_notification_log_count,
                                                             &s_notification_log_selection );
    gui::group_box *history_detail{ notification_history->add_group(
        ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Selected notification" ), gui::column_t::full ) };
    history_detail->add< gui::label >( ASTRAL_XORSTR_STATIC( "Severity" ),
                                       &notification_log_detail_kind );
    history_detail->add< gui::label >( ASTRAL_XORSTR_STATIC( "Source" ),
                                       &notification_log_detail_channel );
    history_detail->add< gui::label >( ASTRAL_XORSTR_STATIC( "Title" ),
                                       &notification_log_detail_title );
    history_detail->add< gui::label >( ASTRAL_XORSTR_STATIC( "Message" ),
                                       &notification_log_detail_message, 5 );
    gui::label *history_actions{ history_detail->add< gui::label >(
        ASTRAL_XORSTR_STATIC( "Actions" ), nullptr ) };
    history_actions->add_module< gui::button >( ASTRAL_XORSTR_STATIC( "Clear" ),
                                                &clear_notification_history );
    refresh_notification_log( );

    gui::sub_tab   *debug{ settings->add_subtab( ASTRAL_XORSTR_STATIC( "Debug" ) ) };
    gui::group_box *performance{ debug->add_group( ICON_SETTINGS, ASTRAL_XORSTR_STATIC( "Performance" ), gui::column_t::left ) };

    gui::checkbox   *projectile_profiler{ performance->add< gui::checkbox >(
        ASTRAL_XORSTR_STATIC( "Projectile profiler" ), &g_cfg.m_ragebot.m_projectile_profiler ) };
    gui::slider_int *projectile_profiler_fps{ performance->add< gui::slider_int >(
        ASTRAL_XORSTR_STATIC( "FPS budget" ), &g_cfg.m_ragebot.m_projectile_profiler_fps,
        30, 240, ASTRAL_XORSTR_STATIC( "fps" ) ) };

    projectile_profiler->set_tooltip( ASTRAL_XORSTR_STATIC(
                                          "Times projectile targeting, movement prediction, direct solving and Splashbot independently, then reports frames that exceed the selected budget." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "projectile profiler lag performance fps debug" ) );
    projectile_profiler_fps->set_tooltip( ASTRAL_XORSTR_STATIC(
                                              "Reports when projectile solving alone takes longer than one frame at this framerate." ) )
        ->set_render_on( &g_cfg.m_ragebot.m_projectile_profiler );

    gui::group_box *animation{ debug->add_group( ICON_USER, ASTRAL_XORSTR_STATIC( "Animation" ), gui::column_t::left ) };

    gui::checkbox *server_hitboxes{ animation->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Server hitboxes" ), &g_cfg.m_debug.m_server_hitboxes ) };
    gui::checkbox *client_hitboxes{ animation->add< gui::checkbox >( ASTRAL_XORSTR_STATIC( "Client hitboxes" ), &g_cfg.m_debug.m_client_hitboxes ) };

    server_hitboxes->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Server" ), &g_cfg.m_debug.m_server_hitbox_color );
    client_hitboxes->add_module< gui::color_picker >( ASTRAL_XORSTR_STATIC( "Client" ), &g_cfg.m_debug.m_client_hitbox_color );

    server_hitboxes->set_tooltip( ASTRAL_XORSTR_STATIC( "On a server you host, the hitboxes the server animated. They show its last tick, "
                                                       "so a moving player's boxes can sit a tick from ours." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "debug server hitbox hitboxes bones animation listen" ) );
    client_hitboxes->set_tooltip( ASTRAL_XORSTR_STATIC( "The hitboxes of the skeleton the game is served for every player, drawn over "
                                                       "the server's: where the two match only this colour shows." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "debug client hitbox hitboxes bones animation" ) );

    gui::group_box          *triggers{ debug->add_group( ICON_EYE, ASTRAL_XORSTR_STATIC( "Trigger volumes" ), gui::column_t::right ) };
    static const char *const TRIGGER_TYPES[]{
        ASTRAL_XORSTR_STATIC( "Hurt" ), ASTRAL_XORSTR_STATIC( "Ignite" ),
        ASTRAL_XORSTR_STATIC( "Push" ), ASTRAL_XORSTR_STATIC( "Regenerate" ),
        ASTRAL_XORSTR_STATIC( "Respawn room" ), ASTRAL_XORSTR_STATIC( "Capture area" ),
        ASTRAL_XORSTR_STATIC( "Catapult" ), ASTRAL_XORSTR_STATIC( "Apply impulse" )
    };

    triggers->add< gui::combo_box >( ASTRAL_XORSTR_STATIC( "Shown" ), TRIGGER_TYPES,
                                     config::debug_t::TRIGGER_COUNT, g_cfg.m_debug.m_triggers )
        ->set_tooltip( ASTRAL_XORSTR_STATIC( "Draws functional brush volumes parsed from the current map." ) )
        ->set_keywords( ASTRAL_XORSTR_STATIC( "debug map trigger hurt ignite push regenerate respawn capture catapult impulse brush" ) );
    triggers->add< gui::slider_int >( ASTRAL_XORSTR_STATIC( "Draw distance" ),
                                      &g_cfg.m_debug.m_trigger_distance, 500, 16000,
                                      ASTRAL_XORSTR_STATIC( "u" ) );

    for ( usize index{ }; index < config::debug_t::TRIGGER_COUNT; ++index )
    {
        triggers->add< gui::color_picker >( TRIGGER_TYPES[ index ],
                                            &g_cfg.m_debug.m_trigger_colors[ index ] );
    }
}

watermark_values_t watermark_values( )
{
    watermark_values_t values{ };

    values.m_watermark_position = &g_cfg.m_misc.m_watermark_position;
    values.m_mode       = g_cfg.m_misc.m_watermark_mode;
    values.m_show_user  = g_cfg.m_misc.m_watermark_show_user;
    values.m_show_fps   = g_cfg.m_misc.m_watermark_show_fps;
    values.m_show_ping  = g_cfg.m_misc.m_watermark_show_ping;
    values.m_show_clock = g_cfg.m_misc.m_watermark_show_clock;
    values.m_show_class = g_cfg.m_misc.m_watermark_show_class;
    values.m_show_kd    = g_cfg.m_misc.m_watermark_show_kd;

    values.m_user = g_cfg.m_misc.m_menu_user[ 0 ]
                        ? g_cfg.m_misc.m_menu_user
                        : ASTRAL_XORSTR_STATIC( "User" );

    if ( g_cl.m_local )
    {
        values.m_kills  = g_cl.m_local->m_iKills( );
        values.m_deaths = g_cl.m_local->m_iDeaths( );
        switch ( g_cl.m_local->m_iClass( ) )
        {
            case TF_CLASS_SCOUT: values.m_class_name = ASTRAL_XORSTR_STATIC( "Scout" ); break;
            case TF_CLASS_SNIPER: values.m_class_name = ASTRAL_XORSTR_STATIC( "Sniper" ); break;
            case TF_CLASS_SOLDIER: values.m_class_name = ASTRAL_XORSTR_STATIC( "Soldier" ); break;
            case TF_CLASS_DEMOMAN: values.m_class_name = ASTRAL_XORSTR_STATIC( "Demoman" ); break;
            case TF_CLASS_MEDIC: values.m_class_name = ASTRAL_XORSTR_STATIC( "Medic" ); break;
            case TF_CLASS_HEAVYWEAPONS: values.m_class_name = ASTRAL_XORSTR_STATIC( "Heavy" ); break;
            case TF_CLASS_PYRO: values.m_class_name = ASTRAL_XORSTR_STATIC( "Pyro" ); break;
            case TF_CLASS_SPY: values.m_class_name = ASTRAL_XORSTR_STATIC( "Spy" ); break;
            case TF_CLASS_ENGINEER: values.m_class_name = ASTRAL_XORSTR_STATIC( "Engineer" ); break;
            default: values.m_class_name = ASTRAL_XORSTR_STATIC( "No class" ); break;
        }
    }

    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        return values;
    }

    CNetChannel *net_channel{ g_tf2.m_engine->GetNetChannelInfo( ) };

    if ( !net_channel )
    {
        return values;
    }

    values.m_latency            = static_cast< i32 >( net_channel->GetLatency( FLOW_OUTGOING ) * 1000.0f );
    values.m_fake_latency       = static_cast< i32 >( g_fake_latency.applied( ) * 1000.0f );
    values.m_scoreboard_latency = g_fake_latency.scoreboard_latency( );
    values.m_connected          = true;

    return values;
}

void sync_menu_theme( )
{
    loader_profile::poll( );
    sync_bind_editor( );

    g_cfg.m_ragebot.m_aim_ring_outer_fov = math::clamp( g_cfg.m_ragebot.m_aim_ring_outer_fov, 1, 180 );
    g_cfg.m_ragebot.m_aim_ring_inner_fov = math::clamp( g_cfg.m_ragebot.m_aim_ring_inner_fov, 0,
                                                        g_cfg.m_ragebot.m_aim_ring_outer_fov );
    usize *optional_aim_types[]{
        &g_cfg.m_ragebot.m_aim_ring_inner_type,
        &g_cfg.m_ragebot.m_aim_ring_inner_thirdperson_type,
        &g_cfg.m_ragebot.m_aim_ring_inner_firstperson_type,
        &g_cfg.m_ragebot.m_aim_ring_outer_type,
        &g_cfg.m_ragebot.m_aim_ring_outer_thirdperson_type,
        &g_cfg.m_ragebot.m_aim_ring_outer_firstperson_type,
        &g_cfg.m_ragebot.m_spectator_thirdperson_type,
        &g_cfg.m_ragebot.m_spectator_firstperson_type,
    };
    for ( usize *type : optional_aim_types )
    {
        if ( *type > config::ragebot_t::AIM_TYPE_COUNT )
            *type = config::ragebot_t::AIM_TYPE_COUNT;
    }
    if ( g_cfg.m_melee.m_aim_type >= config::melee_t::AIM_TYPE_COUNT )
        g_cfg.m_melee.m_aim_type = config::melee_t::AIM_SILENT;

    const bool wants_nextgen{ g_cfg.m_misc.m_menu_style == gui::MENU_STYLE_NEXTGEN };
    if ( s_applied_menu_style != g_cfg.m_misc.m_menu_style ||
         g_theme.m_nextgen_style != wants_nextgen )
    {
        apply_accent( );
    }

    refresh_player_list( );
    refresh_notification_log( );
    poll_lua( );
    bool config_loaded{ };
    if ( configs::poll( config_loaded ) )
    {
        if ( config_loaded )
        {
            apply_accent( );
            s_bind_editor_loaded = static_cast< usize >( -1 );
            g_skins.reload_editor( );
            sync_player_integration_keys( );
            sync_profile_name_inputs( );
            s_ui_scale_pending = g_cfg.m_misc.m_ui_scale;
            if ( s_ui_font_input )
                s_ui_font_input->set_value( g_cfg.m_misc.m_ui_font );
            g_render.set_ui_font( g_cfg.m_misc.m_ui_font );
            g_chams.refresh_custom_materials( );
            refresh_custom_cham_slots( );
        }

        s_config_picker->set_items( configs::names( ), configs::count( ) );
        if ( config_loaded )
        {
            s_selected_config = configs::loaded_index( );
        }
    }
    crash_reporter::poll( );

    sync_custom_cham_slots( );

    if ( s_ui_scale_slider && g_gui.active_slider( ) != s_ui_scale_slider &&
         s_ui_scale_pending != g_cfg.m_misc.m_ui_scale )
    {
        g_cfg.m_misc.m_ui_scale = math::clamp( s_ui_scale_pending, 100, 200 );
    }

    if ( s_ui_font_input && !s_ui_font_input->value( ).empty( ) &&
         s_ui_font_input->value( ) != string_view{ g_cfg.m_misc.m_ui_font } )
    {
        str::copy( g_cfg.m_misc.m_ui_font, sizeof( g_cfg.m_misc.m_ui_font ),
                   s_ui_font_input->value( ).data( ), s_ui_font_input->value( ).size( ) );
        g_render.set_ui_font( g_cfg.m_misc.m_ui_font );
    }

    g_theme.m_shadow = g_cfg.m_misc.m_shadow;
    g_gui.set_dpi_scale( g_cfg.m_misc.m_ui_scale );
    g_theme.m_blur_background = g_cfg.m_misc.m_menu_blur;
    g_theme.m_astral_effect   = g_cfg.m_misc.m_menu_astral_effect &&
                                g_cfg.m_misc.m_menu_style == gui::MENU_STYLE_CLASSIC;
    g_theme.m_snap_indicators = g_cfg.m_misc.m_snap_indicators;
    g_theme.m_dock_panels     = g_cfg.m_misc.m_dock_editor;
    const usize particle_count{ halloween_particle_season( ) ? gui::MENU_PARTICLES_COUNT
                                                              : gui::MENU_PARTICLES_HALLOWEEN };
    g_theme.m_particles       = g_cfg.m_misc.m_menu_particles < particle_count
                                    ? g_cfg.m_misc.m_menu_particles
                                    : gui::MENU_PARTICLES_NETWORK;
    g_theme.m_sounds          = g_cfg.m_misc.m_menu_sounds;
    g_theme.m_sound_volume    = static_cast< float >( g_cfg.m_misc.m_menu_sound_volume ) / 100.0f;

    notification_settings_t notifications{ };
    notifications.m_enabled     = g_cfg.m_misc.m_notifications;
    notifications.m_log_enabled = g_cfg.m_misc.m_notification_log;
    notifications.m_lifetime_ms = g_cfg.m_misc.m_notification_lifetime;
    notifications.m_position    = g_cfg.m_misc.m_notification_position;
    for ( usize index{ }; index < 4; ++index )
    {
        notifications.m_kinds[ index ] = g_cfg.m_misc.m_notification_kinds[ index ];
    }
    for ( usize index{ }; index < static_cast< usize >( notify_channel_t::count ); ++index )
    {
        notifications.m_channels[ index ] = g_cfg.m_misc.m_notification_channels[ index ];
    }
    configure_notifications( notifications );
    loader_profile::refresh_status( );
}

bool register_menu( )
{
#if defined( ASTRAL_DEV )
    if ( !g_gui.init( ASTRAL_XORSTR_STATIC( "AstralMod" ) ) )
    {
        return false;
    }
#else
    if ( !g_gui.init( ASTRAL_XORSTR_STATIC( "AstralMod" ) ) ) // TODO: get this from the api
    {
        return false;
    }
#endif // ASTRAL_DEV

    g_gui.set_close_guard( [ ]( ) { return g_menu_mascot.allow_menu_close( ); } );
    g_gui.set_close_callback( [ ]( ) { s_ragebot_class_selector = 0; } );

#if defined( ASTRAL_DEV )
    g_gui.set_status( ASTRAL_XORSTR_STATIC( "Developer" ), g_api.user( ),
                      g_steam.local_account_id( ) );
#else
    g_gui.set_status( ASTRAL_XORSTR_STATIC( "Release" ), g_api.user( ),
                      g_steam.local_account_id( ) );
#endif

    g_gui.set_header_action( ASTRAL_XORSTR_STATIC( "Save" ), &quick_save_config );

    apply_accent( );
    sync_menu_theme( );
    g_chams.refresh_custom_materials( );

    build_ragebot( );
    build_visuals( );
    build_anti_hack( );
    build_player_list( );
    build_skins( );
    build_misc( );
    build_material_editor( );
    build_lua( );
    build_settings( );
    sync_profile_name_inputs( );
    name_menu_elements( );
    refresh_bind_editor( );

    return true;
}

static_assert( TARGET_GROUP_COUNT == config::CHAM_ENTITY_COUNT );
static_assert( chams::CHAM_COUNT <= COMBO_POPUP_MAX_ROWS );
static_assert( config::TRACER_STYLE_COUNT <= COMBO_POPUP_MAX_ROWS );
static_assert( config::REMOVAL_COUNT <= COMBO_POPUP_MAX_ROWS );
static_assert( skins::SHEEN_COUNT <= COMBO_POPUP_MAX_ROWS );
static_assert( skins::KILLSTREAKER_COUNT <= COMBO_POPUP_MAX_ROWS );
static_assert( skins::FOOTPRINT_COUNT <= COMBO_POPUP_MAX_ROWS );
