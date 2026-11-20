#include "input/input.hpp"
#include "libs/lazy_importer.hpp"
#include "math/math.hpp"
#include "render/render.hpp"
#include "runtime/assert.hpp"
#include "util/windefs.hpp"

#include "gui.hpp"
#include "particles.hpp"
#include "theme.hpp"

inline constexpr float MENU_COLLAPSED_SCALE{ 0.10f };
inline constexpr float MENU_FULL_SCALE{ 1.0f };
inline constexpr u32   MENU_OPEN_DURATION{ 210 };
inline constexpr u32   MENU_CLOSE_DURATION{ 150 };
inline constexpr float MENU_FADE_END_SCALE{ 0.78f };

ASTRAL_NO_DESTROY gui::window g_gui{ };

gui::theme_t g_theme{ };

namespace gui
{
    void draw_nextgen_header_effect( rect_t bounds, float phase, u8 opacity,
                                     i32 left_inset, i32 right_inset )
    {
        const i32 left{ bounds.left + left_inset };
        const i32 right{ bounds.right - right_inset };
        const i32 width{ right - left };
        const i32 height{ bounds.bottom - bounds.top };
        if ( width <= 4 || height <= 4 || !opacity )
        {
            return;
        }

        const i32 lanes{ height >= g_gui.scaled( 24 ) ? 2 : 1 };
        const i32 steps{ math::clamp( width / math::bigger( g_gui.scaled( 12 ), 1 ), 6, 18 ) };
        for ( i32 lane{ }; lane < lanes; ++lane )
        {
            point_t previous{ left, bounds.top + height / 2 };
            for ( i32 step{ 1 }; step <= steps; ++step )
            {
                const i32 x{ left + width * step / steps };
                const float wave{ math::sin( static_cast< float >( step ) * 0.62f +
                                             phase * 0.38f + lane * 2.1f ) };
                const i32 reach{ math::bigger( 1, height / ( lane ? 6 : 5 ) ) };
                const i32 y{ bounds.top + height / 2 + static_cast< i32 >( wave * reach ) };
                const u8 alpha{ static_cast< u8 >( static_cast< u32 >( lane ? 70 : 95 ) * opacity / 255u ) };
                g_render.line( previous, point_t{ x, y }, color_t{ 255, 255, 255, alpha } );
                previous = point_t{ x, y };
            }
        }

        const i32 count{ math::clamp( width / math::bigger( g_gui.scaled( 28 ), 1 ), 3, 14 ) };
        for ( i32 index{ }; index < count; ++index )
        {
            const u64 drift{ static_cast< u64 >( phase * static_cast< float >( 7 + index % 3 * 3 ) ) };
            const i32 x{ left + static_cast< i32 >( ( index * 47u + drift ) % static_cast< u64 >( width ) ) };
            const float pulse{ 0.5f + 0.5f * math::sin( phase * ( 0.8f + index % 3 * 0.15f ) + index * 1.7f ) };
            const i32 usable_height{ math::bigger( height - 4, 1 ) };
            const i32 y{ bounds.top + 2 + ( index * 17 ) % usable_height };
            const i32 arm{ math::bigger( 1, g_gui.scaled( height >= 24 && index % 4 == 0 ? 3 : 2 ) ) };
            const u8 alpha{ static_cast< u8 >( ( 75.0f + pulse * 180.0f ) * opacity / 255.0f ) };
            const color_t star{ 255, 255, 255, alpha };
            g_render.line( point_t{ x - arm, y }, point_t{ x + arm, y }, star );
            g_render.line( point_t{ x, y - arm }, point_t{ x, y + arm }, star );
        }
    }

    void theme_t::set_accent( color_t accent )
    {
        constexpr color_t black{ 0, 0, 0 };
        constexpr color_t white{ 255, 255, 255 };

        this->m_accent     = accent;
        this->m_accent_alt = mix_color( accent, black, 20 );

        this->m_checkbox_top       = accent;
        this->m_checkbox_top_hover = mix_color( accent, white, 34 );

        this->m_slider_drag        = mix_color( accent, white, 40 );
        this->m_input_focus_border = mix_color( accent, black, 70 );

        this->m_selected_background     = mix_color( RAIL_ACTIVE_BACKGROUND, accent, 10 );
        this->m_row_selected_background = mix_color( POPUP_ROW_SELECTED, accent, 10 );
    }

    i32 window::scaled( i32 value ) const
    {
        const i32 design_width{ g_theme.m_nextgen_style ? NEXTGEN_BASE_WIDTH : MENU_BASE_WIDTH };
        return ( value * this->m_menu_width + design_width / 2 ) / design_width;
    }

    u8 window::opacity( ) const
    {
        const float progress{ ( this->m_scale - MENU_COLLAPSED_SCALE ) /
                              ( MENU_FADE_END_SCALE - MENU_COLLAPSED_SCALE ) };

        if ( progress <= 0.0f )
        {
            return 0;
        }

        if ( progress >= 1.0f )
        {
            return 255;
        }

        return static_cast< u8 >( math::smooth_step( progress ) * 255.0f + 0.5f );
    }

    color_t window::color( color_t value ) const
    {
        u32 alpha{ static_cast< u32 >( value.a ) * this->opacity( ) };
        alpha = ( alpha + 127u ) / 255u;

        if ( this->m_fade != 255 )
        {
            alpha = ( alpha * this->m_fade + 127u ) / 255u;
        }

        value.a = static_cast< u8 >( alpha );
        return value;
    }

    bool window::can_interact( ) const
    {
        return this->m_open && this->m_scale >= MENU_FULL_SCALE;
    }

    bool window::hit_test( rect_t bounds ) const
    {
        if ( !this->can_interact( ) )
        {
            return false;
        }

        const point_t mouse{ g_input.mouse( ) };

        if ( this->m_block_active && mouse.x >= this->m_block.left &&
             mouse.x < this->m_block.right && mouse.y >= this->m_block.top &&
             mouse.y < this->m_block.bottom )
        {
            return false; // an open popup owns this patch of screen
        }

        if ( this->m_hit_clip_active &&
             ( mouse.x < this->m_hit_clip.left || mouse.x >= this->m_hit_clip.right ||
               mouse.y < this->m_hit_clip.top || mouse.y >= this->m_hit_clip.bottom ) )
        {
            return false; // the visible part of a scrolled body ends here
        }

        return g_input.in_area( bounds.left, bounds.top, bounds.right - bounds.left - 1,
                                bounds.bottom - bounds.top - 1 );
    }

    i32 window::text_center_y( i32 top, i32 height ) const
    {
        return top + ( height - this->scaled( LABEL_FONT_INK_HEIGHT ) ) / 2 -
               this->scaled( LABEL_FONT_INK_TOP );
    }

    i32 window::tiny_center_y( i32 top, i32 height ) const
    {
        return top + ( height - this->scaled( TINY_FONT_INK_HEIGHT ) ) / 2 -
               this->scaled( TINY_FONT_INK_TOP );
    }
} // namespace gui

static float spring_position( float value, float target, float &velocity, u32 delta )
{
    u32   bounded_delta{ delta > 25u ? 25u : delta };
    float seconds{ static_cast< float >( bounded_delta ) / 1000.0f };
    float displacement{ target - value };
    float acceleration{ displacement * 240.0f - velocity * 24.0f };
    velocity += acceleration * seconds;
    value += velocity * seconds;

    if ( math::abs( target - value ) < 0.02f && math::abs( velocity ) < 0.02f )
    {
        value    = target;
        velocity = 0.0f;
    }

    return value;
}

namespace gui
{
    bool window::init( string_view title, string_view version )
    {
        if ( !g_arena.reserve( ) || !load_sounds( ) )
            return false;

        this->m_title.assign( title );
        this->m_version.assign( version );
        this->m_open = true;

        if ( !this->m_search )
        {
            this->m_search = make< text_input >( string_view{ ASTRAL_XORSTR_STATIC( "search" ) }, ICON_SEARCH,
                                                  true );
        }

        m_cursor_state = CURSOR_STATE_ARROW;

        return this->m_search != nullptr;
    }

    string_view window::search_filter( ) const
    {
        return this->m_search ? this->m_search->value( ) : string_view{ };
    }

