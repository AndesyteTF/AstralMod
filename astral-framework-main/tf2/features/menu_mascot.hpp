#pragma once

#include "link.hpp"
#include "runtime/array.hpp"
#include "runtime/vector.hpp"

inline constexpr usize MASCOT_CAPACITY{ 64 };
inline constexpr usize MASCOT_NAME_LENGTH{ 63 };
inline constexpr usize MASCOT_FILE_LENGTH{ 260 };

class menu_mascot
{
public:
    void refresh( );
    void paint( );
    void close( );
    bool allow_menu_close( );

    const char *const *labels( ) const { return this->m_labels.data( ); }
    usize count( ) const { return this->m_count; }

private:
    struct image_t
    {
        wchar_t                                m_file[ MASCOT_FILE_LENGTH ]{ };
        fixed_string< MASCOT_NAME_LENGTH + 1 > m_name{ };
        const char                            *m_mime{ };
    };

    bool load( usize index );

    atl::array< image_t, MASCOT_CAPACITY >      m_images{ };
    atl::array< const char *, MASCOT_CAPACITY > m_labels{ };
    usize                                       m_count{ };
    usize                                       m_loaded{ MASCOT_CAPACITY };
    usize                                       m_failed{ MASCOT_CAPACITY };
    usize                                       m_pending{ MASCOT_CAPACITY };
    double                                      m_switch_time{ };
    usize                                       m_upload_index{ MASCOT_CAPACITY };
    usize                                       m_upload_cursor{ };
    u8                                          m_upload_phase{ };
    const char                                 *m_upload_mime{ };
    atl::vector< char >                         m_upload_data{ };
    bool                                        m_panel_ready{ };
    bool                                        m_image_ready{ };
    i32                                         m_image_width{ };
    i32                                         m_image_height{ };
    bool                                        m_closing{ };
    u64                                         m_close_deadline{ };
    float                                       m_reveal{ };
    double                                      m_animation_time{ };
    html_handle_t                               m_panel{ };
};

extern menu_mascot g_menu_mascot;
