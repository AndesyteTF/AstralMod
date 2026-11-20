#include "base.hpp"

#include "gui/gui.hpp"
#include "gui/indicator.hpp"
#include "math/math.hpp"
#include "render/icons.hpp"
#include "render/render.hpp"

#include "entity_cache.hpp"
#include "player_data.hpp"
#include "radar.hpp"

ASTRAL_NO_DESTROY radar g_radar{ };

namespace
{
    constexpr usize RADAR_SEGMENT_LIMIT{ 4096 };
    constexpr usize RADAR_FLOOR_LIMIT{ 1536 };
    constexpr usize RADAR_BRUSH_LIMIT{ 1536 };
    constexpr f32   RADAR_CACHE_MOVE{ 512.0f };
    constexpr u32   CLASS_ICON_SIZE{ 64 };
    constexpr u32   CLASS_ICON_COUNT{ 10 };

    enum vtf_format : i32
    {
        VTF_RGBA8888 = 0,
        VTF_BGRA8888 = 12,
        VTF_DXT1 = 13,
        VTF_DXT5 = 15,
        VTF_BGRX8888 = 16,
        VTF_DXT1_ALPHA = 20,
    };

    ASTRAL_FORCE_INLINE f32 absf( f32 value )
    {
        return value < 0.0f ? -value : value;
    }

    ASTRAL_FORCE_INLINE bool radar_projectile( ETFClassID id )
    {
        switch ( id )
        {
            case ETFClassID::CTFGrenadePipebombProjectile:
            case ETFClassID::CTFStunBall:
            case ETFClassID::CTFProjectile_Cleaver:
            case ETFClassID::CTFProjectile_Rocket:
            case ETFClassID::CTFProjectile_SentryRocket:
            case ETFClassID::CTFProjectile_BallOfFire:
            case ETFClassID::CTFProjectile_EnergyBall:
            case ETFClassID::CTFProjectile_Flare:
            case ETFClassID::CTFProjectile_Arrow:
            case ETFClassID::CTFProjectile_HealingBolt:
            case ETFClassID::CTFProjectile_SpellFireball:
            case ETFClassID::CTFProjectile_Jar:
            case ETFClassID::CTFProjectile_JarMilk:
            case ETFClassID::CTFProjectile_JarGas:
            case ETFClassID::CTFProjectile_MechanicalArmOrb: return true;
            default: return false;
        }
    }

    ASTRAL_FORCE_INLINE bool radar_building( ETFClassID id )
    {
        return id == ETFClassID::CObjectSentrygun || id == ETFClassID::CObjectDispenser ||
               id == ETFClassID::CObjectTeleporter;
    }

    ASTRAL_FORCE_INLINE bool radar_npc( ETFClassID id )
    {
        return id == ETFClassID::CEyeballBoss || id == ETFClassID::CHeadlessHatman ||
               id == ETFClassID::CMerasmus || id == ETFClassID::CZombie ||
               id == ETFClassID::CTFTankBoss;
    }

    void radar_filled_circle( point_t centre, color_t color, f32 radius )
    {
        constexpr usize POINTS{ 24 };
        constexpr f32 PI{ 3.14159265358979323846f };
        vec2 vertices[ POINTS ]{ };
        color_t colors[ POINTS ]{ };
        for ( usize index{ }; index < POINTS; ++index )
        {
            const f32 angle{ 2.0f * PI * static_cast< f32 >( index ) / static_cast< f32 >( POINTS ) };
            vertices[ index ] = vec2{ static_cast< f32 >( centre.x ) + math::cos( angle ) * radius,
                                      static_cast< f32 >( centre.y ) + math::sin( angle ) * radius };
            colors[ index ] = color;
        }
        g_render.polygon( vertices, colors, POINTS );
    }

    ASTRAL_FORCE_INLINE u16 read_u16( const u8 *data )
    {
        return static_cast< u16 >( data[ 0 ] | ( static_cast< u16 >( data[ 1 ] ) << 8 ) );
    }

    ASTRAL_FORCE_INLINE u32 read_u32( const u8 *data )
    {
        return static_cast< u32 >( data[ 0 ] ) | ( static_cast< u32 >( data[ 1 ] ) << 8 ) |
               ( static_cast< u32 >( data[ 2 ] ) << 16 ) | ( static_cast< u32 >( data[ 3 ] ) << 24 );
    }

    i32 vtf_bytes( i32 format, i32 width, i32 height )
    {
        width = math::bigger( width, 1 );
        height = math::bigger( height, 1 );
        if ( format == VTF_RGBA8888 || format == VTF_BGRA8888 || format == VTF_BGRX8888 )
            return width * height * 4;
        if ( format == VTF_DXT1 || format == VTF_DXT1_ALPHA )
            return ( ( width + 3 ) / 4 ) * ( ( height + 3 ) / 4 ) * 8;
        if ( format == VTF_DXT5 )
            return ( ( width + 3 ) / 4 ) * ( ( height + 3 ) / 4 ) * 16;
        return 0;
    }

    void rgb565( u16 value, u8 &red, u8 &green, u8 &blue )
    {
        red = static_cast< u8 >( ( ( value >> 11 ) & 31 ) * 255 / 31 );
        green = static_cast< u8 >( ( ( value >> 5 ) & 63 ) * 255 / 63 );
        blue = static_cast< u8 >( ( value & 31 ) * 255 / 31 );
    }