    rect_t window::search_bounds( ) const
    {
        if ( g_theme.m_nextgen_style )
        {
            const i32 height{ this->scaled( 18 ) };
            const i32 right{ this->m_bounds.right - this->scaled( 34 ) };
            const i32 top{ this->m_bounds.bottom - this->scaled( NEXTGEN_FOOTER_HEIGHT ) +
                           ( this->scaled( NEXTGEN_FOOTER_HEIGHT ) - height ) / 2 };
            return rect_t{ right - this->scaled( 200 ), top, right, top + height };
        }

        const i32 top{ this->m_bounds.top +
                       ( this->scaled( MENU_HEADER_HEIGHT ) -
                         this->scaled( MENU_SEARCH_HEIGHT ) ) /
                           2 };
        const i32 right{ this->m_bounds.right - this->scaled( MENU_SEARCH_MARGIN ) };

        return rect_t{ right - this->scaled( MENU_SEARCH_WIDTH ), top, right,
                       top + this->scaled( MENU_SEARCH_HEIGHT ) };
    }

    rect_t window::header_action_bounds( ) const
    {
        if ( g_theme.m_nextgen_style )
        {
            const rect_t rail{ this->rail_bounds( ) };
            const i32 height{ this->scaled( 20 ) };
            const i32 top{ this->m_bounds.top + ( this->scaled( NEXTGEN_HEADER_HEIGHT ) - height ) / 2 };
            return rect_t{ rail.left - this->scaled( 62 ), top, rail.left - this->scaled( 8 ), top + height };
        }

        const rect_t search{ this->search_bounds( ) };
        const i32 gap{ this->scaled( 8 ) };
        const i32 width{ this->scaled( 54 ) };
        return rect_t{ search.left - gap - width, search.top, search.left - gap, search.bottom };
    }

    void window::set_header_action( string_view label, void ( *callback )( ) )
    {
        if ( this->m_header_action )
        {
            this->m_header_action->~button( );
        }
        this->m_header_action = make< button >( label, callback );
    }

    rect_t window::rail_bounds( ) const
    {
        if ( g_theme.m_nextgen_style )
        {
            const i32 count{ static_cast< i32 >( this->m_tabs.size( ) ) };
            return rect_t{ this->m_bounds.right - this->scaled( 18 + count * 36 ),
                           this->m_bounds.top, this->m_bounds.right - this->scaled( 11 ),
                           this->m_bounds.top + this->scaled( NEXTGEN_HEADER_HEIGHT ) };
        }

        return rect_t{ this->m_bounds.left,
                       this->m_bounds.top + this->scaled( MENU_HEADER_HEIGHT ),
                       this->m_bounds.left + this->scaled( MENU_RAIL_WIDTH ),
                       this->m_bounds.bottom - this->scaled( MENU_FOOTER_HEIGHT ) };
    }

    rect_t window::rail_foot_bounds( ) const
    {
        const rect_t rail{ this->rail_bounds( ) };
        if ( g_theme.m_nextgen_style )
        {
            const i32 index{ static_cast< i32 >( this->m_foot_tab ) };
            return rect_t{ rail.left + this->scaled( index * 36 ), rail.top,
                           rail.left + this->scaled( index * 36 + 30 ), rail.bottom };
        }
        const i32    top{ rail.top + this->scaled( MENU_RAIL_TOP_PADDING ) +
                          this->scaled( MENU_RAIL_FOOT_OFFSET ) };

        return rect_t{ rail.left, top, rail.right - 1, top + this->scaled( MENU_RAIL_ITEM_HEIGHT ) };
    }

    rect_t window::subnav_bounds( ) const
    {
        if ( g_theme.m_nextgen_style )
        {
            return rect_t{ this->m_bounds.left + this->scaled( NEXTGEN_PAGE_PADDING ),
                           this->m_bounds.top + this->scaled( NEXTGEN_HEADER_HEIGHT + NEXTGEN_GAP ),
                           this->m_bounds.right - this->scaled( NEXTGEN_PAGE_PADDING ),
                           this->m_bounds.top + this->scaled( NEXTGEN_HEADER_HEIGHT + NEXTGEN_GAP + NEXTGEN_CARD_HEIGHT ) };
        }

        const rect_t rail{ this->rail_bounds( ) };

        return rect_t{ rail.right, rail.top,
                       rail.right + this->scaled( MENU_SUBNAV_WIDTH ), rail.bottom };
    }

    void window::draw_search( ) const
    {
        if ( this->m_search )
        {
            this->m_search->draw( );
        }
    }

    tab *window::add_tab( string_view label, const char *icon, bool rail_foot )
    {
        Assert( this->m_foot_tab == STRIP_CAPACITY );

        tab *child{ make< tab >( label, icon ) };

        if ( !child || !this->m_tabs.push_back( child ) )
            return nullptr;

        if ( rail_foot )
        {
            this->m_foot_tab = this->m_tabs.size( ) - 1;
        }

        this->m_layout_dirty = true;
        return child;
    }

    void window::shutdown( )
    {
        for ( tab *child : this->m_tabs )
        {
            child->destroy( );
            child->~tab( );
        }

        this->m_tabs.clear( );

        if ( this->m_search )
        {
            this->m_search->~text_input( );
            this->m_search = nullptr;
        }

        if ( this->m_header_action )
        {
            this->m_header_action->~button( );
            this->m_header_action = nullptr;
        }

        g_arena.release( );
    }

    sub_tab *window::current_subtab( ) const
    {
        if ( this->m_selected_tab >= this->m_tabs.size( ) )
        {
            return nullptr;
        }

        return this->m_tabs[ this->m_selected_tab ]->subtab( this->m_selected_subnav );
    }

    bool window::evaluate_all( )
    {
        bool changed{ };

        for ( tab *entry : this->m_tabs )
        {
            for ( usize index{ }; index < entry->subtab_count( ); ++index )
            {
                if ( sub_tab * child{ entry->subtab( index ) } )
                {
                    changed |= child->evaluate( );
                }
            }
        }

        return changed;
    }

    bool window::find_match( usize &tab_index, usize &subnav_index ) const
    {
        for ( usize outer{ }; outer < this->m_tabs.size( ); ++outer )
        {
            const tab *entry{ this->m_tabs[ outer ] };

            for ( usize inner{ }; inner < entry->subtab_count( ); ++inner )
            {
                const sub_tab *child{ entry->subtab( inner ) };

                if ( child && child->has_visible( ) )
                {
                    tab_index    = outer;
                    subnav_index = inner;
                    return true;
                }
            }
        }

        return false;
    }

    void window::update_visibility( )
    {
        if ( g_input.pressed( VK_INSERT ) || g_input.pressed( VK_F3 ) )
        {
            if ( this->m_open && this->m_close_guard && !this->m_close_guard( ) )
            {
                this->m_close_pending = true;
            } else
            {
                this->m_open          = !this->m_open;
                this->m_close_pending = false;
                this->m_dragging      = false;
                this->m_focused_input = nullptr;
                this->m_listening     = nullptr;

                play_sound( this->m_open ? SOUND_MENU_OPEN : SOUND_MENU_CLOSE );
                if ( !this->m_open && this->m_close_callback )
                {
                    this->m_close_callback( );
                }
            }
        }

        if ( this->m_close_pending && ( !this->m_close_guard || this->m_close_guard( ) ) )
        {
            this->m_open          = false;
            this->m_close_pending = false;
            this->m_dragging      = false;
            this->m_focused_input = nullptr;
            this->m_listening     = nullptr;
            play_sound( SOUND_MENU_CLOSE );
            if ( this->m_close_callback )
            {
                this->m_close_callback( );
            }
        }

        g_input.block( this->m_open );

        const u32 delta{ animation_delta( this->m_scale_tick ) };
        step_mix( this->m_scale_mix, this->m_open, delta,
                  this->m_open ? MENU_OPEN_DURATION : MENU_CLOSE_DURATION );

        const u8 eased{ eased_out( this->m_scale_mix ) };

        this->m_scale = eased == 255 ? MENU_FULL_SCALE
                        : eased == 0 ? MENU_COLLAPSED_SCALE
                                     : MENU_COLLAPSED_SCALE +
                                           ( MENU_FULL_SCALE - MENU_COLLAPSED_SCALE ) *
                                               static_cast< float >( eased ) / 255.0f;
    }

