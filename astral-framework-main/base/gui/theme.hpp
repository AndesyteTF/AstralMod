#pragma once

#include "runtime/obfuscation.hpp"
#include "util/typedefs.hpp"

// I think this is better for organization, after all we don't need to go after calls through the code
// We can just find what we want here and cross references and change the values easier.

inline constexpr usize MENU_LABEL_LENGTH{ 64 };
inline constexpr usize MENU_TOOLTIP_LENGTH{ 256 }; // Setting this to 512 crashes the game. IDK why.

// icons
inline constexpr const char *ICON_USER{ "!" };
inline constexpr const char *ICON_CROSSHAIR{ "\"" };
inline constexpr const char *ICON_EYE{ "#" };
inline constexpr const char *ICON_WRENCH{ "$" };
inline constexpr const char *ICON_SETTINGS{ "%" };
inline constexpr const char *ICON_CODE{ "&" };
inline constexpr const char *ICON_SAVE{ "*" };
inline constexpr const char *ICON_CHEVRON_DOWN{ "+" };
inline constexpr const char *ICON_SEARCH{ "," };
inline constexpr const char *ICON_CHEVRON_UP{ "-" };
inline constexpr const char *ICON_CROSS{ "." };
inline constexpr const char *ICON_SPARKLES{ "/" };
inline constexpr const char *ICON_ZAP{ "0" };
inline constexpr const char *ICON_DOLLAR{ "1" };
inline constexpr const char *ICON_BAN{ "2" };
inline constexpr const char *ICON_FLAME{ "3" };
inline constexpr const char *ICON_PLAYERS{ "4" };
inline constexpr const char *ICON_LOADER{ "5" };
inline constexpr const char *ICON_TRIANGLE_ALERT{ "6" };
inline constexpr const char *ICON_RULER{ "7" };
inline constexpr const char *ICON_RADIO{ "8" };
inline constexpr const char *ICON_MUSIC{ "9" };

inline const char *MENU_FONT( )
{
    return ASTRAL_XORSTR_STATIC( "Astral Compact" );
}
inline const char *MENU_FONT_BOLD( )
{
    return ASTRAL_XORSTR_STATIC( "Astral Compact Bd" );
}
inline const char *MENU_FONT_TINY( )
{
    return ASTRAL_XORSTR_STATIC( "Astral Compact Sm" );
}
inline const char *MENU_FONT_MONO( )
{
    return ASTRAL_XORSTR_STATIC( "Astral Mono" );
}

inline constexpr i32 MENU_ICON_SIZE{ 13 };
inline constexpr i32 MENU_ICON_SIZE_LARGE{ 16 };

inline constexpr i32 TINY_FONT_INK_TOP{ 1 };
inline constexpr i32 TINY_FONT_INK_HEIGHT{ 7 };

inline constexpr i32 LABEL_FONT_INK_TOP{ 1 };
inline constexpr i32 LABEL_FONT_INK_HEIGHT{ 8 };

inline constexpr i32 MENU_BASE_WIDTH{ 560 };
inline constexpr i32 MENU_BASE_HEIGHT{ 400 };
inline constexpr i32 NEXTGEN_BASE_WIDTH{ 600 };
inline constexpr i32 NEXTGEN_BASE_HEIGHT{ 620 };
inline constexpr i32 NEXTGEN_HEADER_HEIGHT{ 44 };
inline constexpr i32 NEXTGEN_FOOTER_HEIGHT{ 26 };
inline constexpr i32 NEXTGEN_PAGE_PADDING{ 14 };
inline constexpr i32 NEXTGEN_CARD_HEIGHT{ 42 };
inline constexpr i32 NEXTGEN_GAP{ 12 };
inline constexpr i32 NEXTGEN_COLUMN_WIDTH{ 280 };
inline constexpr i32 MENU_SCREEN_MARGIN{ 48 };
inline constexpr i32 MENU_HEADER_HEIGHT{ 23 };
inline constexpr i32 MENU_HEADER_PADDING{ 7 };
inline constexpr i32 MENU_HEADER_GAP{ 6 };
inline constexpr i32 MENU_LOGO_SIZE{ 20 };
inline constexpr i32 MENU_DECORATION_SIZE{ 16 };

