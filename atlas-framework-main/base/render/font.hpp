#pragma once

#include "gdi.hpp"
#include "runtime/array.hpp"
#include "util/string.hpp"

namespace render
{
    inline constexpr usize MAX_FONT_RESOURCES{ 24 };
    inline constexpr u32   DEFAULT_FONT_PIXEL_SIZE{ 15 };

    // text quality / antialiasing mode, passed through to
    // the fdwQuality parameter of CreateFont()
    enum class text_quality : u8
    {
        default_quality,  // DEFAULT_QUALITY       - let GDI decide
        aliased,         // NONANTIALIASED_QUALITY - hard edges (old "crisp")
        antialiased,     // ANTIALIASED_QUALITY    - grayscale AA
        cleartype,       // CLEARTYPE_QUALITY      - subpixel AA, best for LCDs
    };

    struct font_resource_t
    {
        const char        *m_name{ };
        char               m_face[ 64 ]{ };
        const u8          *m_data{ };
        u32                m_size{ };
        u32                m_pixel_size{ DEFAULT_FONT_PIXEL_SIZE };
        i32                m_weight{ FW_NORMAL };
        text_quality       m_quality{ text_quality::default_quality };
        void              *m_handle{ };
        u32                m_font_count{ };
    };

    class font_manager
    {
    public:
        bool init( gdi::api *api );
        void shutdown( );

        bool register_memory( const char *name, const char *face, const u8 *data, u32 size,
                             u32           pixel_size = DEFAULT_FONT_PIXEL_SIZE, i32 weight = FW_NORMAL,
                             text_quality  quality    = text_quality::default_quality );
        bool register_system( const char *name, const char *face,
                              u32          pixel_size = DEFAULT_FONT_PIXEL_SIZE,
                              i32          weight     = FW_NORMAL,
                              text_quality quality    = text_quality::default_quality );
        const font_resource_t *font( const char *name ) const;
        const char            *face( const char *name ) const;
        bool                   set_system_face( const char *name, const char *face );

        // runtime quality change - caller must recreate any cached HFONT afterwards,
        // since quality is baked in at CreateFont() time
        void set_quality( const char *name, text_quality quality );

        bool ready( ) const
        {
            return this->m_api && this->m_api->FontReady( );
        }

        bool system_ready( ) const
        {
            return this->m_api && this->m_api->Ready( );
        }

    private:
        font_resource_t       *find( const char *name );
        const font_resource_t *find( const char *name ) const;

        gdi::api                                         *m_api{ };
        atl::array< font_resource_t, MAX_FONT_RESOURCES > m_resources{ };
        usize                                             m_resource_count{ };
    };
} // namespace render