    void decode_dxt_color( const u8 *block, u8 pixels[ 16 ][ 4 ], bool four_colors )
    {
        const u16 first{ read_u16( block ) }, second{ read_u16( block + 2 ) };
        const u32 indices{ read_u32( block + 4 ) };
        u8 colors[ 4 ][ 4 ]{ };
        rgb565( first, colors[ 0 ][ 0 ], colors[ 0 ][ 1 ], colors[ 0 ][ 2 ] );
        rgb565( second, colors[ 1 ][ 0 ], colors[ 1 ][ 1 ], colors[ 1 ][ 2 ] );
        colors[ 0 ][ 3 ] = colors[ 1 ][ 3 ] = 255;
        if ( four_colors || first > second )
        {
            for ( i32 channel{ }; channel < 3; ++channel )
            {
                colors[ 2 ][ channel ] = static_cast< u8 >( ( colors[ 0 ][ channel ] * 2 + colors[ 1 ][ channel ] ) / 3 );
                colors[ 3 ][ channel ] = static_cast< u8 >( ( colors[ 0 ][ channel ] + colors[ 1 ][ channel ] * 2 ) / 3 );
            }
            colors[ 2 ][ 3 ] = colors[ 3 ][ 3 ] = 255;
        } else
        {
            for ( i32 channel{ }; channel < 3; ++channel )
                colors[ 2 ][ channel ] = static_cast< u8 >( ( colors[ 0 ][ channel ] + colors[ 1 ][ channel ] ) / 2 );
            colors[ 2 ][ 3 ] = 255;
        }
        for ( i32 pixel{ }; pixel < 16; ++pixel )
            atl::memcpy( pixels[ pixel ], colors[ ( indices >> ( pixel * 2 ) ) & 3 ], 4 );
    }

    bool decode_vtf( const atl::vector< u8 > &file, atl::vector< u8 > &rgba, i32 &width, i32 &height )
    {
        if ( file.size( ) < 64 || file[ 0 ] != 'V' || file[ 1 ] != 'T' || file[ 2 ] != 'F' ||
             file[ 3 ] || read_u32( file.data( ) + 4 ) != 7 ) return false;
        const i32 minor{ static_cast< i32 >( read_u32( file.data( ) + 8 ) ) };
        if ( minor < 0 || minor > 5 ) return false;
        width = read_u16( file.data( ) + 16 );
        height = read_u16( file.data( ) + 18 );
        const i32 frames{ math::bigger( static_cast< i32 >( read_u16( file.data( ) + 24 ) ), 1 ) };
        const i32 format{ static_cast< i32 >( read_u32( file.data( ) + 52 ) ) };
        const i32 mips{ math::bigger( static_cast< i32 >( file[ 56 ] ), 1 ) };
        if ( width <= 0 || height <= 0 || width > 2048 || height > 2048 ) return false;

        usize offset{ };
        if ( minor >= 3 )
        {
            if ( file.size( ) < 80 ) return false;
            const i32 resources{ math::smaller( static_cast< i32 >( read_u32( file.data( ) + 68 ) ), 32 ) };
            bool found{ };
            for ( i32 index{ }; index < resources; ++index )
            {
                const usize entry{ 80u + static_cast< usize >( index ) * 8u };
                if ( entry + 8 > file.size( ) ) return false;
                if ( file[ entry ] == 0x30 && !file[ entry + 1 ] && !file[ entry + 2 ] && !( file[ entry + 3 ] & 2 ) )
                {
                    offset = read_u32( file.data( ) + entry + 4 );
                    found = true;
                    break;
                }
            }
            if ( !found ) return false;
        } else
        {
            offset = read_u32( file.data( ) + 12 );
            const i32 low_width{ file[ 61 ] }, low_height{ file[ 62 ] };
            if ( low_width && low_height ) offset += vtf_bytes( static_cast< i32 >( read_u32( file.data( ) + 57 ) ), low_width, low_height );
        }
        for ( i32 mip{ mips - 1 }; mip >= 1; --mip )
        {
            const i32 bytes{ vtf_bytes( format, math::bigger( width >> mip, 1 ), math::bigger( height >> mip, 1 ) ) };
            if ( bytes <= 0 ) return false;
            offset += static_cast< usize >( bytes ) * frames;
        }
        const i32 image_bytes{ vtf_bytes( format, width, height ) };
        if ( image_bytes <= 0 || offset + image_bytes > file.size( ) ||
             !rgba.resize( static_cast< usize >( width ) * height * 4 ) ) return false;
        const u8 *source{ file.data( ) + offset };
        if ( format == VTF_RGBA8888 || format == VTF_BGRA8888 || format == VTF_BGRX8888 )
        {
            for ( i32 pixel{ }; pixel < width * height; ++pixel )
            {
                const u8 *in{ source + pixel * 4 };
                u8 *out{ rgba.data( ) + pixel * 4 };
                if ( format == VTF_RGBA8888 ) atl::memcpy( out, in, 4 );
                else { out[ 0 ] = in[ 2 ]; out[ 1 ] = in[ 1 ]; out[ 2 ] = in[ 0 ]; out[ 3 ] = format == VTF_BGRX8888 ? 255 : in[ 3 ]; }
            }
            return true;
        }
        const i32 blocks_x{ ( width + 3 ) / 4 }, blocks_y{ ( height + 3 ) / 4 };
        for ( i32 block_y{ }; block_y < blocks_y; ++block_y )
        {
            for ( i32 block_x{ }; block_x < blocks_x; ++block_x )
            {
                const bool dxt5{ format == VTF_DXT5 };
                if ( !dxt5 && format != VTF_DXT1 && format != VTF_DXT1_ALPHA ) return false;
                const u8 *block{ source + ( block_y * blocks_x + block_x ) * ( dxt5 ? 16 : 8 ) };
                u8 pixels[ 16 ][ 4 ]{ };
                decode_dxt_color( block + ( dxt5 ? 8 : 0 ), pixels, dxt5 );
                if ( dxt5 )
                {
                    u8 alpha[ 8 ]{ block[ 0 ], block[ 1 ] };
                    if ( alpha[ 0 ] > alpha[ 1 ] )
                        for ( i32 index{ 1 }; index <= 6; ++index ) alpha[ index + 1 ] = static_cast< u8 >( ( ( 7 - index ) * alpha[ 0 ] + index * alpha[ 1 ] ) / 7 );
                    else
                    {
                        for ( i32 index{ 1 }; index <= 4; ++index ) alpha[ index + 1 ] = static_cast< u8 >( ( ( 5 - index ) * alpha[ 0 ] + index * alpha[ 1 ] ) / 5 );
                        alpha[ 6 ] = 0; alpha[ 7 ] = 255;
                    }
                    u64 bits{ };
                    for ( i32 index{ }; index < 6; ++index ) bits |= static_cast< u64 >( block[ index + 2 ] ) << ( index * 8 );
                    for ( i32 pixel{ }; pixel < 16; ++pixel ) pixels[ pixel ][ 3 ] = alpha[ ( bits >> ( pixel * 3 ) ) & 7 ];
                }
                for ( i32 pixel{ }; pixel < 16; ++pixel )
                {
                    const i32 x{ block_x * 4 + ( pixel & 3 ) }, y{ block_y * 4 + ( pixel >> 2 ) };
                    if ( x < width && y < height ) atl::memcpy( rgba.data( ) + ( y * width + x ) * 4, pixels[ pixel ], 4 );
                }
            }
        }
        return true;
    }