inline constexpr i32 MENU_RAIL_WIDTH{ 32 };
inline constexpr i32 MENU_RAIL_ITEM_HEIGHT{ 31 };
inline constexpr i32 MENU_RAIL_TOP_PADDING{ 3 };
inline constexpr i32 MENU_RAIL_FOOT_HEIGHT{ 31 };
inline constexpr i32 MENU_SELECTION_BAR{ 2 };

inline constexpr i32 MENU_SEARCH_WIDTH{ 96 };
inline constexpr i32 MENU_SEARCH_HEIGHT{ 15 };
inline constexpr i32 MENU_SEARCH_MARGIN{ 8 };

inline constexpr i32 MENU_SUBNAV_WIDTH{ 92 };
inline constexpr i32 MENU_SUBNAV_ITEM_HEIGHT{ 20 };
inline constexpr i32 MENU_SUBNAV_TOP_PADDING{ 3 };
inline constexpr i32 MENU_SUBNAV_INDENT{ 10 };

inline constexpr i32 MENU_FOOTER_HEIGHT{ MENU_HEADER_HEIGHT };

inline constexpr i32 MENU_RAIL_FOOT_OFFSET{ MENU_BASE_HEIGHT - MENU_HEADER_HEIGHT - MENU_FOOTER_HEIGHT -
                                            MENU_RAIL_TOP_PADDING - MENU_RAIL_FOOT_HEIGHT };
inline constexpr i32 MENU_FOOTER_PADDING{ MENU_HEADER_PADDING };
inline constexpr i32 MENU_FOOTER_PADDING_RIGHT{ MENU_SEARCH_MARGIN };
inline constexpr i32 MENU_FOOTER_DOT{ 5 };

inline constexpr i32 MENU_CONTENT_PADDING{ 7 };
inline constexpr i32 MENU_CONTENT_PADDING_RIGHT{ 6 };
inline constexpr i32 MENU_COLUMN_GAP{ 7 };
inline constexpr i32 MENU_COLUMN_WIDTH{ 208 };

inline constexpr i32 PANEL_DOCK_GAP{ 6 };
inline constexpr i32 PANEL_HEADER_HEIGHT{ MENU_HEADER_HEIGHT };
inline constexpr i32 PANEL_DOCK_PADDING{ MENU_CONTENT_PADDING };

inline constexpr i32 CARD_TITLE_HEIGHT{ 18 }; // 1px frame + 17px bar
inline constexpr i32 CARD_PADDING_X{ 7 };
inline constexpr i32 CARD_PADDING_TOP{ 4 };    // 1px rule + 3px
inline constexpr i32 CARD_PADDING_BOTTOM{ 6 }; // 5px + 1px frame
inline constexpr i32 CARD_BLOCK_GAP{ 0 };      // rows sit flush, blocks carry their own padding