    void window::update_position( i32 screen_width, i32 screen_height, i32 menu_height )
    {
        const i32 maximum_x{ screen_width - this->m_menu_width };
        const i32 maximum_y{ screen_height - menu_height };

        if ( !this->m_position_initialized )
        {
            this->m_position.x           = maximum_x / 2;
            this->m_position.y           = maximum_y / 2;
            this->m_position_initialized = true;
        }

        this->m_position.x = math::clamp( this->m_position.x, 0, maximum_x );
        this->m_position.y = math::clamp( this->m_position.y, 0, maximum_y );

        const i32     drag_left{ this->m_position.x + this->scaled( MENU_HEADER_PADDING ) };
        const i32     drag_right{ this->m_position.x + this->m_menu_width -
                                  this->scaled( g_theme.m_nextgen_style ? 18 + static_cast< i32 >( this->m_tabs.size( ) ) * 36
                                                                       : MENU_SEARCH_MARGIN + MENU_HEADER_GAP + MENU_SEARCH_WIDTH ) };
        const i32     header_height{ this->scaled( g_theme.m_nextgen_style ? NEXTGEN_HEADER_HEIGHT : MENU_HEADER_HEIGHT ) };
        const point_t mouse{ g_input.mouse( ) };

        if ( !this->m_dragging && this->m_scale >= MENU_FULL_SCALE &&
             g_input.pressed( VK_LBUTTON ) && drag_right > drag_left &&
             g_input.in_area( drag_left, this->m_position.y, drag_right - drag_left - 1,
                              header_height - 1 ) )
        {
            this->m_drag_offset.x = mouse.x - this->m_position.x;
            this->m_drag_offset.y = mouse.y - this->m_position.y;
            this->m_dragging      = true;
        }

        if ( !this->m_dragging )
        {
            return;
        }

        if ( !g_input.down( VK_LBUTTON ) )
        {
            this->m_dragging = false;
            return;
        }

        this->m_position.x = math::clamp( mouse.x - this->m_drag_offset.x, 0, maximum_x );
        this->m_position.y = math::clamp( mouse.y - this->m_drag_offset.y, 0, maximum_y );
    }

    void window::layout( rect_t content )
    {
        if ( sub_tab * active{ this->current_subtab( ) } )
        {
            active->layout( content );
        }
    }

    void window::drive_rail( )
    {
        strip_state_t &state{ this->m_tab_state };

        const rect_t rail{ this->rail_bounds( ) };
        const usize  count{ this->m_tabs.size( ) };
        const i32    item_height{ this->scaled( g_theme.m_nextgen_style ? NEXTGEN_HEADER_HEIGHT : MENU_RAIL_ITEM_HEIGHT ) };
        const i32    items_top{ rail.top + this->scaled( g_theme.m_nextgen_style ? 0 : MENU_RAIL_TOP_PADDING ) };
        const bool   enabled{ this->can_interact( ) };

        const u32 delta{ animation_delta( state.m_last_tick ) };
        const u8  active_step{ animation_step( delta, STRIP_ACTIVE_DURATION ) };
        const u8  hover_step{ animation_step( delta, STRIP_HOVER_DURATION ) };

        const float indicator_target{ static_cast< float >(
            this->m_selected_tab == this->m_foot_tab
                ? MENU_RAIL_FOOT_OFFSET
                : static_cast< i32 >( this->m_selected_tab ) * MENU_RAIL_ITEM_HEIGHT ) };

        if ( !state.m_initialized )
        {
            state.m_indicator_offset   = indicator_target;
            state.m_indicator_velocity = 0.0f;
            state.m_initialized        = true;

            if ( this->m_selected_tab < STRIP_CAPACITY )
            {
                state.m_active_mix[ this->m_selected_tab ] = 255;
            }
        }

        state.m_indicator_offset = spring_position( state.m_indicator_offset, indicator_target,
                                                    state.m_indicator_velocity, delta );

        usize hovered_item{ STRIP_CAPACITY };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const i32    item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const rect_t item{ g_theme.m_nextgen_style
                                   ? rect_t{ rail.left + this->scaled( static_cast< i32 >( index ) * 36 ), rail.top,
                                             rail.left + this->scaled( static_cast< i32 >( index ) * 36 + 30 ), rail.bottom }
                               : index == this->m_foot_tab
                                   ? this->rail_foot_bounds( )
                                   : rect_t{ rail.left, item_top, rail.right - 1,
                                             item_top + item_height } };

            const bool hovered{ enabled && g_input.in_area( item.left, item.top, item.right - item.left, item.bottom - item.top - 1 ) };
            if ( hovered )
            {
                set_cursor_state( CURSOR_STATE_CLICK_HAND );
                hovered_item = index;
            }

            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                if ( index != this->m_selected_tab )
                {
                    play_sound( SOUND_TAB );
                }

                this->m_selected_tab    = index;
                this->m_selected_subnav = 0;
            }

