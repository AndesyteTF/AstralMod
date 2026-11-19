#pragma once

#include "link.hpp"

struct movement_surface_t
{
    vec3 m_point{ };
    vec3 m_normal{ };
    f32  m_distance{ FLT_MAX };
};

struct movement_lip_t
{
    vec3 m_point{ };
    vec3 m_direction{ };
    f32  m_distance{ FLT_MAX };
};

class movement_surface_scanner
{
public:
    static bool nearest_wall( C_TFPlayer *player, f32 reach, movement_surface_t &out )
    {
        if ( !player )
        {
            return false;
        }

        constexpr i32 DIRECTIONS{ 16 };
        const vec3 origin{ player->GetAbsOrigin( ) };
        const vec3 mins{ player->m_vecMins( ) };
        const vec3 maxs{ player->m_vecMaxs( ) };
        CTraceFilterSimple filter{ player, COLLISION_GROUP_PLAYER_MOVEMENT };

        out = movement_surface_t{ };
        for ( i32 index{ }; index < DIRECTIONS; ++index )
        {
            const f32 angle{ DEG2RAD( 360.0f * static_cast< f32 >( index ) /
                                      static_cast< f32 >( DIRECTIONS ) ) };
            const vec3 direction{ math::cos( angle ), math::sin( angle ), 0.0f };
            Ray_t ray{ };
            trace_t trace{ };
            ray.Init( origin, origin + direction * reach, mins, maxs );
            g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );

            if ( trace.startsolid || trace.fraction >= 1.0f ||
                 math::abs( trace.plane.normal.z ) >= 0.7f )
            {
                continue;
            }

            const f32 distance{ reach * trace.fraction };
            if ( distance < out.m_distance )
            {
                out.m_point = trace.endpos;
                out.m_normal = trace.plane.normal;
                out.m_distance = distance;
            }
        }

        return out.m_distance < FLT_MAX;
    }

    static bool first_wall_on_path( C_TFPlayer *player, i32 ticks, movement_surface_t &out,
                                    i32 &contact_tick )
    {
        if ( !player )
        {
            return false;
        }

        const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) *
                           ( player->m_flGravity( ) != 0.0f ? player->m_flGravity( ) : 1.0f ) };
        vec3 position{ player->GetAbsOrigin( ) };
        vec3 velocity{ player->m_vecVelocity( ) };
        CTraceFilterSimple filter{ player, COLLISION_GROUP_PLAYER_MOVEMENT };

        for ( i32 tick{ 1 }; tick <= ticks; ++tick )
        {
            velocity.z -= gravity * TICK_INTERVAL;
            const vec3 next{ position + velocity * TICK_INTERVAL };
            Ray_t ray{ };
            trace_t trace{ };
            ray.Init( position, next, player->m_vecMins( ), player->m_vecMaxs( ) );
            g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );
            position = trace.endpos;

            if ( trace.startsolid )
            {
                return false;
            }
            if ( trace.fraction >= 1.0f )
            {
                continue;
            }
            if ( math::abs( trace.plane.normal.z ) >= 0.7f )
            {
                return false;
            }

            out.m_point = trace.endpos;
            out.m_normal = trace.plane.normal;
            out.m_distance = player->GetAbsOrigin( ).dist_to( trace.endpos );
            contact_tick = tick;
            return true;
        }

        return false;
    }

    static bool nearest_seam( C_TFPlayer *player, f32 reach, movement_surface_t &out )
    {
        if ( !player )
        {
            return false;
        }

        constexpr i32 DIRECTIONS{ 16 };
        const vec3 origin{ player->GetAbsOrigin( ) };
        CTraceFilterSimple filter{ player, COLLISION_GROUP_PLAYER_MOVEMENT };
        out = movement_surface_t{ };

        for ( i32 index{ }; index < DIRECTIONS; ++index )
        {
            const f32 angle{ DEG2RAD( 360.0f * static_cast< f32 >( index ) /
                                      static_cast< f32 >( DIRECTIONS ) ) };
            const vec3 direction{ math::cos( angle ), math::sin( angle ), 0.0f };
            trace_t lower{ }, upper{ };
            Ray_t lower_ray{ }, upper_ray{ };
            const vec3 lower_start{ origin + vec3( 0.0f, 0.0f, 4.0f ) };
            const vec3 upper_start{ origin + vec3( 0.0f, 0.0f, 36.0f ) };
            lower_ray.Init( lower_start, lower_start + direction * reach );
            upper_ray.Init( upper_start, upper_start + direction * reach );
            g_tf2.m_trace->TraceRay( lower_ray, MASK_PLAYERSOLID, &filter, &lower );
            g_tf2.m_trace->TraceRay( upper_ray, MASK_PLAYERSOLID, &filter, &upper );

            const bool lower_wall{ !lower.startsolid && lower.fraction < 1.0f &&
                                   math::abs( lower.plane.normal.z ) < 0.7f };
            const bool upper_wall{ !upper.startsolid && upper.fraction < 1.0f &&
                                   math::abs( upper.plane.normal.z ) < 0.7f };
            if ( !lower_wall && !upper_wall )
            {
                continue;
            }

            const f32 split{ lower_wall && upper_wall
                                 ? math::abs( lower.fraction - upper.fraction ) * reach +
                                       ( 1.0f - math::abs( lower.plane.normal.dot( upper.plane.normal ) ) ) * reach
                                 : reach };
            if ( split < 0.5f )
            {
                continue;
            }

            const trace_t &chosen{ !upper_wall || ( lower_wall && lower.fraction <= upper.fraction )
                                       ? lower : upper };
            const f32 distance{ chosen.fraction * reach };
            if ( distance < out.m_distance )
            {
                out.m_point = chosen.endpos;
                out.m_normal = chosen.plane.normal;
                out.m_distance = distance;
            }
        }

        return out.m_distance < FLT_MAX;
    }

    static bool nearest_lip( C_TFPlayer *player, f32 reach, movement_lip_t &out )
    {
        if ( !player || !player->IsOnGround( ) )
        {
            return false;
        }

        constexpr i32 DIRECTIONS{ 16 };
        const vec3 origin{ player->GetAbsOrigin( ) };
        const f32 hull_radius{ math::bigger( math::abs( player->m_vecMins( ).x ),
                                             math::abs( player->m_vecMaxs( ).x ) ) };
        CTraceFilterSimple filter{ player, COLLISION_GROUP_PLAYER_MOVEMENT };
        out = movement_lip_t{ };

        for ( i32 index{ }; index < DIRECTIONS; ++index )
        {
            const f32 angle{ DEG2RAD( 360.0f * static_cast< f32 >( index ) /
                                      static_cast< f32 >( DIRECTIONS ) ) };
            const vec3 direction{ math::cos( angle ), math::sin( angle ), 0.0f };
            const vec3 probe{ origin + direction * ( hull_radius + reach ) + vec3( 0.0f, 0.0f, 2.0f ) };
            Ray_t ray{ };
            trace_t trace{ };
            ray.Init( probe, probe - vec3( 0.0f, 0.0f, 12.0f ) );
            g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );
            if ( trace.startsolid || trace.fraction < 1.0f )
            {
                continue;
            }

            out.m_point = probe;
            out.m_direction = direction;
            out.m_distance = reach;
            return true;
        }

        return false;
    }
};