inline constexpr i32 WIDGET_ROW_HEIGHT{ 17 };
inline constexpr i32 WIDGET_ROW_BLEED{ 4 }; // hover strip runs past the card padding
inline constexpr i32 WIDGET_CHECKBOX_SIZE{ 9 };
inline constexpr i32 WIDGET_CHECKBOX_GAP{ 6 };
inline constexpr i32 WIDGET_LABEL_HEIGHT{ 14 };
inline constexpr i32 WIDGET_LABEL_GAP{ 0 };
inline constexpr i32 WIDGET_LABEL_INDENT{ 15 }; // checkbox + gap
inline constexpr i32 WIDGET_BLOCK_PADDING_TOP{ 1 };
inline constexpr i32 WIDGET_BLOCK_PADDING_BOTTOM{ 4 };
inline constexpr i32 WIDGET_WELL_HEIGHT{ 18 }; // 16px well + 1px frame either side
inline constexpr i32 WIDGET_WELL_PADDING{ 5 };
inline constexpr i32 WIDGET_KEYBIND_HEIGHT{ 17 };
inline constexpr i32 WIDGET_CHIP_HEIGHT{ 15 };
inline constexpr i32 WIDGET_CHIP_PADDING{ 4 };
inline constexpr i32 WIDGET_KEYBIND_MIN_WIDTH{ 30 };
inline constexpr i32 WIDGET_BIND_POPUP_WIDTH{ 60 };
inline constexpr i32 WIDGET_BIND_MODE_COUNT{ 3 };
inline constexpr i32 WIDGET_SLIDER_HEIGHT{ 5 }; // 3px track + 1px frame either side
inline constexpr i32 WIDGET_SEGMENT_HEIGHT{ 15 };
inline constexpr i32 WIDGET_SEGMENT_GAP{ 4 };
inline constexpr i32 WIDGET_MARK_SIZE{ 3 };
inline constexpr i32 WIDGET_LIST_ROW_HEIGHT{ 15 };
inline constexpr i32 WIDGET_LIST_PADDING{ 5 };
inline constexpr i32 WIDGET_LIST_MARGIN_TOP{ 2 };
inline constexpr i32 WIDGET_LIST_MARGIN_BOTTOM{ 4 };
inline constexpr i32 WIDGET_BUTTON_HEIGHT{ 17 };
inline constexpr i32 WIDGET_BUTTON_WIDTH{ 40 };
inline constexpr i32 WIDGET_BUTTON_GAP{ 4 };
inline constexpr i32 COMBO_POPUP_GAP{ 0 }; // the list hangs straight off the well
// hard ceiling, not a window: the popup has no scroll offset, so anything past this row is drawn
// nowhere and hit-tested nowhere. it has to stay >= the longest list any combo box is built with
inline constexpr i32 COMBO_POPUP_MAX_ROWS{ 16 };

inline constexpr i32 PICKER_SWATCH_WIDTH{ 20 };
inline constexpr i32 PICKER_SWATCH_HEIGHT{ 9 };
inline constexpr i32 PICKER_POPUP_GAP{ 3 }; // swatch bottom -> popup top
inline constexpr i32 PICKER_POPUP_PADDING{ 6 };
inline constexpr i32 PICKER_FIELD_SIZE{ 88 };
inline constexpr i32 PICKER_BAR_WIDTH{ 8 };
inline constexpr i32 PICKER_BAR_GAP{ 6 };
inline constexpr i32 PICKER_ROW_GAP{ 6 }; // field bottom -> footer top
inline constexpr i32 PICKER_FOOTER_HEIGHT{ 16 };
inline constexpr i32 PICKER_PREVIEW_WIDTH{ 20 };
inline constexpr i32 PICKER_HEX_PADDING{ 5 };
inline constexpr i32 PICKER_CHECKER_SIZE{ 3 };
inline constexpr i32 PICKER_CURSOR_RADIUS{ 4 };
inline constexpr i32 PICKER_KNOB_HEIGHT{ 4 };
inline constexpr i32 PICKER_KNOB_BLEED{ 2 };
inline constexpr i32 PICKER_POPUP_RISE{ 5 };

namespace gui
{
    enum menu_style_t : usize
    {
        MENU_STYLE_CLASSIC = 0,
        MENU_STYLE_NEXTGEN,
        MENU_STYLE_COUNT,
    };

    struct theme_t
    {
        color_t m_accent{ 46, 214, 188 };
        color_t m_accent_alt{ 23, 157, 184 };
        color_t m_accent_end{ 255, 178, 124 };
        color_t m_checkbox_top{ 46, 214, 188 };
        color_t m_checkbox_top_hover{ 105, 239, 211 };
        color_t m_slider_drag{ 105, 239, 211 };
        color_t m_input_focus_border{ 31, 151, 170 };
        color_t m_selected_background{ 11, 49, 62 };
        color_t m_row_selected_background{ 13, 54, 66 };

        color_t m_shadow{ 0, 0, 0, 72 };
        bool    m_blur_background{ true };
        bool    m_astral_effect{ false };
        bool    m_nextgen_style{ false };
        bool    m_snap_indicators{ false };
        bool    m_dock_panels{ true };
        usize   m_particles{ 2 };
        bool    m_sounds{ true };
        float   m_sound_volume{ 1.0f };