    bool intersect_planes( const brush_plane_t &a, const brush_plane_t &b,
                           const brush_plane_t &c, vec3 &out )
    {
        const vec3 bc{ b.m_normal.cross( c.m_normal ) };
        const f32 determinant{ a.m_normal.dot( bc ) };
        if ( absf( determinant ) < 0.0001f ) return false;
        out = ( bc * a.m_distance + c.m_normal.cross( a.m_normal ) * b.m_distance +
                a.m_normal.cross( b.m_normal ) * c.m_distance ) / determinant;
        return true;
    }
}

void radar::on_level_shutdown( )
{
    atl::scoped_lock lock{ this->m_publish };
    this->m_segments.clear( );
    this->m_floors.clear( );
    this->m_markers.clear( );
    this->m_cache_valid = false;
    this->m_display_height_valid = false;
    hide_indicator( this->m_card );
}

bool radar::load_class_icons( )
{
    if ( g_cfg.m_radar.m_icon_style != config::RADAR_ICON_CLASS ) return false;
    const bool custom{ g_cfg.m_radar.m_custom_icons };
    if ( this->m_class_icons_attempted && this->m_class_icons_custom != custom )
    {
        this->m_class_icons.clear( );
        this->m_class_icons_loaded = false;
        this->m_class_icons_attempted = false;
        this->m_class_icons_need_upload = false;
    }
    this->m_class_icons_custom = custom;
    if ( this->m_class_icons_loaded ) return true;
    if ( this->m_class_icons_attempted || !g_tf2.m_filesystem ) return false;
    this->m_class_icons_attempted = true;

    static const char *paths[ CLASS_ICON_COUNT ]{
        nullptr,
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_scout.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_sniper.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_soldier.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_demo.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_medic.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_heavy.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_pyro.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_spy.vtf" ),
        ASTRAL_XORSTR_STATIC( "materials/hud/leaderboard_class_engineer.vtf" ),
    };

    const usize astral_bytes{ static_cast< usize >( CLASS_ICON_SIZE ) * CLASS_ICON_SIZE *
                             CLASS_ICON_COUNT * 4 };
    if ( !this->m_class_icons.resize( astral_bytes ) ) return false;
    atl::memset( this->m_class_icons.data( ), 0, astral_bytes );

    for ( u32 player_class{ 1 }; player_class < CLASS_ICON_COUNT; ++player_class )
    {
        char resolved[ 1024 ]{ };
        const char *open_path{ paths[ player_class ] };
        const char *path_id{ ASTRAL_XORSTR_STATIC( "GAME" ) };
        if ( !custom )
        {
            if ( !g_tf2.m_filesystem->RelativePathToFullPath( paths[ player_class ], path_id,
                                                               resolved, sizeof( resolved ),
                                                               PATH_FILTER_CULL_NONPACK ) )
                return false;
            open_path = resolved;
            path_id = nullptr;
        }
        FileHandle_t file{ g_tf2.m_filesystem->Open( open_path, ASTRAL_XORSTR_STATIC( "rb" ), path_id ) };
        if ( !file ) return false;
        const u32 size{ g_tf2.m_filesystem->Size( file ) };
        atl::vector< u8 > bytes{ };
        const bool valid_size{ size >= 64 && size <= 12u * 1024u * 1024u && bytes.resize( size ) };
        const i32 read{ valid_size ? g_tf2.m_filesystem->Read( bytes.data( ), static_cast< i32 >( size ), file ) : 0 };
        g_tf2.m_filesystem->Close( file );
        if ( !valid_size || read != static_cast< i32 >( size ) ) return false;

        atl::vector< u8 > rgba{ };
        i32 width{ }, height{ };
        if ( !decode_vtf( bytes, rgba, width, height ) ) return false;

        const f32 scale{ math::smaller( 60.0f / static_cast< f32 >( width ),
                                        60.0f / static_cast< f32 >( height ) ) };
        const i32 draw_width{ math::bigger( static_cast< i32 >( width * scale ), 1 ) };
        const i32 draw_height{ math::bigger( static_cast< i32 >( height * scale ), 1 ) };
        const i32 left{ ( static_cast< i32 >( CLASS_ICON_SIZE ) - draw_width ) / 2 };
        const i32 top{ ( static_cast< i32 >( CLASS_ICON_SIZE ) - draw_height ) / 2 };
        const u32 astral_width{ CLASS_ICON_SIZE * CLASS_ICON_COUNT };
        for ( i32 y{ }; y < draw_height; ++y )
        {
            const i32 source_y{ math::smaller( y * height / draw_height, height - 1 ) };
            for ( i32 x{ }; x < draw_width; ++x )
            {
                const i32 source_x{ math::smaller( x * width / draw_width, width - 1 ) };
                const u8 *source{ rgba.data( ) + ( source_y * width + source_x ) * 4 };
                u8 *destination{ this->m_class_icons.data( ) +
                    ( static_cast< usize >( top + y ) * astral_width +
                      player_class * CLASS_ICON_SIZE + left + x ) * 4 };
                destination[ 0 ] = source[ 2 ];
                destination[ 1 ] = source[ 1 ];
                destination[ 2 ] = source[ 0 ];
                destination[ 3 ] = source[ 3 ];
            }
        }
    }

    this->m_class_icons_loaded = true;
    this->m_class_icons_need_upload = true;
    return true;
}

