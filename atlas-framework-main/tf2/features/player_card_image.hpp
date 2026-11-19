#pragma once

#include "link.hpp"
#include "runtime/array.hpp"
#include "runtime/vector.hpp"

inline constexpr usize PLAYER_CARD_IMAGE_CAPACITY{ 64 };

class player_card_image
{
public:
    void refresh( );
    void paint( rect_t bounds, u8 opacity, usize selected );

    const char *const *labels( ) const { return this->m_labels.data( ); }
    usize count( ) const { return this->m_count; }
    bool ready( usize selected ) const { return this->m_image_ready && this->m_loaded == selected; }

private:
    struct image_t
    {
        wchar_t         m_file[ 260 ]{ };
        fixed_string< 64 > m_name{ };
        const char     *m_mime{ };
    };

    bool load( usize index );

    atl::array< image_t, PLAYER_CARD_IMAGE_CAPACITY >      m_images{ };
    atl::array< const char *, PLAYER_CARD_IMAGE_CAPACITY > m_labels{ };
    usize m_count{ }, m_loaded{ PLAYER_CARD_IMAGE_CAPACITY };
    usize m_failed{ PLAYER_CARD_IMAGE_CAPACITY }, m_upload_index{ PLAYER_CARD_IMAGE_CAPACITY };
    usize m_upload_cursor{ };
    u8 m_upload_phase{ };
    const char *m_upload_mime{ };
    atl::vector< char > m_upload_data{ };
    bool m_panel_ready{ }, m_image_ready{ };
    html_handle_t m_panel{ };
};

extern player_card_image g_player_card_image;