        void set_accent( color_t accent );
    };

} // namespace gui

extern gui::theme_t g_theme;

// color pallete
inline color_t MENU_BACKGROUND{ 7, 20, 30, 244 };
inline color_t MENU_HEADER_BACKGROUND{ 8, 31, 43, 248 };
inline color_t MENU_RAIL_BACKGROUND{ 5, 18, 28, 250 };
inline color_t MENU_SUBNAV_BACKGROUND{ 7, 25, 36, 248 };
inline color_t MENU_OUTER_BORDER{ 28, 105, 121 };
inline color_t MENU_INNER_BORDER{ 13, 56, 70 };
inline color_t MENU_SECTION_SEPARATOR{ 20, 76, 91 };
inline color_t MENU_SUBNAV_SEPARATOR{ 15, 62, 76 };
inline color_t MENU_FOOTER_TEXT{ 92, 151, 160 };
inline color_t MENU_FOOTER_TEXT_DIM{ 71, 127, 139 };
inline color_t MENU_TEXT{ 226, 250, 247 };
inline color_t MENU_TEXT_MUTED{ 145, 200, 199 };
inline color_t MENU_TEXT_DIM{ 91, 150, 158 };
inline color_t MENU_TEXT_FAINT{ 61, 116, 130 };
inline color_t MENU_LOGO_GLYPH{ 113, 242, 210 };
inline color_t MENU_WORDMARK{ 225, 255, 249 };

inline color_t RAIL_IDLE_COLOR{ 80, 143, 153 };
inline color_t RAIL_HOVER_COLOR{ 205, 250, 241 };
inline color_t RAIL_HOVER_BACKGROUND{ 15, 83, 96, 145 };
inline color_t RAIL_ACTIVE_BACKGROUND{ 10, 55, 68 };
inline color_t RAIL_FOOT_COLOR{ 53, 113, 127 };

inline color_t SUBNAV_IDLE_COLOR{ 103, 165, 170 };
inline color_t SUBNAV_HOVER_COLOR{ 207, 247, 239 };
inline color_t SUBNAV_ACTIVE_COLOR{ 231, 255, 249 };
inline color_t SUBNAV_HOVER_BACKGROUND{ 14, 74, 87, 145 };
inline color_t SUBNAV_ACTIVE_BACKGROUND{ 10, 53, 66 };

inline color_t CARD_BACKGROUND{ 9, 30, 41, 238 };
inline color_t CARD_TITLE_BACKGROUND{ 12, 48, 59, 244 };
inline color_t CARD_BORDER{ 24, 91, 105 };
inline color_t CARD_RULE{ 26, 101, 112 };
inline color_t CARD_TITLE_TEXT{ 212, 249, 241 };

inline color_t CONTROL_BACKGROUND{ 5, 24, 34 };
inline color_t CONTROL_BACKGROUND_HOVER{ 10, 44, 55 };
inline color_t CONTROL_BORDER{ 23, 79, 92 };
inline color_t CONTROL_BORDER_HOVER{ 42, 137, 145 };
inline color_t CONTROL_LABEL{ 135, 191, 191 };
inline color_t CONTROL_VALUE{ 213, 246, 239 };
inline color_t STATUS_WARNING{ 226, 170, 74 };

inline color_t CHECKBOX_BOX_IDLE{ 6, 29, 39 };
inline color_t CHECKBOX_BORDER_IDLE{ 29, 91, 101 };
inline color_t CHECKBOX_TEXT_IDLE{ 133, 188, 188 };
inline color_t CHECKBOX_TEXT_HOVER{ 201, 240, 233 };
inline color_t CHECKBOX_TEXT_ACTIVE{ 229, 255, 248 };
inline color_t CHECKBOX_MARK_COLOR{ 232, 255, 249 };
inline color_t CHECKBOX_KEY_COLOR{ 84, 151, 158 };

inline color_t SWATCH_BORDER{ 35, 111, 121 };