void radar::think( )
{
    if ( !g_cfg.m_radar.m_enabled || !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        atl::scoped_lock lock{ this->m_publish };
        this->m_markers.clear( );
        return;
    }

    atl::vector< marker_t > markers{ };
    for ( const entity_cache::entry_t *entry : g_entity_cache.visuals( ) )
    {
        if ( !entry || !entry->networked( ) || !entry->m_entity || entry->m_entity == g_cl.m_local ) continue;

        C_BaseEntity *entity{ entry->m_entity };
        marker_t marker{ };
        marker.m_origin   = entity->m_vecOrigin( );
        marker.m_class_id = entry->m_class_id;
        marker.m_index    = entry->m_index;
        marker.m_team     = entity->m_iTeamNum( );
        marker.m_dormant  = entity->IsDormant( );

        if ( entry->m_class_id == ETFClassID::CTFPlayer )
        {
            if ( marker.m_dormant )
            {
                float remaining{ };
                if ( !g_player_data.dormant_origin( entity, marker.m_origin, remaining ) ) continue;
                marker.m_team  = g_player_data.team( entry->m_index );
                marker.m_alive = g_player_data.alive( entry->m_index );
                marker.m_player_class = g_player_data.player_class( entry->m_index );
            } else
            {
                C_TFPlayer *player{ entity->As< C_TFPlayer * >( ) };
                marker.m_alive        = player->IsAlive( );
                marker.m_player_class = player->m_iClass( );
            }

            player_info_t info{ };
            if ( g_tf2.m_engine->GetPlayerInfo( entry->m_index, &info ) )
                str::copy( marker.m_name, sizeof( marker.m_name ), info.name );
        }

        markers.emplace_back( marker );
    }

    atl::scoped_lock lock{ this->m_publish };
    this->load_class_icons( );
    this->m_markers = static_cast< atl::vector< marker_t > && >( markers );
    this->m_local_class = g_cl.m_local->m_iClass( );

    const vec3 origin{ g_cl.m_local->m_vecOrigin( ) };
    const vec3 moved{ origin - this->m_cache_origin };
    if ( g_cfg.m_radar.m_map_mode == config::RADAR_MAP_OFF )
    {
        this->m_segments.clear( );
        this->m_floors.clear( );
        this->m_cache_valid = false;
    } else if ( !this->m_cache_valid || moved.length2d( ) > RADAR_CACHE_MOVE ||
                absf( moved.z ) > static_cast< f32 >( g_cfg.m_radar.m_map_detail ) * 160.0f ||
                this->m_cache_range != g_cfg.m_radar.m_range ||
                this->m_cache_detail != g_cfg.m_radar.m_map_detail ||
                this->m_cache_mode != g_cfg.m_radar.m_map_mode )
    {
        this->rebuild_map( origin );
    }
}