            state.m_active_mix[ index ] =
                math::approach( state.m_active_mix[ index ],
                                static_cast< u8 >( index == this->m_selected_tab ? 255 : 0 ),
                                active_step );
            state.m_hover_mix[ index ] =
                math::approach( state.m_hover_mix[ index ],
                                static_cast< u8 >( hovered ? 255 : 0 ), hover_step );
        }

        if ( hovered_item != state.m_hovered && hovered_item != STRIP_CAPACITY )
        {
            play_sound( SOUND_HOVER );
        }

        state.m_hovered = hovered_item;
    }

    void window::drive_subnav( )
    {
        const tab *owner{ this->m_selected_tab < this->m_tabs.size( )
                              ? this->m_tabs[ this->m_selected_tab ]
                              : nullptr };

        if ( !owner )
        {
            return;
        }

        const usize count{ owner->subtab_count( ) };

        if ( this->m_selected_subnav >= count )
        {
            this->m_selected_subnav = 0;
        }

        strip_state_t &state{ this->m_subnav_state };

        if ( g_theme.m_nextgen_style )
        {
            const rect_t card{ this->subnav_bounds( ) };
            const bool card_hovered{ this->can_interact( ) &&
                g_input.in_area( card.left, card.top, card.right - card.left - 1,
                                 card.bottom - card.top - 1 ) };

            if ( card_hovered ) this->set_cursor_state( CURSOR_STATE_CLICK_HAND );
            if ( card_hovered && g_input.pressed( VK_LBUTTON ) )
            {
                this->m_nextgen_subnav_open = !this->m_nextgen_subnav_open;
                play_sound( SOUND_TAB );
            }

            if ( this->m_nextgen_subnav_open )
            {
                const u8 hover_step{ animation_step( animation_delta( state.m_last_tick ),
                                                     STRIP_HOVER_DURATION ) };
                const i32 row_height{ this->scaled( 20 ) };
                const i32 list_top{ card.top + this->scaled( 32 ) };
                const rect_t popup{ card.left, card.top, card.right,
                                    list_top + static_cast< i32 >( count ) * row_height + this->scaled( 8 ) };
                this->block_hits( popup );

                bool inside_popup{ g_input.in_area( popup.left, popup.top,
                                                     popup.right - popup.left - 1,
                                                     popup.bottom - popup.top - 1 ) };
                for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
                {
                    const rect_t item{ popup.left + this->scaled( 12 ),
                                       list_top + static_cast< i32 >( index ) * row_height,
                                       popup.right - this->scaled( 12 ),
                                       list_top + static_cast< i32 >( index + 1 ) * row_height };
                    const bool hovered{ this->can_interact( ) &&
                        g_input.in_area( item.left, item.top, item.right - item.left - 1,
                                         item.bottom - item.top - 1 ) };
                    state.m_hover_mix[ index ] = math::approach(
                        state.m_hover_mix[ index ], static_cast< u8 >( hovered ? 255 : 0 ),
                        hover_step );
                    if ( hovered ) this->set_cursor_state( CURSOR_STATE_CLICK_HAND );
                    if ( hovered && g_input.pressed( VK_LBUTTON ) )
                    {
                        if ( index != this->m_selected_subnav ) play_sound( SOUND_TAB );
                        this->m_selected_subnav = index;
                        this->m_nextgen_subnav_open = false;
                    }
                }

                if ( this->can_interact( ) && g_input.pressed( VK_LBUTTON ) &&
                     !inside_popup && !card_hovered )
                {
                    this->m_nextgen_subnav_open = false;
                }
            }

            for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
            {
                state.m_active_mix[ index ] = index == this->m_selected_subnav ? 255 : 0;
            }
            return;
        }

        const rect_t subnav{ this->subnav_bounds( ) };
        const i32    item_height{ this->scaled( g_theme.m_nextgen_style ? NEXTGEN_CARD_HEIGHT : MENU_SUBNAV_ITEM_HEIGHT ) };
        const i32    items_top{ subnav.top + this->scaled( g_theme.m_nextgen_style ? 0 : MENU_SUBNAV_TOP_PADDING ) };
        const bool   enabled{ this->can_interact( ) };

        const u32 delta{ animation_delta( state.m_last_tick ) };
        const u8  active_step{ animation_step( delta, STRIP_ACTIVE_DURATION ) };
        const u8  hover_step{ animation_step( delta, STRIP_HOVER_DURATION ) };

        const float indicator_target{ static_cast< float >( this->m_selected_subnav ) *
                                      static_cast< float >( MENU_SUBNAV_ITEM_HEIGHT ) };

        if ( !state.m_initialized )
        {
            state.m_indicator_offset   = indicator_target;
            state.m_indicator_velocity = 0.0f;
            state.m_initialized        = true;

            if ( this->m_selected_subnav < STRIP_CAPACITY )
            {
                state.m_active_mix[ this->m_selected_subnav ] = 255;
            }
        }

        state.m_indicator_offset = spring_position( state.m_indicator_offset, indicator_target,
                                                    state.m_indicator_velocity, delta );

        usize hovered_item{ STRIP_CAPACITY };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const i32 item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const i32 item_width{ count ? ( subnav.right - subnav.left ) / static_cast< i32 >( count ) : 0 };
            const rect_t item{ g_theme.m_nextgen_style
                                   ? rect_t{ subnav.left + static_cast< i32 >( index ) * item_width, subnav.top,
                                             index + 1 == count ? subnav.right : subnav.left + static_cast< i32 >( index + 1 ) * item_width,
                                             subnav.bottom }
                                   : rect_t{ subnav.left, item_top, subnav.right, item_top + item_height } };
            const bool hovered{ enabled && g_input.in_area( item.left, item.top, item.right - item.left - 1, item.bottom - item.top - 1 ) };

            if ( hovered )
            {
                set_cursor_state( CURSOR_STATE_CLICK_HAND );
                hovered_item = index;
            }

            if ( hovered && g_input.pressed( VK_LBUTTON ) )
            {
                if ( index != this->m_selected_subnav )
                {
                    play_sound( SOUND_TAB );
                }

                this->m_selected_subnav = index;
            }

            state.m_active_mix[ index ] =
                math::approach( state.m_active_mix[ index ],
                                static_cast< u8 >( index == this->m_selected_subnav ? 255 : 0 ),
                                active_step );
            state.m_hover_mix[ index ] =
                math::approach( state.m_hover_mix[ index ],
                                static_cast< u8 >( hovered ? 255 : 0 ), hover_step );
        }

        if ( hovered_item != state.m_hovered && hovered_item != STRIP_CAPACITY )
        {
            play_sound( SOUND_HOVER );
        }

        state.m_hovered = hovered_item;
    }

    void window::think( i32 screen_width, i32 screen_height )
    {
        this->m_screen_width  = screen_width;
        this->m_screen_height = screen_height;

        step_mix( this->m_background_mix, this->m_open,
                  animation_delta( this->m_background_tick ), BACKGROUND_FADE_DURATION );

        ++this->m_frame;
        if ( g_theme.m_nextgen_style && this->m_open )
        {
            const u32 header_delta{ animation_delta( this->m_nextgen_header_tick ) };
            const u32 capped_delta{ header_delta > 33u ? 33u : header_delta };
            this->m_nextgen_header_time += static_cast< float >( capped_delta ) * 0.001f;
        } else
        {
            // Keep the clock current while this style is inactive so switching styles cannot
            // inject the entire idle interval into a single animation frame.
            animation_delta( this->m_nextgen_header_tick );
        }
        // a popup that stopped renewing its block last frame releases the screen it owned
        if ( !this->m_block_renewed )
        {
            this->m_block_active = false;
        }

        this->m_block_renewed = false;

        this->update_visibility( );
        this->set_cursor_state( CURSOR_STATE_ARROW );

        const i32 available_width{ screen_width - MENU_SCREEN_MARGIN };
        const i32 available_height{ screen_height - MENU_SCREEN_MARGIN };

        if ( available_width <= 0 || available_height <= 0 )
        {
            return;
        }

        const i32 design_width{ g_theme.m_nextgen_style ? NEXTGEN_BASE_WIDTH : MENU_BASE_WIDTH };
        const i32 design_height{ g_theme.m_nextgen_style ? NEXTGEN_BASE_HEIGHT : MENU_BASE_HEIGHT };
        const i32 desired_width{ design_width * this->m_dpi_percent / 100 };
        const i32 width_from_height{ available_height * design_width / design_height };
        const i32 menu_width{ math::smaller(
            desired_width, math::smaller( available_width, width_from_height ) ) };

        if ( menu_width <= 0 )
        {
            return;
        }

        this->m_menu_width = menu_width;

        const i32 menu_height{ ( design_height * menu_width + design_width / 2 ) / design_width };

        if ( !this->m_open && this->m_scale <= MENU_COLLAPSED_SCALE )
        {
            return;
        }

        if ( this->m_open )
        {
            this->update_position( screen_width, screen_height, menu_height );
        } else
        {
            this->m_dragging = false;
        }

        this->m_bounds = rect_t{ this->m_position.x, this->m_position.y,
                                 this->m_position.x + menu_width,
                                 this->m_position.y + menu_height };

        this->drive_rail( );
        this->drive_subnav( );

        bool query_changed{ };

        if ( this->m_search )
        {
            if ( g_theme.m_nextgen_style )
            {
                const i32 icon_size{ this->scaled( 22 ) };
                const rect_t toggle{ this->m_bounds.right - this->scaled( 29 ),
                                     this->m_bounds.bottom - this->scaled( NEXTGEN_FOOTER_HEIGHT ) +
                                         ( this->scaled( NEXTGEN_FOOTER_HEIGHT ) - icon_size ) / 2,
                                     this->m_bounds.right - this->scaled( 7 ),
                                     this->m_bounds.bottom - this->scaled( NEXTGEN_FOOTER_HEIGHT ) +
                                         ( this->scaled( NEXTGEN_FOOTER_HEIGHT ) + icon_size ) / 2 };
                if ( this->hit_test( toggle ) )
                {
                    this->set_cursor_state( CURSOR_STATE_CLICK_HAND );
                    if ( g_input.pressed( VK_LBUTTON ) )
                    {
                        this->m_nextgen_search_open = !this->m_nextgen_search_open;
                        if ( !this->m_nextgen_search_open ) this->m_search->set_value( { } );
                        play_sound( SOUND_TAB );
                    }
                }
            }

            this->m_search->set_bounds( this->search_bounds( ) );
            if ( !g_theme.m_nextgen_style || this->m_nextgen_search_open )
            {
                this->m_search->think( );
            }

            const usize length{ this->m_search->value( ).size( ) };

            if ( length != this->m_search_length )
            {
                // remember where the user was, so clearing the box puts them back
                if ( !this->m_search_length )
                {
                    this->m_search_return_tab    = this->m_selected_tab;
                    this->m_search_return_subnav = this->m_selected_subnav;
                }

                this->m_search_length = length;
                this->m_layout_dirty  = true;
                query_changed         = true;
            }
        }

        if ( this->m_header_action )
        {
            this->m_header_action->set_bounds( this->header_action_bounds( ) );
            this->m_header_action->think( );
        }

        const rect_t content{ g_theme.m_nextgen_style
            ? rect_t{ this->m_bounds.left + this->scaled( NEXTGEN_PAGE_PADDING ),
                      this->m_bounds.top + this->scaled( NEXTGEN_HEADER_HEIGHT + NEXTGEN_GAP + NEXTGEN_CARD_HEIGHT + NEXTGEN_GAP ),
                      this->m_bounds.right - this->scaled( NEXTGEN_PAGE_PADDING ),
                      this->m_bounds.bottom - this->scaled( NEXTGEN_FOOTER_HEIGHT + NEXTGEN_GAP ) }
            : rect_t{ this->m_bounds.left + this->scaled( MENU_RAIL_WIDTH + MENU_SUBNAV_WIDTH + MENU_CONTENT_PADDING ),
                      this->m_bounds.top + this->scaled( MENU_HEADER_HEIGHT + MENU_CONTENT_PADDING ),
                      this->m_bounds.right - this->scaled( MENU_CONTENT_PADDING_RIGHT ),
                      this->m_bounds.bottom - this->scaled( MENU_FOOTER_HEIGHT + MENU_CONTENT_PADDING ) } };

        // nextgen: the subsection dropdown card sits above the content. Keep the content room
        // starting below it so scrolled cards can never paint over the dropdown.
        this->m_content_room = rect_t{ content.left,
                                       g_theme.m_nextgen_style ? content.top
                                                               : this->m_bounds.top + this->scaled( MENU_HEADER_HEIGHT ),
                                       content.right,
                                       this->m_bounds.bottom - this->scaled( g_theme.m_nextgen_style ? NEXTGEN_FOOTER_HEIGHT
                                                                                                     : MENU_FOOTER_HEIGHT ) };

        const bool searching{ this->m_search_length > 0 };

        if ( searching || query_changed )
        {
            if ( this->evaluate_all( ) )
            {
                this->m_layout_dirty = true;
            }
        } else if ( sub_tab * current{ this->current_subtab( ) } )
        {
            if ( current->evaluate( ) )
            {
                this->m_layout_dirty = true;
            }
        }

        if ( query_changed )
        {
            if ( searching )
            {
                // only walk the user somewhere else if where they are came up empty
                const sub_tab *here{ this->current_subtab( ) };

                if ( !here || !here->has_visible( ) )
                {
                    usize found_tab{ };
                    usize found_subnav{ };

                    if ( this->find_match( found_tab, found_subnav ) )
                    {
                        this->m_selected_tab    = found_tab;
                        this->m_selected_subnav = found_subnav;
                    }
                }
            } else
            {
                this->m_selected_tab    = this->m_search_return_tab;
                this->m_selected_subnav = this->m_search_return_subnav;
            }
        }

        sub_tab *active{ this->current_subtab( ) };

        if ( !active )
        {
            return;
        }

        const bool selection_moved{ this->m_layout_tab != this->m_selected_tab ||
                                    this->m_layout_subnav != this->m_selected_subnav };
        const bool content_moved{ content.left != this->m_layout_content.left ||
                                  content.top != this->m_layout_content.top ||
                                  content.right != this->m_layout_content.right ||
                                  content.bottom != this->m_layout_content.bottom };

        if ( this->m_layout_dirty || content_moved || selection_moved )
        {
            this->layout( content );
            this->m_layout_content = content;
            this->m_layout_tab     = this->m_selected_tab;
            this->m_layout_subnav  = this->m_selected_subnav;
            this->m_layout_dirty   = false;
        }

        if ( selection_moved )
        {
            this->m_content_mix = 0; // the new page slides in rather than snapping
        }

        step_mix( this->m_content_mix, true, animation_delta( this->m_content_tick ),
                  CONTENT_ENTER_DURATION );

        active->think( );

        if ( baseobject * panel{ active->has_visible( ) ? active->panel( ) : nullptr } )
        {
            const rect_t bounds{ this->panel_bounds( active ) };

            if ( g_theme.m_dock_panels )
            {
                this->m_panel.place( point_t{ bounds.left, bounds.top } );
            } else
            {
                this->m_panel.drag( rect_t{ bounds.left, bounds.top, bounds.right, bounds.top + this->scaled( PANEL_HEADER_HEIGHT ) },
                                    this->m_screen_width, this->m_screen_height, bounds.right - bounds.left, bounds.bottom - bounds.top );
                this->m_panel.push_out( this->m_bounds, this->scaled( PANEL_DOCK_GAP ), this->m_screen_width, this->m_screen_height,
                                        bounds.right - bounds.left, bounds.bottom - bounds.top );
            }

            panel->set_bounds( this->panel_body( this->panel_bounds( active ) ) );
            panel->think( );
        }
    }

    void window::draw_background( )
    {
        if ( !this->m_open && this->m_scale <= MENU_COLLAPSED_SCALE )
        {
            return;
        }

        const rect_t screen{ 0, 0, this->m_screen_width, this->m_screen_height };
        const u8     background{ eased_mix( this->m_background_mix ) };

        if ( g_theme.m_blur_background )
        {
            g_render.blur( screen, background );
        }

        if ( g_theme.m_astral_effect )
        {
            g_render.astral( screen, this->m_bounds, g_theme.m_accent, background );
        }

        draw_menu_particles( screen, background );
    }

    void window::draw( )
    {
        if ( !this->m_open && this->m_scale <= MENU_COLLAPSED_SCALE )
        {
            return;
        }

        const u8 shown{ this->opacity( ) };

        const point_t origin{ ( this->m_bounds.left + this->m_bounds.right ) / 2,
                              ( this->m_bounds.top + this->m_bounds.bottom ) / 2 };

        g_render.set_transform( origin, this->m_scale,
                                static_cast< float >( this->m_menu_width ) /
                                    static_cast< float >( g_theme.m_nextgen_style ? NEXTGEN_BASE_WIDTH : MENU_BASE_WIDTH ) );

        const sub_tab *active{ this->current_subtab( ) };
        const bool     panelled{ active && active->panel( ) && active->has_visible( ) };

        const i32     spread{ this->scaled( MENU_SHADOW_SPREAD ) };
        const i32     drop{ this->scaled( MENU_SHADOW_DROP ) };
        const color_t shadow{ this->color( g_theme.m_shadow ) };

        draw_drop_shadow( this->m_bounds, spread, drop, shadow );

        if ( panelled )
        {
            draw_drop_shadow( this->panel_bounds( active ), spread, drop, shadow );
        }

        g_render.blur( this->m_bounds, shown );

        this->draw_shell( );
        this->draw_header( );
        this->draw_rail( );
        this->draw_subnav( );

        if ( active )
        {
            this->m_fade = eased_mix( this->m_content_mix );

            if ( active->has_visible( ) )
            {
                active->draw( );
            } else
            {
                this->draw_empty_page( );
            }

            if ( panelled )
            {
                this->draw_panel( active );
            }

            this->m_fade = 255;
        }

        this->draw_footer( );
        this->draw_frame( );

        if ( !g_theme.m_nextgen_style )
        {
            draw_menu_decorations( this->m_bounds, this->subnav_bounds( ),
                                   this->scaled( MENU_DECORATION_SIZE ), shown );
        }

        if ( active && active->has_visible( ) )
        {
            this->m_fade = eased_mix( this->m_content_mix );

            active->draw_last( );

            if ( panelled )
            {
                active->panel( )->draw_last( );
            }

            this->m_fade = 255;
        }

        this->draw_subnav_popup( );

        g_render.reset_transform( );
    }

    // @ a page with nothing on it says so instead of leaving the reader staring at the shell.
    // It rides the same enter animation the cards do, so switching onto it does not snap
    void window::draw_empty_page( ) const
    {
        const u8  entered{ eased_mix( this->m_content_mix ) };
        const i32 rise{ this->scaled( CONTENT_ENTER_RISE ) * ( 255 - entered ) / 255 };

        const i32 left{ this->m_content_room.left + this->scaled( MENU_CONTENT_PADDING ) };
        const i32 top{ this->m_content_room.top + this->scaled( MENU_CONTENT_PADDING ) + rise };

        if ( this->m_selected_tab < this->m_tabs.size( ) )
        {
            g_render.text( MENU_FONT( ), point_t{ left, top },
                           this->color( MENU_TEXT_DIM ),
                           this->m_tabs[ this->m_selected_tab ]->label( ) );
        }

        g_render.text( MENU_FONT( ),
                       point_t{ left, top + this->scaled( WIDGET_ROW_HEIGHT ) },
                       this->color( MENU_TEXT_FAINT ),
                       ASTRAL_XORSTR( "Nothing to configure here yet." ).get( ) );
    }

    void window::draw_shell( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const rect_t rail{ this->rail_bounds( ) };
        const rect_t subnav{ this->subnav_bounds( ) };
        const i32    header_bottom{ bounds.top + this->scaled( g_theme.m_nextgen_style ? NEXTGEN_HEADER_HEIGHT : MENU_HEADER_HEIGHT ) };
        const i32    footer_top{ bounds.bottom - this->scaled( g_theme.m_nextgen_style ? NEXTGEN_FOOTER_HEIGHT : MENU_FOOTER_HEIGHT ) };

        const i32 corner{ this->scaled( 6 ) };
        if ( g_theme.m_nextgen_style )
            fill_rounded_box( bounds, corner, this->color( MENU_BACKGROUND ) );
        else
            g_render.box( bounds, this->color( MENU_BACKGROUND ), true );
        if ( g_theme.m_nextgen_style )
        {
            fill_rounded_horizontal_gradient(
                rect_t{ bounds.left, bounds.top, bounds.right, header_bottom + corner }, corner,
                this->color( g_theme.m_accent ), this->color( g_theme.m_accent_end ) );
            g_render.box( rect_t{ bounds.left, header_bottom, bounds.right,
                                  header_bottom + corner }, this->color( MENU_BACKGROUND ), true );
        } else
        {
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right, header_bottom },
                          this->color( MENU_HEADER_BACKGROUND ), true );
        }
        if ( !g_theme.m_nextgen_style )
        {
            g_render.box( rail, this->color( MENU_RAIL_BACKGROUND ), true );
            g_render.box( subnav, this->color( MENU_SUBNAV_BACKGROUND ), true );
        } else
        {
            g_render.box( subnav, this->color( MENU_HEADER_BACKGROUND ), true );
        }
        if ( g_theme.m_nextgen_style )
        {
            fill_rounded_box( rect_t{ bounds.left, footer_top, bounds.right, bounds.bottom },
                              corner, this->color( MENU_HEADER_BACKGROUND ) );
            g_render.box( rect_t{ bounds.left, footer_top, bounds.right,
                                  bounds.bottom - corner },
                          this->color( MENU_HEADER_BACKGROUND ), true );
        } else
            g_render.box( rect_t{ bounds.left, footer_top, bounds.right, bounds.bottom },
                          this->color( MENU_HEADER_BACKGROUND ), true );

        // Dark Aero glass: cool reflected light along the chrome and a faint
        // submerged glow behind the content without sacrificing contrast.
        g_render.box( rect_t{ bounds.left + ( g_theme.m_nextgen_style ? corner : 1 ), bounds.top + 1,
                              bounds.right - ( g_theme.m_nextgen_style ? corner : 1 ),
                              bounds.top + this->scaled( 3 ) },
                      this->color( g_theme.m_nextgen_style ? color_t{ 255, 255, 255, 38 }
                                                          : color_t{ 113, 242, 210, 34 } ), true );
        if ( !g_theme.m_nextgen_style )
        {
            g_render.box( rect_t{ subnav.right, header_bottom,
                                  bounds.right, header_bottom + this->scaled( 22 ) },
                          this->color( color_t{ 21, 124, 145, 18 } ), true );
            g_render.box( rect_t{ subnav.right, footer_top - this->scaled( 18 ),
                                  bounds.right, footer_top },
                          this->color( color_t{ 38, 181, 126, 10 } ), true );
        }
    }

    void window::draw_frame( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const rect_t rail{ this->rail_bounds( ) };
        const rect_t subnav{ this->subnav_bounds( ) };
        const i32    header_bottom{ bounds.top + this->scaled( g_theme.m_nextgen_style ? NEXTGEN_HEADER_HEIGHT : MENU_HEADER_HEIGHT ) };
        const i32    footer_top{ bounds.bottom - this->scaled( g_theme.m_nextgen_style ? NEXTGEN_FOOTER_HEIGHT : MENU_FOOTER_HEIGHT ) };

        g_render.line( point_t{ bounds.left + 1, header_bottom - 1 },
                       point_t{ bounds.right - 2, header_bottom - 1 },
                       this->color( MENU_SECTION_SEPARATOR ) );
        if ( !g_theme.m_nextgen_style )
        {
            g_render.line( point_t{ rail.right - 1, rail.top },
                           point_t{ rail.right - 1, rail.bottom - 1 },
                           this->color( MENU_SUBNAV_SEPARATOR ) );
            g_render.line( point_t{ subnav.right - 1, subnav.top },
                           point_t{ subnav.right - 1, subnav.bottom - 1 },
                           this->color( MENU_SUBNAV_SEPARATOR ) );
        }
        g_render.line( point_t{ bounds.left + 1, footer_top },
                       point_t{ bounds.right - 2, footer_top },
                       this->color( MENU_SECTION_SEPARATOR ) );

        if ( g_theme.m_nextgen_style )
            stroke_rounded_box( bounds, this->scaled( 6 ), this->color( MENU_OUTER_BORDER ) );
        else
            g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                          this->color( MENU_OUTER_BORDER ) );
    }

    void window::draw_header( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const i32    height{ this->scaled( g_theme.m_nextgen_style ? NEXTGEN_HEADER_HEIGHT : MENU_HEADER_HEIGHT ) };
        const i32    gap{ this->scaled( MENU_HEADER_GAP ) };

        i32 cursor{ bounds.left + this->scaled( MENU_HEADER_PADDING ) };

        if ( g_theme.m_nextgen_style )
        {
            const i32 text_y{ this->text_center_y( bounds.top, height ) };
            g_render.text( MENU_FONT_BOLD( ), point_t{ cursor, text_y },
                           this->color( color_t{ 255, 255, 255 } ), this->m_title );

            const rect_t rail{ this->rail_bounds( ) };
            const i32 band_left{ cursor + estimate_bold_text_width( this->m_title ) + this->scaled( 8 ) };
            const i32 band_right{ rail.left - this->scaled( 5 ) };
            const float phase{ this->m_nextgen_header_time };

            draw_nextgen_header_effect( rect_t{ band_left, bounds.top, band_right, bounds.top + height },
                                        phase, this->opacity( ) );

            if ( this->m_header_action ) this->m_header_action->draw( );
            return;
        }

        draw_logo( point_t{ cursor, bounds.top +
                                        ( height - this->scaled( MENU_LOGO_SIZE ) ) / 2 },
                   this->color( MENU_LOGO_GLYPH ) );

        cursor += this->scaled( MENU_LOGO_SIZE ) + gap;

        const i32 text_y{ this->text_center_y( bounds.top, height ) };

        g_render.text( MENU_FONT_BOLD( ), point_t{ cursor, text_y },
                       this->color( MENU_WORDMARK ), this->m_title );

        cursor += estimate_bold_text_width( this->m_title ) + gap;

        if ( !this->m_version.empty( ) )
        {
            g_render.text( MENU_FONT_TINY( ),
                           point_t{ cursor, this->tiny_center_y( bounds.top, height ) },
                           this->color( MENU_TEXT_FAINT ), this->m_version );
        }

        this->draw_search( );
        if ( this->m_header_action )
        {
            this->m_header_action->draw( );
        }
    }

    void window::draw_rail( ) const
    {
        const strip_state_t &state{ this->m_tab_state };
        const rect_t         rail{ this->rail_bounds( ) };

        const usize count{ this->m_tabs.size( ) };
        const i32   item_height{ this->scaled( g_theme.m_nextgen_style ? NEXTGEN_HEADER_HEIGHT : MENU_RAIL_ITEM_HEIGHT ) };
        const i32   items_top{ rail.top + this->scaled( g_theme.m_nextgen_style ? 0 : MENU_RAIL_TOP_PADDING ) };
        const i32   bar{ this->scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const tab   *entry{ this->m_tabs[ index ] };
            const i32    item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const rect_t item{ g_theme.m_nextgen_style
                                   ? rect_t{ rail.left + this->scaled( static_cast< i32 >( index ) * 36 ), rail.top,
                                             rail.left + this->scaled( static_cast< i32 >( index ) * 36 + 30 ), rail.bottom }
                               : index == this->m_foot_tab
                                   ? this->rail_foot_bounds( )
                                   : rect_t{ rail.left, item_top, rail.right - 1,
                                             item_top + item_height } };

            const u8 active_mix{ eased_mix( state.m_active_mix[ index ] ) };
            const u8 hover_mix{ eased_mix( state.m_hover_mix[ index ] ) };

            color_t background{ mix_color( color_t{ 0, 0, 0, 0 }, RAIL_HOVER_BACKGROUND,
                                           hover_mix ) };
            background = mix_color( background, g_theme.m_selected_background, active_mix );

            if ( background.a && !g_theme.m_nextgen_style )
            {
                g_render.box( item, this->color( background ), true );
            }

            color_t item_color{ mix_color( RAIL_IDLE_COLOR, RAIL_HOVER_COLOR, hover_mix ) };
            item_color = mix_color( item_color, g_theme.m_nextgen_style ? color_t{ 255, 255, 255 } : g_theme.m_accent, active_mix );

            draw_icon_glyph( entry->icon( ),
                             point_t{ ( item.left + ( g_theme.m_nextgen_style ? 0 : bar ) + item.right ) / 2,
                                      ( item.top + item.bottom ) / 2 },
                             this->color( item_color ) );
        }

        if ( !count || g_theme.m_nextgen_style )
        {
            return;
        }

        const float maximum_offset{ static_cast< float >(
            this->m_foot_tab < count
                ? MENU_RAIL_FOOT_OFFSET
                : static_cast< i32 >( count - 1u ) * MENU_RAIL_ITEM_HEIGHT ) };
        const float visual_offset{ math::clamp( state.m_indicator_offset, 0.0f,
                                                maximum_offset ) };
        const i32   bar_top{ items_top +
                             this->scaled( static_cast< i32 >( visual_offset + 0.5f ) ) };

        g_render.box( rect_t{ rail.left, bar_top, rail.left + bar, bar_top + item_height },
                      this->color( g_theme.m_accent ), true );
    }

    void window::draw_subnav( ) const
    {
        const tab *owner{ this->m_selected_tab < this->m_tabs.size( )
                              ? this->m_tabs[ this->m_selected_tab ]
                              : nullptr };

        if ( !owner || !owner->subtab_count( ) )
        {
            return;
        }

        const strip_state_t &state{ this->m_subnav_state };
        const rect_t         subnav{ this->subnav_bounds( ) };

        const usize count{ owner->subtab_count( ) };
        if ( g_theme.m_nextgen_style )
        {
            g_render.box( subnav, this->color( MENU_HEADER_BACKGROUND ), true );
            if ( !count ) return;

            const sub_tab *selected{ owner->subtab( this->m_selected_subnav ) };
            if ( selected )
            {
                g_render.text( MENU_FONT( ),
                               point_t{ subnav.left + this->scaled( 18 ),
                                        this->text_center_y( subnav.top, subnav.bottom - subnav.top ) },
                               this->color( MENU_TEXT_MUTED ), selected->label( ) );
            }

            const i32 cx{ subnav.right - this->scaled( 20 ) };
            const i32 cy{ ( subnav.top + subnav.bottom ) / 2 };
            const i32 arm{ this->scaled( 4 ) };
            if ( this->m_nextgen_subnav_open )
            {
                g_render.line( point_t{ cx - arm, cy + 2 }, point_t{ cx, cy - 2 }, this->color( MENU_TEXT_MUTED ) );
                g_render.line( point_t{ cx, cy - 2 }, point_t{ cx + arm, cy + 2 }, this->color( MENU_TEXT_MUTED ) );
            } else
            {
                g_render.line( point_t{ cx - arm, cy - 2 }, point_t{ cx, cy + 2 }, this->color( MENU_TEXT_MUTED ) );
                g_render.line( point_t{ cx, cy + 2 }, point_t{ cx + arm, cy - 2 }, this->color( MENU_TEXT_MUTED ) );
            }
            return;
        }

        const i32   item_height{ this->scaled( MENU_SUBNAV_ITEM_HEIGHT ) };
        const i32   items_top{ subnav.top + this->scaled( MENU_SUBNAV_TOP_PADDING ) };
        const i32   bar{ this->scaled( MENU_SELECTION_BAR ) };

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const i32 item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const u8  hover_mix{ eased_mix( state.m_hover_mix[ index ] ) };
            const u8  active_mix{ eased_mix( state.m_active_mix[ index ] ) };

            color_t background{ mix_color( color_t{ 0, 0, 0, 0 }, SUBNAV_HOVER_BACKGROUND,
                                           hover_mix ) };
            background = mix_color( background, g_theme.m_selected_background, active_mix );

            if ( !background.a )
            {
                continue;
            }

            g_render.box( rect_t{ subnav.left, item_top, subnav.right - 1,
                                  item_top + item_height },
                          this->color( background ), true );
        }

        const float maximum_offset{ static_cast< float >(
            ( count - 1u ) * static_cast< usize >( MENU_SUBNAV_ITEM_HEIGHT ) ) };
        const float visual_offset{ math::clamp( state.m_indicator_offset, 0.0f,
                                                maximum_offset ) };
        const i32   bar_top{ items_top +
                             this->scaled( static_cast< i32 >( visual_offset + 0.5f ) ) };

        g_render.box( rect_t{ subnav.left, bar_top, subnav.left + bar,
                              bar_top + item_height },
                      this->color( g_theme.m_accent ), true );

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const sub_tab *entry{ owner->subtab( index ) };

            if ( !entry )
            {
                continue;
            }

            const i32 item_top{ items_top + static_cast< i32 >( index ) * item_height };
            const u8  hover_mix{ eased_mix( state.m_hover_mix[ index ] ) };
            const u8  active_mix{ eased_mix( state.m_active_mix[ index ] ) };

            color_t label_color{ mix_color( SUBNAV_IDLE_COLOR, SUBNAV_HOVER_COLOR,
                                            hover_mix ) };
            label_color = mix_color( label_color, SUBNAV_ACTIVE_COLOR, active_mix );

            g_render.text( MENU_FONT( ),
                           point_t{ subnav.left + this->scaled( MENU_SUBNAV_INDENT ),
                                    this->text_center_y( item_top, item_height ) },
                           this->color( label_color ), entry->label( ) );
        }
    }

    void window::draw_subnav_popup( ) const
    {
        if ( !g_theme.m_nextgen_style || !this->m_nextgen_subnav_open ||
             this->m_selected_tab >= this->m_tabs.size( ) )
        {
            return;
        }

        const tab *owner{ this->m_tabs[ this->m_selected_tab ] };
        const usize count{ owner ? owner->subtab_count( ) : 0 };
        if ( !count ) return;

        const rect_t card{ this->subnav_bounds( ) };
        const i32 row_height{ this->scaled( 20 ) };
        const i32 list_top{ card.top + this->scaled( 32 ) };
        const rect_t popup{ card.left + this->scaled( 7 ), card.top + this->scaled( 7 ),
                            card.right - this->scaled( 7 ),
                            list_top + static_cast< i32 >( count ) * row_height + this->scaled( 8 ) };

        draw_drop_shadow( popup, this->scaled( 12 ), this->scaled( 4 ),
                          this->color( color_t{ 0, 0, 0, 110 } ) );
        g_render.box( popup, this->color( color_t{ 59, 63, 77, 255 } ), true );
        g_render.box( rect_t{ popup.left, popup.top, popup.right - 1, popup.bottom - 1 },
                      this->color( color_t{ 71, 76, 91, 255 } ) );

        const sub_tab *selected{ owner->subtab( this->m_selected_subnav ) };
        if ( selected )
        {
            g_render.text( MENU_FONT_BOLD( ),
                           point_t{ popup.left + this->scaled( 12 ),
                                    this->text_center_y( popup.top, this->scaled( 25 ) ) },
                           this->color( MENU_TEXT ), selected->label( ) );
        }

        for ( usize index{ }; index < count && index < STRIP_CAPACITY; ++index )
        {
            const sub_tab *entry{ owner->subtab( index ) };
            if ( !entry ) continue;
            const i32 top{ list_top + static_cast< i32 >( index ) * row_height };
            const u8 hover{ eased_mix( this->m_subnav_state.m_hover_mix[ index ] ) };
            if ( hover )
            {
                g_render.box( rect_t{ popup.left + 1, top, popup.right - 1, top + row_height },
                              this->color( with_alpha( CONTROL_BACKGROUND_HOVER, hover ) ), true );
            }
            g_render.text( index == this->m_selected_subnav ? MENU_FONT_BOLD( ) : MENU_FONT( ),
                           point_t{ popup.left + this->scaled( 12 ), this->text_center_y( top, row_height ) },
                           this->color( index == this->m_selected_subnav ? g_theme.m_accent : MENU_TEXT_DIM ),
                           entry->label( ) );
        }
    }

    rect_t window::panel_bounds( const sub_tab *page ) const
    {
        const i32 width{ this->scaled( page->panel_width( ) ) };
        const i32 gap{ this->scaled( PANEL_DOCK_GAP ) };

        if ( !g_theme.m_dock_panels && this->m_panel.placed( ) )
        {
            const point_t position{ this->m_panel.position( ) };
            return rect_t{ position.x, position.y, position.x + width, position.y + this->m_bounds.bottom - this->m_bounds.top };
        }

        i32 left{ this->m_bounds.right + gap };

        if ( left + width > this->m_screen_width )
        {
            left = this->m_bounds.left - gap - width;
        }

        return rect_t{ left, this->m_bounds.top, left + width, this->m_bounds.bottom };
    }

    rect_t window::panel_body( rect_t panel ) const
    {
        const i32 padding{ this->scaled( PANEL_DOCK_PADDING ) };

        return rect_t{ panel.left + padding,
                       panel.top + this->scaled( PANEL_HEADER_HEIGHT ) + padding,
                       panel.right - padding, panel.bottom - padding };
    }

    void window::draw_panel( const sub_tab *page ) const
    {
        const rect_t bounds{ this->panel_bounds( page ) };
        const i32    header{ this->scaled( PANEL_HEADER_HEIGHT ) };
        const i32    header_bottom{ bounds.top + header };

        g_render.blur( bounds, this->opacity( ) );

        if ( g_theme.m_nextgen_style )
        {
            const i32 radius{ this->scaled( 6 ) };
            draw_drop_shadow( bounds, this->scaled( 14 ), this->scaled( 4 ), this->color( g_theme.m_shadow ) );
            fill_rounded_box( bounds, radius, this->color( MENU_BACKGROUND ) );
            fill_rounded_horizontal_gradient( rect_t{ bounds.left, bounds.top, bounds.right, header_bottom + radius },
                                              radius, this->color( g_theme.m_accent ),
                                              this->color( g_theme.m_accent_end ) );
            g_render.box( rect_t{ bounds.left, header_bottom, bounds.right, bounds.bottom - radius },
                          this->color( MENU_BACKGROUND ), true );

            const i32 icon_left{ bounds.left + this->scaled( MENU_HEADER_PADDING ) };
            const i32 title_left{ icon_left + this->scaled( MENU_ICON_SIZE + MENU_HEADER_GAP ) };
            const i32 effect_left{ title_left + estimate_bold_text_width( page->panel_title( ) ) + this->scaled( 8 ) };
            draw_nextgen_header_effect( rect_t{ effect_left, bounds.top, bounds.right - radius, header_bottom },
                                        this->m_nextgen_header_time, this->opacity( ) );

            draw_icon_glyph( page->panel_icon( ),
                             point_t{ icon_left + this->scaled( MENU_ICON_SIZE ) / 2, bounds.top + header / 2 },
                             this->color( color_t{ 255, 255, 255 } ) );
            g_render.text( MENU_FONT_BOLD( ), point_t{ title_left, this->text_center_y( bounds.top, header ) },
                           this->color( color_t{ 255, 255, 255 } ), page->panel_title( ) );
            stroke_rounded_box( bounds, radius, this->color( MENU_OUTER_BORDER ) );
            page->panel( )->draw( );
            return;
        }

        g_render.box( bounds, this->color( MENU_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right, header_bottom },
                      this->color( MENU_HEADER_BACKGROUND ), true );
        g_render.box( rect_t{ bounds.left + 1, bounds.top + 1, bounds.right - 1,
                              bounds.top + this->scaled( 3 ) },
                      this->color( color_t{ 113, 242, 210, 34 } ), true );

        i32 cursor{ bounds.left + this->scaled( MENU_HEADER_PADDING ) };

        draw_icon_glyph( page->panel_icon( ),
                         point_t{ cursor + this->scaled( MENU_ICON_SIZE ) / 2,
                                  bounds.top + header / 2 },
                         g_theme.m_accent );

        cursor += this->scaled( MENU_ICON_SIZE + MENU_HEADER_GAP );

        g_render.text( MENU_FONT_BOLD( ),
                       point_t{ cursor, this->text_center_y( bounds.top, header ) },
                       this->color( MENU_TEXT ), page->panel_title( ) );

        g_render.line( point_t{ bounds.left + 1, header_bottom - 1 },
                       point_t{ bounds.right - 2, header_bottom - 1 },
                       this->color( MENU_SECTION_SEPARATOR ) );
        g_render.box( rect_t{ bounds.left, bounds.top, bounds.right - 1, bounds.bottom - 1 },
                      this->color( MENU_OUTER_BORDER ) );

        page->panel( )->draw( );
    }

    void window::draw_footer( ) const
    {
        const rect_t bounds{ this->m_bounds };
        const i32    height{ this->scaled( g_theme.m_nextgen_style ? NEXTGEN_FOOTER_HEIGHT : MENU_FOOTER_HEIGHT ) };
        const i32    footer_top{ bounds.bottom - height };
        const i32    text_y{ this->text_center_y( footer_top, height ) };

        if ( g_theme.m_nextgen_style )
        {
            const i32 avatar_size{ this->scaled( 16 ) };
            const i32 avatar_left{ bounds.left + this->scaled( 13 ) };
            const i32 avatar_top{ footer_top + ( height - avatar_size ) / 2 };
            // Avatar geometry uses the text command's Y coordinate as its top edge, while
            // glyphs use it as their font-line origin. Keep the two draws separate so both
            // are independently centred instead of pulling the avatar down to the baseline.
            if ( this->m_profile_avatar )
                g_render.icon( 0, render::icons::KIND_PROFILE,
                               point_t{ avatar_left, avatar_top },
                               this->color( color_t{ 255, 255, 255 } ), 0.0f, false,
                               static_cast< u32 >( avatar_size ) );
            else
                g_render.text( MENU_FONT( ), point_t{ avatar_left, avatar_top },
                               this->color( color_t{ 255, 255, 255 } ),
                               ASTRAL_XORSTR( " " ).get( ), false,
                               render::text_align::left, 0.0f, this->m_avatar_account,
                               render::AVATAR_CIRCLE, static_cast< u32 >( avatar_size ) );

            const i32 user_left{ avatar_left + avatar_size + this->scaled( 4 ) };
            g_render.text( MENU_FONT( ), point_t{ user_left, text_y },
                           this->color( MENU_FOOTER_TEXT ), this->m_user );

            const i32 user_width{ g_render.get_text_width( MENU_FONT( ), this->m_user ) };
            const i32 separator_left{ user_left + user_width + this->scaled( 6 ) };
            g_render.text( MENU_FONT( ), point_t{ separator_left, text_y },
                           this->color( MENU_FOOTER_TEXT_DIM ), ASTRAL_XORSTR( "/" ).get( ) );
            g_render.text( MENU_FONT( ), point_t{ separator_left + this->scaled( 9 ), text_y },
                           this->color( MENU_FOOTER_TEXT_DIM ), this->m_build );
            const i32 icon_right{ bounds.right - this->scaled( 10 ) };
            const i32 icon_centre_y{ footer_top + height / 2 };
            draw_icon_glyph( ICON_SEARCH,
                             point_t{ icon_right - this->scaled( 7 ), icon_centre_y },
                             this->color( this->m_nextgen_search_open ? MENU_TEXT : MENU_FOOTER_TEXT ) );
            if ( this->m_nextgen_search_open ) this->draw_search( );
            return;
        }

        g_render.text( MENU_FONT( ), point_t{ bounds.left + this->scaled( MENU_FOOTER_PADDING ), text_y },
                       this->color( MENU_FOOTER_TEXT ), this->m_build );

        const i32 dot{ this->scaled( MENU_FOOTER_DOT ) };
        const i32 dot_left{ bounds.right - this->scaled( MENU_FOOTER_PADDING_RIGHT ) - dot };
        const i32 dot_top{ footer_top + ( height - dot ) / 2 };

        g_render.box( rect_t{ dot_left, dot_top, dot_left + dot, dot_top + dot },
                      this->color( g_theme.m_accent ), true );

        if ( !this->m_user.empty( ) )
        {
            g_render.text( MENU_FONT( ),
                           point_t{ dot_left - this->scaled( MENU_HEADER_GAP ), text_y },
                           this->color( MENU_FOOTER_TEXT_DIM ), this->m_user, false,
                           render::text_align::right );
        }
    }
} // namespace gui
