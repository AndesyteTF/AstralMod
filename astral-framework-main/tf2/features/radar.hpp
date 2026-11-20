#pragma once

#include "gui/indicator.hpp"
#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"

class radar
{
public:
    void think( );
    void paint( );
    void on_level_shutdown( );

private:
    struct segment_t
    {
        vec3 m_from{ };
        vec3 m_to{ };
    };

    struct marker_t
    {
        vec3       m_origin{ };
        ETFClassID m_class_id{ static_cast< ETFClassID >( -1 ) };
        i32        m_index{ };
        i32        m_team{ };
        i32        m_player_class{ };
        bool       m_dormant{ };
        bool       m_alive{ true };
        char       m_name[ 64 ]{ };
    };

    struct floor_t
    {
        vec3 m_points[ 8 ]{ };
        u8   m_count{ };
        f32  m_height{ };
    };

    void rebuild_map( const vec3 &origin );
    bool load_class_icons( );
    bool project( const vec3 &world, const vec3 &origin, const rect_t &content,
                  float yaw, bool clamp, point_t &out ) const;
    u8 layer_opacity( f32 height ) const;
    void draw_entity( const marker_t &marker, const vec3 &origin,
                      const rect_t &content, float yaw, u8 alpha );

    indicator_card_t        m_card{ };
    atl::mutex              m_publish{ };
    atl::vector< segment_t > m_segments{ };
    atl::vector< floor_t >   m_floors{ };
    atl::vector< marker_t >  m_markers{ };
    vec3                    m_cache_origin{ };
    i32                     m_cache_range{ };
    i32                     m_cache_detail{ };
    usize                   m_cache_mode{ };
    f32                     m_display_height{ };
    bool                    m_display_height_valid{ };
    i32                     m_local_class{ };
    atl::vector< u8 >       m_class_icons{ };
    bool                    m_class_icons_loaded{ };
    bool                    m_class_icons_attempted{ };
    bool                    m_class_icons_custom{ };
    bool                    m_class_icons_need_upload{ };
    bool                    m_cache_valid{ };
};

extern radar g_radar;