void radar::rebuild_map( const vec3 &origin )
{
    this->m_segments.clear( );
    this->m_floors.clear( );
    this->m_cache_origin = origin;
    this->m_cache_range  = g_cfg.m_radar.m_range;
    this->m_cache_detail = g_cfg.m_radar.m_map_detail;
    this->m_cache_mode   = g_cfg.m_radar.m_map_mode;
    this->m_cache_valid  = true;

    if ( g_cfg.m_radar.m_map_mode == config::RADAR_MAP_OFF || !g_tf2.m_trace ) return;

    const f32 range{ static_cast< f32 >( g_cfg.m_radar.m_range ) };
    const i32 layers{ g_cfg.m_radar.m_map_mode == config::RADAR_MAP_MSG
                          ? 1 + g_cfg.m_radar.m_map_detail * 2
                          : 1 };
    const f32 step{ 128.0f };

    CUtlVector< i32 > brushes{ };
    g_tf2.m_trace->GetBrushesInAABB( vec3{ origin.x - range, origin.y - range, origin.z - 512.0f },
                                     vec3{ origin.x + range, origin.y + range, origin.z + 512.0f },
                                     &brushes, MASK_PLAYERSOLID_BRUSHONLY );

    const usize brush_count{ math::smaller( static_cast< usize >( brushes.m_Size ), RADAR_BRUSH_LIMIT ) };
    for ( usize brush_index{ }; brush_index < brush_count && this->m_segments.size( ) < RADAR_SEGMENT_LIMIT; ++brush_index )
    {
        CUtlVector< brush_plane_t > planes{ };
        i32 contents{ };
        if ( !g_tf2.m_trace->GetBrushInfo( brushes.m_pElements[ brush_index ], &planes, &contents ) ||
             !( contents & CONTENTS_SOLID ) || planes.m_Size < 4 )
            continue;

        for ( i32 face{ }; face < planes.m_Size && this->m_floors.size( ) < RADAR_FLOOR_LIMIT; ++face )
        {
            const brush_plane_t &floor_plane{ planes.m_pElements[ face ] };
            if ( floor_plane.m_normal.z < 0.70f ) continue;

            vec3 vertices[ 32 ]{ };
            usize vertex_count{ };
            for ( i32 first{ }; first < planes.m_Size && vertex_count < 32; ++first )
            {
                if ( first == face ) continue;
                for ( i32 second{ first + 1 }; second < planes.m_Size && vertex_count < 32; ++second )
                {
                    if ( second == face ) continue;
                    vec3 vertex{ };
                    if ( !intersect_planes( floor_plane, planes.m_pElements[ first ],
                                            planes.m_pElements[ second ], vertex ) )
                        continue;

                    bool inside{ true };
                    for ( i32 clip{ }; clip < planes.m_Size; ++clip )
                    {
                        if ( planes.m_pElements[ clip ].m_normal.dot( vertex ) >
                             planes.m_pElements[ clip ].m_distance + 0.75f )
                        {
                            inside = false;
                            break;
                        }
                    }
                    if ( !inside ) continue;

                    bool duplicate{ };
                    for ( usize existing{ }; existing < vertex_count; ++existing )
                    {
                        if ( ( vertices[ existing ] - vertex ).length_sqr( ) < 1.0f )
                        {
                            duplicate = true;
                            break;
                        }
                    }
                    if ( !duplicate ) vertices[ vertex_count++ ] = vertex;
                }
            }
            if ( vertex_count < 3 ) continue;

            vec3 centre{ };
            for ( usize index{ }; index < vertex_count; ++index ) centre += vertices[ index ];
            centre /= static_cast< f32 >( vertex_count );
            const f32 vertical_range{ static_cast< f32 >( g_cfg.m_radar.m_map_detail ) * 192.0f + 64.0f };
            if ( absf( centre.z - origin.z ) > vertical_range ||
                 ( centre - origin ).length2d( ) > range * 1.45f )
                continue;

            for ( usize index{ 1 }; index < vertex_count; ++index )
            {
                const vec3 value{ vertices[ index ] };
                const f32 angle{ math::atan2( value.y - centre.y, value.x - centre.x ) };
                usize at{ index };
                while ( at && math::atan2( vertices[ at - 1 ].y - centre.y,
                                            vertices[ at - 1 ].x - centre.x ) > angle )
                {
                    vertices[ at ] = vertices[ at - 1 ];
                    --at;
                }
                vertices[ at ] = value;
            }

            floor_t floor{ };
            floor.m_count  = static_cast< u8 >( math::smaller( vertex_count, usize{ 8 } ) );
            floor.m_height = centre.z;
            if ( vertex_count <= 8 )
            {
                for ( usize index{ }; index < vertex_count; ++index ) floor.m_points[ index ] = vertices[ index ];
            } else
            {
                for ( usize index{ }; index < 8; ++index )
                    floor.m_points[ index ] = vertices[ index * vertex_count / 8 ];
            }
            this->m_floors.emplace_back( floor );
        }

        for ( i32 face{ }; face < planes.m_Size && this->m_segments.size( ) < RADAR_SEGMENT_LIMIT; ++face )
        {
            const brush_plane_t &wall{ planes.m_pElements[ face ] };
            const f32 normal_length{ wall.m_normal.x * wall.m_normal.x + wall.m_normal.y * wall.m_normal.y };
            if ( normal_length < 0.20f || absf( wall.m_normal.z ) > 0.72f ) continue;

            for ( i32 layer{ }; layer < layers && this->m_segments.size( ) < RADAR_SEGMENT_LIMIT; ++layer )
            {
                const f32 z{ origin.z + static_cast< f32 >( layer - layers / 2 ) * step };
                const f32 slice_distance{ wall.m_distance - wall.m_normal.z * z };
                const vec3 point{ wall.m_normal.x * slice_distance / normal_length,
                                  wall.m_normal.y * slice_distance / normal_length, z };
                const vec3 direction{ -wall.m_normal.y, wall.m_normal.x, 0.0f };
                f32 low{ -range * 2.0f };
                f32 high{ range * 2.0f };
                bool valid{ true };

                for ( i32 clip{ }; clip < planes.m_Size; ++clip )
                {
                    const brush_plane_t &plane{ planes.m_pElements[ clip ] };
                    const f32 divisor{ plane.m_normal.x * direction.x + plane.m_normal.y * direction.y };
                    const f32 room{ plane.m_distance - plane.m_normal.z * z -
                                    plane.m_normal.x * point.x - plane.m_normal.y * point.y };
                    if ( absf( divisor ) < 0.0001f )
                    {
                        if ( room < -0.25f ) valid = false;
                        continue;
                    }
                    const f32 at{ room / divisor };
                    if ( divisor > 0.0f ) high = math::smaller( high, at );
                    else low = math::bigger( low, at );
                    if ( low > high )
                    {
                        valid = false;
                        break;
                    }
                }

                if ( !valid || high - low < 8.0f ) continue;
                const vec3 from{ point + direction * low };
                const vec3 to{ point + direction * high };
                const vec3 middle{ ( from + to ) * 0.5f };
                if ( ( middle - origin ).length2d( ) > range * 1.45f ) continue;
                this->m_segments.emplace_back( segment_t{ from, to } );
            }
        }
    }
}