inline color_t SLIDER_BORDER{ 22, 75, 87 };
inline color_t SLIDER_VALUE_COLOR{ 221, 250, 243 };
inline color_t SLIDER_NOTCH_COLOR{ 232, 255, 249 };

inline color_t ROW_HOVER_BACKGROUND{ 15, 67, 77 };

inline color_t LIST_ROW_BACKGROUND{ 10, 39, 50 };
inline color_t LIST_META_COLOR{ 91, 151, 157 };
inline color_t LIST_TEXT_IDLE{ 139, 194, 193 };
inline color_t LIST_TEXT_ACTIVE{ 231, 255, 249 };

inline color_t POPUP_BACKGROUND{ 5, 22, 31 };
inline color_t POPUP_ROW_HOVER{ 12, 55, 67 };
inline color_t POPUP_ROW_SELECTED{ 12, 62, 72 };

inline color_t BUTTON_BACKGROUND{ 10, 50, 62 };
inline color_t BUTTON_BACKGROUND_HOVER{ 16, 76, 85 };
inline color_t BUTTON_BACKGROUND_PRESS{ 7, 37, 47 };
inline color_t BUTTON_BORDER{ 30, 104, 116 };
inline color_t BUTTON_TEXT{ 190, 231, 225 };
inline color_t BUTTON_TEXT_HOVER{ 235, 255, 249 };
inline color_t BUTTON_TEXT_PRESS{ 232, 255, 249 };

inline color_t PANEL_BACKGROUND{ 7, 24, 34, 232 };
inline color_t PANEL_HEADER_BACKGROUND{ 8, 38, 49, 244 };
inline color_t PANEL_INNER_BORDER{ 17, 67, 80 };

inline color_t CHIP_IDLE_BACKGROUND{ 5, 27, 37 };
inline color_t CHIP_HOVER_BACKGROUND{ 12, 57, 68 };
inline color_t CHIP_IDLE_TEXT{ 125, 181, 182 };
inline color_t CHIP_ACTIVE_TEXT{ 207, 244, 237 };

inline color_t SEARCH_BACKGROUND{ 4, 23, 32 };
inline color_t SEARCH_BORDER{ 25, 91, 103 };

inline constexpr i32     MENU_SHADOW_SPREAD{ 26 };
inline constexpr i32     MENU_SHADOW_DROP{ 6 };
inline color_t MENU_SHADOW{ 0, 8, 14, 104 };

inline color_t PICKER_POPUP_BACKGROUND{ 5, 22, 31, 252 };
inline color_t PICKER_HEX_BACKGROUND{ 5, 28, 38 };
inline color_t PICKER_CHECKER_LIGHT{ 52, 92, 95 };
inline color_t PICKER_CHECKER_DARK{ 24, 53, 60 };
inline color_t PICKER_CURSOR_COLOR{ 230, 255, 249 };
inline color_t PICKER_CURSOR_SHADOW{ 2, 15, 22, 220 };

inline constexpr i32     SCROLLBAR_WIDTH{ 2 };
inline constexpr i32     SCROLLBAR_INSET{ 1 };
inline color_t SCROLLBAR_TRACK{ 5, 25, 34 };
inline color_t SCROLLBAR_THUMB{ 48, 139, 145 };
inline constexpr i32     SCROLL_WHEEL_STEP{ 17 };
inline constexpr u32     SCROLL_GLIDE_DURATION{ 110 };
inline constexpr i32     SCROLL_SHADOW_DEPTH{ 16 };
inline constexpr i32     SCROLL_SHADOW_DROP{ 4 }; // clears the title rule before it ramps in
inline color_t SCROLL_SHADOW{ 2, 13, 21, 205 };

inline constexpr u32     TOOLTIP_FADE_IN_DURATION{ 150 };
inline constexpr u32     TOOLTIP_FADE_OUT_DURATION{ 110 };
inline constexpr i32     TOOLTIP_PADDING_X{ 5 };
inline constexpr i32     TOOLTIP_HEIGHT{ 17 };
inline constexpr i32     TOOLTIP_LINE_ADVANCE{ 14 };
inline constexpr i32     TOOLTIP_OFFSET_X{ 11 };
inline constexpr i32     TOOLTIP_OFFSET_Y{ 14 };
inline color_t TOOLTIP_BACKGROUND{ 5, 25, 35, 250 };
inline color_t TOOLTIP_BORDER{ 31, 107, 116 };
inline color_t TOOLTIP_TEXT{ 203, 240, 234 };

