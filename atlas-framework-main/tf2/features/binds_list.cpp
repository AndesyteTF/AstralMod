#include "binds_list.hpp"

#include "config.hpp"
#include "global.hpp"
#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "gui/theme.hpp"
#include "render/render.hpp"

namespace
{
    constexpr usize BIND_CAPACITY{ 128 };
    constexpr i32   CARD_WIDTH{ 190 };
    constexpr i32   ROW_HEIGHT{ 16 };
    constexpr i32   BODY_PADDING{ 4 };

    indicator_card_t s_card{ };

    const char *bind_value( const bind_t &bind )
    {
        if ( bind.m_mode == bind_mode_t::always )
        {
            return ATLAS_XORSTR_STATIC( "ON" );
        }
        return gui::key_name( bind.m_key );
    }

    string_view bind_label( const gui::keybind_view_t &entry )
    {
        if ( entry.m_bind->m_display_name[ 0 ] )
        {
            return string_view{ entry.m_bind->m_display_name };
        }

        if ( g_cfg.m_misc.m_binds_list_name_mode == 1 && !entry.m_variable.empty( ) )
        {
            return entry.m_variable;
        }
        return entry.m_feature;
    }
}

void draw_binds_list( )
{
    if ( !g_cfg.m_misc.m_binds_list )
    {
        hide_indicator( s_card );
        return;
    }

    gui::keybind_view_t entries[ BIND_CAPACITY ]{ };
    const usize total{ gui::collect_keybinds( entries, BIND_CAPACITY,
                                              g_cfg.m_misc.m_binds_list_show_always ) };
    if ( !total )
    {
        hide_indicator( s_card );
        return;
    }

    const i32 title_space{ 2 + INDICATOR_TITLE_HEIGHT + 1 };
    const i32 available{ math::bigger( g_cl.m_screen_height - INDICATOR_MARGIN * 2 - title_space - BODY_PADDING * 2,
                                      ROW_HEIGHT ) };
    const usize row_capacity{ static_cast< usize >( math::clamp( available / ROW_HEIGHT, 1, 64 ) ) };
    const bool overflow{ total > row_capacity };
    const usize shown{ overflow && row_capacity > 1 ? row_capacity - 1 : math::smaller( total, row_capacity ) };
    const usize rows{ shown + ( overflow ? 1u : 0u ) };
    const i32 height{ title_space + BODY_PADDING * 2 + static_cast< i32 >( rows ) * ROW_HEIGHT + 1 };

    s_card.m_at = g_cfg.m_misc.m_binds_list_position;
    step_indicator( s_card, 0.0f, g_cl.m_screen_width, g_cl.m_screen_height,
                    CARD_WIDTH, height, true );
    g_cfg.m_misc.m_binds_list_position = s_card.m_at;

    const indicator_frame_t frame{ draw_indicator_frame( s_card, ATLAS_XORSTR_STATIC( "binds" ), CARD_TITLE_TEXT ) };
    const u8 enter{ frame.m_enter };
    const i32 left{ frame.m_content.left };
    const i32 right{ frame.m_content.right };
    i32 top{ frame.m_content.top + BODY_PADDING };

    for ( usize index{ }; index < shown; ++index )
    {
        bind_t &bind{ *entries[ index ].m_bind };
        const bool active{ gui::bind_active( bind ) };
        const rect_t row{ left, top, right, top + ROW_HEIGHT };

        if ( active )
        {
            gui::fill_rounded_box( row, g_gui.scaled( 2 ),
                                   gui::with_alpha( gui::with_alpha( g_theme.m_accent, 32 ), enter ) );
        }

        const char *value{ bind_value( bind ) };
        const i32 value_width{ gui::estimate_tiny_text_width( value ) };
        char label[ MENU_LABEL_LENGTH ]{ };
        gui::fit_text( label, bind_label( entries[ index ] ),
                       right - left - value_width - g_gui.scaled( 18 ) );

        const i32 dot{ g_gui.scaled( 3 ) };
        const i32 dot_top{ top + ( ROW_HEIGHT - dot ) / 2 };
        gui::fill_rounded_box( rect_t{ left, dot_top, left + dot, dot_top + dot }, dot,
                               gui::with_alpha( active ? g_theme.m_accent : MENU_TEXT_FAINT, enter ) );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ left + g_gui.scaled( 8 ), indicator_text_y( row ) },
                       gui::with_alpha( active ? MENU_TEXT : MENU_TEXT_MUTED, enter ), label );
        g_render.text( MENU_FONT_TINY( ), point_t{ right, indicator_text_y( row ) },
                       gui::with_alpha( active ? g_theme.m_accent : MENU_TEXT_DIM, enter ), value,
                       false, render::text_align::right );
        top += ROW_HEIGHT;
    }

    if ( overflow )
    {
        char more[ 32 ]{ };
        str::format_to( more, sizeof( more ), ATLAS_XORSTR( "+{} more" ).get( ), total - shown );
        g_render.text( MENU_FONT_TINY( ),
                       point_t{ left + g_gui.scaled( 8 ), indicator_text_y( rect_t{ left, top, right, top + ROW_HEIGHT } ) },
                       gui::with_alpha( MENU_TEXT_DIM, enter ), more );
    }
}