bool radar::project( const vec3 &world, const vec3 &origin, const rect_t &content,
                     float yaw, bool clamp, point_t &out ) const
{
    const vec3 delta{ world - origin };
    constexpr f32 PI{ 3.14159265358979323846f };
    const f32 radians{ yaw * PI / 180.0f };
    const f32 sine{ math::sin( radians ) };
    const f32 cosine{ math::cos( radians ) };
    f32 right{ sine * delta.x - cosine * delta.y };
    f32 forward{ cosine * delta.x + sine * delta.y };
    const f32 range{ static_cast< f32 >( g_cfg.m_radar.m_range ) };
    const f32 half{ static_cast< f32 >( math::smaller( content.right - content.left,
                                                       content.bottom - content.top ) ) * 0.5f - 5.0f };
    const f32 scale{ half / range };

    if ( g_cfg.m_radar.m_map_mode == config::RADAR_MAP_MSG )
    {
        forward *= 0.68f;
        forward += delta.z * 0.30f;
    }

    if ( !clamp && ( absf( right ) > range || absf( forward ) > range ) ) return false;
    if ( clamp )
    {
        right   = math::clamp( right, -range, range );
        forward = math::clamp( forward, -range, range );
    }

    out.x = ( content.left + content.right ) / 2 + static_cast< i32 >( right * scale );
    out.y = ( content.top + content.bottom ) / 2 - static_cast< i32 >( forward * scale );
    return true;
}

u8 radar::layer_opacity( f32 height ) const
{
    const f32 difference{ height - this->m_display_height };
    const f32 proximity{ math::clamp( 1.0f - absf( difference ) / 128.0f, 0.0f, 1.0f ) };
    const f32 floor{ difference < -16.0f ? 0.16f : 0.38f };
    return static_cast< u8 >( ( floor + ( 1.0f - floor ) * math::smooth_step( proximity ) ) * 255.0f );
}