inline color_t INPUT_BACKGROUND_FOCUS{ 7, 38, 48 };
inline color_t INPUT_TEXT{ 226, 251, 245 };
inline constexpr i32     INPUT_RAIL_HEIGHT{ 1 };
inline constexpr i32     INPUT_PADDING_X{ 5 };
inline constexpr i32     INPUT_ICON_GAP{ 4 };
inline constexpr usize   MENU_TEXT_LENGTH{ 48 };

inline constexpr i32     EDITOR_LINE_HEIGHT{ 14 };
inline constexpr i32     EDITOR_PADDING{ 6 };
inline constexpr i32     EDITOR_TAB_WIDTH{ 4 };
inline constexpr i32     EDITOR_WHEEL_LINES{ 3 };
inline constexpr u32     EDITOR_DOUBLE_CLICK{ 400 };
inline color_t EDITOR_BACKGROUND{ 4, 20, 29 };
inline color_t EDITOR_BORDER{ 23, 81, 93 };
inline color_t EDITOR_GUTTER{ 6, 29, 39 };
inline color_t EDITOR_GUTTER_TEXT{ 69, 127, 138 };
inline color_t EDITOR_CURRENT_LINE{ 9, 42, 52 };
inline color_t EDITOR_SELECTION{ 22, 83, 102 };
inline color_t EDITOR_ERROR_LINE{ 92, 34, 40 };
inline color_t EDITOR_CARET{ 113, 242, 210 };
inline color_t EDITOR_TEXT{ 205, 235, 231 };
inline color_t EDITOR_KEYWORD{ 88, 220, 206 };
inline color_t EDITOR_LIBRARY{ 86, 181, 241 };
inline color_t EDITOR_STRING{ 161, 221, 123 };
inline color_t EDITOR_NUMBER{ 242, 190, 98 };
inline color_t EDITOR_COMMENT{ 87, 139, 144 };
inline color_t CONSOLE_INFO{ 139, 194, 193 };
inline color_t CONSOLE_ERROR{ 244, 112, 106 };

// animation timers
inline constexpr u32 WIDGET_REVEAL_DURATION{ 220 };
inline constexpr u8  WIDGET_REVEAL_SPLIT{ 150 };
inline constexpr u32 WIDGET_TOGGLE_DURATION{ 140 };
inline constexpr u32 WIDGET_HOVER_DURATION{ 100 };
inline constexpr u32 COMBO_EXPAND_DURATION{ 170 };
inline constexpr u32 COMBO_COLLAPSE_DURATION{ 130 };
inline constexpr u32 PICKER_EXPAND_DURATION{ 160 };
inline constexpr u32 PICKER_COLLAPSE_DURATION{ 110 };

inline constexpr u32   INPUT_FOCUS_DURATION{ 190 };
inline constexpr u32   INPUT_PLACEHOLDER_DURATION{ 130 };
inline constexpr u32   INPUT_CARET_PERIOD{ 1150 };
inline constexpr u32   INPUT_CARET_SOLID{ 420 };
inline constexpr float INPUT_CARET_GLIDE{ 62.0f };

inline constexpr u32 CONTENT_ENTER_DURATION{ 220 };
inline constexpr i32 CONTENT_ENTER_RISE{ 8 };
inline constexpr i32 CONTENT_CARD_STAGGER{ 90 };
inline constexpr i32 CONTENT_CARD_FADE{ 130 };
inline constexpr u32 STRIP_ACTIVE_DURATION{ 200 };
inline constexpr u32 STRIP_HOVER_DURATION{ 130 };
inline constexpr u32 ANIMATION_MAX_DELTA{ 50 };
inline constexpr u32 BACKGROUND_FADE_DURATION{ 260 };