void radar::draw_entity( const marker_t &marker, const vec3 &origin,
                         const rect_t &content, float yaw, u8 alpha )
{
    const ETFClassID id{ marker.m_class_id };
    const vec3 position{ marker.m_origin };
    color_t color{ };
    i32 radius{ 3 };
    bool player{ id == ETFClassID::CTFPlayer };
    const bool teammate{ marker.m_team == g_cl.m_local->m_iTeamNum( ) };

    if ( g_cfg.m_radar.m_map_mode != config::RADAR_MAP_OFF )
    {
        const f32 vertical_range{ static_cast< f32 >( g_cfg.m_radar.m_map_detail ) * 192.0f + 64.0f };
        if ( absf( position.z - this->m_display_height ) > vertical_range ) return;
        alpha = static_cast< u8 >( static_cast< u32 >( alpha ) *
                                   this->layer_opacity( position.z ) / 255u );
    }

    if ( player )
    {
        if ( marker.m_dormant && !g_cfg.m_radar.m_show_dormant ) return;
        if ( !marker.m_alive ) return;
        if ( teammate ? !g_cfg.m_radar.m_show_teammates : !g_cfg.m_radar.m_show_enemies ) return;
        color  = teammate ? g_cfg.m_radar.m_teammate_color : g_cfg.m_radar.m_enemy_color;
        radius = 4;
    } else if ( radar_building( id ) )
    {
        if ( !g_cfg.m_radar.m_show_buildings ||
             ( teammate ? !g_cfg.m_radar.m_show_teammates : !g_cfg.m_radar.m_show_enemies ) ) return;
        color = g_cfg.m_radar.m_building_color;
    } else if ( radar_projectile( id ) )
    {
        if ( !g_cfg.m_radar.m_show_projectiles ) return;
        color  = g_cfg.m_radar.m_projectile_color;
        radius = 2;
    } else if ( id == ETFClassID::CCaptureFlag )
    {
        if ( !g_cfg.m_radar.m_show_objectives ) return;
        color  = g_cfg.m_radar.m_objective_color;
        radius = 4;
    } else if ( id == ETFClassID::CBaseAnimating || id == ETFClassID::CTFAmmoPack )
    {
        if ( !g_cfg.m_radar.m_show_pickups ) return;
        color = g_cfg.m_radar.m_pickup_color;
    } else if ( radar_npc( id ) )
    {
        if ( !g_cfg.m_radar.m_show_npcs ) return;
        color  = g_cfg.m_radar.m_npc_color;
        radius = 4;
    } else return;

    point_t point{ };
    if ( !this->project( position, origin, content, yaw, false, point ) ) return;
    color = gui::with_alpha( color, alpha );
    const i32 icon_size{ g_cfg.m_radar.m_icon_size };
    const i32 icon_half{ icon_size / 2 };
    if ( player )
    {
        if ( g_cfg.m_radar.m_icon_style == config::RADAR_ICON_DOT )
        {
            radar_filled_circle( point, color, static_cast< f32 >( math::bigger( icon_size / 4, 3 ) ) );
        }
        else
        {
        radar_filled_circle( point, color, static_cast< f32 >( icon_half + 2 ) );
        const color_t icon_color{ 255, 255, 255, alpha };
        if ( g_cfg.m_radar.m_icon_style == config::RADAR_ICON_CLASS && this->m_class_icons_loaded && marker.m_player_class > TF_CLASS_UNDEFINED &&
             marker.m_player_class <= TF_CLASS_ENGINEER )
            g_render.icon( static_cast< u32 >( marker.m_player_class ), render::icons::KIND_TF2_CLASS,
                           point_t{ point.x - icon_half, point.y - icon_half }, icon_color,
                           0.0f, false, icon_size );
        else
            gui::draw_icon_at( ICON_USER, point_t{ point.x - icon_half, point.y - icon_half },
                               icon_color, icon_size );
        }
    } else
    {
        const char *glyph{ radar_building( id ) ? ICON_WRENCH
                           : radar_projectile( id ) ? ICON_ZAP
                           : id == ETFClassID::CCaptureFlag ? ICON_SPARKLES
                           : ( id == ETFClassID::CBaseAnimating || id == ETFClassID::CTFAmmoPack ) ? ICON_CROSS
                           : radar_npc( id ) ? ICON_TRIANGLE_ALERT
                                             : ICON_USER };
        gui::draw_icon_at( glyph, point_t{ point.x - icon_half, point.y - icon_half }, color, icon_size );
    }

    if ( player && g_cfg.m_radar.m_show_names )
    {
        if ( marker.m_name[ 0 ] )
            g_render.text( MENU_FONT_TINY( ), point_t{ point.x + icon_half + 3, point.y - 5 }, color, marker.m_name );
    }
}

void radar::paint( )
{
    if ( !g_cfg.m_radar.m_enabled )
    {
        hide_indicator( this->m_card );
        return;
    }

    const bool preview{ g_gui.open( ) };
    const bool ready{ g_tf2.m_engine->IsInGame( ) && g_cl.m_local };
    if ( !ready && !preview )
    {
        hide_indicator( this->m_card );
        return;
    }

    const i32 size{ g_cfg.m_radar.m_size };
    this->m_card.m_at = g_cfg.m_radar.m_position;
    const u32 frame_delta{ step_indicator( this->m_card, 0.0f, g_cl.m_screen_width,
                                           g_cl.m_screen_height, size, size, true ) };
    g_cfg.m_radar.m_position = this->m_card.m_at;
    const indicator_frame_t frame{ draw_indicator_frame( this->m_card, ASTRAL_XORSTR_STATIC( "radar" ), CARD_TITLE_TEXT ) };
    const u8 alpha{ frame.m_enter };
    const rect_t content{ frame.m_content };
    const vec3 origin{ ready ? g_cl.m_local->m_vecOrigin( ) : vec3{ } };
    if ( ready && !this->m_display_height_valid )
    {
        this->m_display_height       = origin.z;
        this->m_display_height_valid = true;
    } else if ( ready )
    {
        this->m_display_height = gui::approach_float( this->m_display_height, origin.z,
                                                       frame_delta, 320.0f );
    }
    vec3 render_origin{ origin };
    render_origin.z = this->m_display_height;
    vec3 view{ };
    if ( ready ) g_tf2.m_engine->GetViewAngles( view );
    const f32 yaw{ g_cfg.m_radar.m_rotate ? view.y : 0.0f };

    g_render.set_clip( content );
    g_render.box( content, gui::with_alpha( CARD_BACKGROUND, alpha ), true );
    const point_t centre{ ( content.left + content.right ) / 2, ( content.top + content.bottom ) / 2 };
    g_render.line( point_t{ content.left, centre.y }, point_t{ content.right, centre.y },
                   gui::with_alpha( CONTROL_BORDER, alpha ) );
    g_render.line( point_t{ centre.x, content.top }, point_t{ centre.x, content.bottom },
                   gui::with_alpha( CONTROL_BORDER, alpha ) );

    atl::scoped_lock lock{ this->m_publish };

    if ( this->m_class_icons_loaded )
    {
        if ( g_render.upload_icon_astral( render::icons::KIND_TF2_CLASS, this->m_class_icons.data( ),
                                         CLASS_ICON_SIZE, CLASS_ICON_COUNT,
                                         this->m_class_icons_need_upload ) )
            this->m_class_icons_need_upload = false;
    }

    if ( ready && g_cfg.m_radar.m_map_mode != config::RADAR_MAP_OFF )
    {
        if ( g_cfg.m_radar.m_map_mode == config::RADAR_MAP_DOOM )
        {
            for ( i32 active_pass{ }; active_pass < 2; ++active_pass )
            {
                for ( const floor_t &floor : this->m_floors )
                {
                    const bool active{ absf( floor.m_height - this->m_display_height ) < 64.0f };
                    if ( active != ( active_pass != 0 ) ) continue;

                    vec2 points[ 8 ]{ };
                    color_t colors[ 8 ]{ };
                    bool visible{ true };
                    const u8 layer{ this->layer_opacity( floor.m_height ) };
                    const u8 opacity{ static_cast< u8 >( static_cast< u32 >( alpha ) * layer / 255u ) };
                    color_t fill{ g_cfg.m_radar.m_map_color };
                    fill.a = static_cast< u8 >( static_cast< u32 >( fill.a ) * 3u / 5u );
                    fill = gui::with_alpha( fill, opacity );
                    for ( usize index{ }; index < floor.m_count; ++index )
                    {
                        point_t projected{ };
                        if ( !this->project( floor.m_points[ index ], render_origin, content, yaw, true, projected ) )
                        {
                            visible = false;
                            break;
                        }
                        points[ index ] = vec2{ static_cast< f32 >( projected.x ), static_cast< f32 >( projected.y ) };
                        colors[ index ] = fill;
                    }
                    if ( visible ) g_render.polygon( points, colors, floor.m_count );
                }
            }
        }

        for ( i32 active_pass{ }; active_pass < 2; ++active_pass )
        {
            for ( const segment_t &segment : this->m_segments )
            {
                const bool active{ absf( segment.m_from.z - this->m_display_height ) < 64.0f };
                if ( active != ( active_pass != 0 ) ) continue;
                const u8 layer{ this->layer_opacity( segment.m_from.z ) };
                const u8 opacity{ static_cast< u8 >( static_cast< u32 >( alpha ) * layer / 255u ) };
                const color_t line{ gui::with_alpha( g_cfg.m_radar.m_map_color, opacity ) };
                point_t from{ }, to{ };
                if ( this->project( segment.m_from, render_origin, content, yaw, true, from ) &&
                     this->project( segment.m_to, render_origin, content, yaw, true, to ) )
                    g_render.line( from, to, line, active ? 1.5f : 1.0f );
            }
        }
    }

    if ( ready )
    {
        for ( const marker_t &marker : this->m_markers )
            this->draw_entity( marker, render_origin, content, yaw, alpha );

        if ( g_cfg.m_radar.m_show_local )
        {
            const color_t local{ gui::with_alpha( g_cfg.m_radar.m_local_color, alpha ) };
            const i32 icon_size{ g_cfg.m_radar.m_icon_size };
            const i32 icon_half{ icon_size / 2 };
            if ( g_cfg.m_radar.m_icon_style == config::RADAR_ICON_DOT )
            {
                radar_filled_circle( centre, local, static_cast< f32 >( math::bigger( icon_size / 4, 3 ) ) );
                g_render.reset_clip( );
                return;
            }
            radar_filled_circle( centre, local, static_cast< f32 >( icon_half + 2 ) );
            const color_t icon_color{ 255, 255, 255, alpha };
            if ( g_cfg.m_radar.m_icon_style == config::RADAR_ICON_CLASS && this->m_class_icons_loaded && this->m_local_class > TF_CLASS_UNDEFINED &&
                 this->m_local_class <= TF_CLASS_ENGINEER )
                g_render.icon( static_cast< u32 >( this->m_local_class ), render::icons::KIND_TF2_CLASS,
                               point_t{ centre.x - icon_half, centre.y - icon_half }, icon_color,
                               0.0f, false, icon_size );
            else
                gui::draw_icon_at( ICON_USER, point_t{ centre.x - icon_half, centre.y - icon_half },
                                   icon_color, icon_size );
        }
    } else
    {
        g_render.text( MENU_FONT_TINY( ), centre, gui::with_alpha( MENU_TEXT_DIM, alpha ),
                       ASTRAL_XORSTR_STATIC( "map preview" ), false, render::text_align::center );
    }
    g_render.reset_clip( );
}
