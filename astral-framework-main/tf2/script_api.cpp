#include "script_api.hpp"

#include "config.hpp"
#include "features/entity_cache.hpp"
#include "features/crits.hpp"
#include "features/networking.hpp"
#include "features/nospread.hpp"
#include "features/prediction.hpp"
#include "features/tickbase.hpp"

#include "gui/gui.hpp"
#include "gui/theme.hpp"
#include "runtime/hash.hpp"
#include "script/bind.hpp"
#include "script/script.hpp"
#include "util/json.hpp"

#include "libs/lua/lauxlib.h"
#include "libs/lua/lua.h"

#include "config_fields.inl"

ASTRAL_NO_DESTROY script_api g_script_api{ };

static char s_entity_key{ };
static char s_cvar_key{ };
static char s_command_key{ };
static char s_event_key{ };
static char s_listeners_key{ };

struct config_binding_t
{
    const bool *m_enabled{ };
    bind_t     *m_bind{ };
};

inline constexpr usize CONFIG_BINDING_CAPACITY{ 64 };
static config_binding_t s_config_bindings[ CONFIG_BINDING_CAPACITY ]{ };
static usize            s_config_binding_count{ };

enum class entity_kind_t : u8
{
    player,
    building,
    projectile,
    weapon,
    other,
    _COUNT,
};

struct entity_ref_t
{
    u32           m_handle{ };
    ETFClassID    m_class_id{ };
    entity_kind_t m_kind{ };
};

inline constexpr usize PROP_CACHE_CAPACITY{ 512 };
inline constexpr u32   PROP_CACHE_MASK{ static_cast< u32 >( PROP_CACHE_CAPACITY - 1 ) };

struct prop_cache_t
{
    const RecvTable      *m_table{ };
    hash_t                m_path{ };
    const netvars::entry *m_entry{ };
};

static prop_cache_t s_props[ PROP_CACHE_CAPACITY ]{ };

ASTRAL_FORCE_INLINE void push_entity( lua_State *state, const entity_ref_t &reference )
{
    *static_cast< entity_ref_t * >( script::new_object( state, sizeof( entity_ref_t ), &s_entity_key ) ) = reference;
}

ASTRAL_FORCE_INLINE void push_cached( lua_State *state, C_BaseEntity *entity, entity_kind_t kind )
{
    const entity_cache::entry_t *entry{ entity ? g_entity_cache.find( entity ) : nullptr };

    if ( !entry )
    {
        lua_pushnil( state );
        return;
    }

    push_entity( state, entity_ref_t{ entry->m_handle, entry->m_class_id, kind } );
}

ASTRAL_FORCE_INLINE entity_ref_t &check_entity( lua_State *state )
{
    return *static_cast< entity_ref_t * >( script::check_object( state, 1, &s_entity_key, ASTRAL_XORSTR( "entity" ).get( ) ) );
}

ASTRAL_FORCE_INLINE C_BaseEntity *resolve( lua_State *state, const entity_ref_t &reference )
{
    C_BaseEntity *entity{ g_entity_cache.resolve( reference.m_handle ) };

    if ( !entity )
    {
        luaL_error( state, ASTRAL_XORSTR( "entity is no longer valid" ).get( ) );
    }

    return entity;
}

ASTRAL_FORCE_INLINE C_BaseEntity *check_kind( lua_State *state, entity_kind_t kind, const char *expected )
{
    const entity_ref_t &reference{ check_entity( state ) };

    if ( reference.m_kind != kind )
    {
        luaL_argerror( state, 1, expected );
    }

    return resolve( state, reference );
}

ASTRAL_FORCE_INLINE C_TFPlayer *check_player( lua_State *state )
{
    return check_kind( state, entity_kind_t::player, ASTRAL_XORSTR( "player expected" ).get( ) )->As< C_TFPlayer * >( );
}

ASTRAL_FORCE_INLINE C_TFWeaponBase *check_weapon( lua_State *state )
{
    return check_kind( state, entity_kind_t::weapon, ASTRAL_XORSTR( "weapon expected" ).get( ) )->As< C_TFWeaponBase * >( );
}

static int entity_valid( lua_State *state )
{
    lua_pushboolean( state, g_entity_cache.resolve( check_entity( state ).m_handle ) != nullptr );
    return 1;
}

static int entity_index( lua_State *state )
{
    lua_pushinteger( state, resolve( state, check_entity( state ) )->entindex( ) );
    return 1;
}

static int entity_handle( lua_State *state )
{
    lua_pushinteger( state, static_cast< lua_Integer >( check_entity( state ).m_handle ) );
    return 1;
}

static int entity_class_id( lua_State *state )
{
    lua_pushinteger( state, static_cast< lua_Integer >( check_entity( state ).m_class_id ) );
    return 1;
}

static int entity_kind( lua_State *state )
{
    switch ( check_entity( state ).m_kind )
    {
        case entity_kind_t::player: lua_pushstring( state, ASTRAL_XORSTR( "player" ).get( ) ); break;
        case entity_kind_t::building: lua_pushstring( state, ASTRAL_XORSTR( "building" ).get( ) ); break;
        case entity_kind_t::projectile: lua_pushstring( state, ASTRAL_XORSTR( "projectile" ).get( ) ); break;
        case entity_kind_t::weapon: lua_pushstring( state, ASTRAL_XORSTR( "weapon" ).get( ) ); break;
        default: lua_pushstring( state, ASTRAL_XORSTR( "other" ).get( ) ); break;
    }

    return 1;
}

static int entity_origin( lua_State *state )
{
    script::push_vec3( state, resolve( state, check_entity( state ) )->GetAbsOrigin( ) );
    return 1;
}

static int entity_center( lua_State *state )
{
    script::push_vec3( state, resolve( state, check_entity( state ) )->WorldSpaceCenter( ) );
    return 1;
}

static int entity_bounds( lua_State *state )
{
    C_BaseEntity *entity{ resolve( state, check_entity( state ) ) };
    script::push_vec3( state, entity->m_vecMins( ) );
    script::push_vec3( state, entity->m_vecMaxs( ) );
    return 2;
}

static int entity_classname( lua_State *state )
{
    const char *name{ resolve( state, check_entity( state ) )->GetClassname( ) };
    if ( name ) lua_pushstring( state, name );
    else lua_pushnil( state );
    return 1;
}

static int entity_distance( lua_State *state )
{
    C_BaseEntity *entity{ resolve( state, check_entity( state ) ) };
    const entity_ref_t &other_ref{ *static_cast< entity_ref_t * >(
        script::check_object( state, 2, &s_entity_key, ASTRAL_XORSTR( "entity" ).get( ) ) ) };
    C_BaseEntity *other{ resolve( state, other_ref ) };
    lua_pushnumber( state, entity->WorldSpaceCenter( ).dist_to( other->WorldSpaceCenter( ) ) );
    return 1;
}

static int entity_team( lua_State *state )
{
    lua_pushinteger( state, resolve( state, check_entity( state ) )->m_iTeamNum( ) );
    return 1;
}

static int entity_dormant( lua_State *state )
{
    lua_pushboolean( state, resolve( state, check_entity( state ) )->IsDormant( ) );
    return 1;
}

static int entity_is_enemy( lua_State *state )
{
    C_BaseEntity *entity{ resolve( state, check_entity( state ) ) };
    lua_pushboolean( state, g_cl.m_local && entity->m_iTeamNum( ) != g_cl.m_local->m_iTeamNum( ) );
    return 1;
}

static int entity_is_local( lua_State *state )
{
    lua_pushboolean( state, resolve( state, check_entity( state ) ) == g_cl.m_local );
    return 1;
}

static int entity_health( lua_State *state )
{
    const entity_ref_t &reference{ check_entity( state ) };
    C_BaseEntity       *entity{ resolve( state, reference ) };

    switch ( reference.m_kind )
    {
        case entity_kind_t::player: lua_pushinteger( state, entity->As< C_TFPlayer * >( )->m_iHealth( ) ); return 1;
        case entity_kind_t::building: lua_pushinteger( state, entity->As< C_BaseObject * >( )->m_iHealth( ) ); return 1;
        default: return luaL_argerror( state, 1, ASTRAL_XORSTR( "player or building expected" ).get( ) );
    }
}

static int entity_max_health( lua_State *state )
{
    const entity_ref_t &reference{ check_entity( state ) };
    C_BaseEntity       *entity{ resolve( state, reference ) };

    switch ( reference.m_kind )
    {
        case entity_kind_t::player:
        {
            lua_pushinteger( state, g_cl.m_resource ? g_cl.m_resource->m_iMaxHealth( )[ entity->entindex( ) ] : 0 );
            return 1;
        }

        case entity_kind_t::building: lua_pushinteger( state, entity->As< C_BaseObject * >( )->m_iMaxHealth( ) ); return 1;
        default: return luaL_argerror( state, 1, ASTRAL_XORSTR( "player or building expected" ).get( ) );
    }
}

static int player_alive( lua_State *state )
{
    lua_pushboolean( state, check_player( state )->IsAlive( ) );
    return 1;
}

static int player_class( lua_State *state )
{
    lua_pushinteger( state, check_player( state )->m_iClass( ) );
    return 1;
}

static int player_name( lua_State *state )
{
    C_TFPlayer   *player{ check_player( state ) };
    player_info_t info{ };

    if ( !g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) )
    {
        lua_pushnil( state );
        return 1;
    }

    info.name[ MAX_PLAYER_NAME_LENGTH - 1 ] = '\0';
    lua_pushstring( state, info.name );
    return 1;
}

static int player_info_value( lua_State *state, i32 field )
{
    C_TFPlayer *player{ check_player( state ) };
    player_info_t info{ };
    if ( !g_tf2.m_engine->GetPlayerInfo( player->entindex( ), &info ) )
    {
        lua_pushnil( state );
        return 1;
    }
    switch ( field )
    {
        case 0: lua_pushinteger( state, info.userID ); break;
        case 1: lua_pushinteger( state, info.friendsID ); break;
        case 2: lua_pushboolean( state, info.fakeplayer ); break;
        case 3: lua_pushboolean( state, g_cl.is_friend( info.friendsID ) ); break;
        case 4: lua_pushinteger( state, g_cl.party_index( info.friendsID ) ); break;
        default: lua_pushboolean( state, g_cl.is_astral_user( info.friendsID ) ); break;
    }
    return 1;
}

static int player_user_id( lua_State *state ) { return player_info_value( state, 0 ); }
static int player_account_id( lua_State *state ) { return player_info_value( state, 1 ); }
static int player_is_bot( lua_State *state ) { return player_info_value( state, 2 ); }
static int player_is_friend( lua_State *state ) { return player_info_value( state, 3 ); }
static int player_party( lua_State *state ) { return player_info_value( state, 4 ); }
static int player_is_astral_user( lua_State *state ) { return player_info_value( state, 5 ); }

static int player_ping( lua_State *state )
{
    const i32 index{ check_player( state )->entindex( ) };
    lua_pushinteger( state, g_cl.m_resource && index > 0 && index < MAX_TRACKED_PLAYERS
        ? g_cl.m_resource->m_iPing( )[ index ] : 0 );
    return 1;
}

static int player_eye_position( lua_State *state )
{
    C_TFPlayer *player{ check_player( state ) };
    script::push_vec3( state, player->GetAbsOrigin( ) + player->m_vecViewOffset( ) );
    return 1;
}

static int player_eye_angles( lua_State *state )
{
    script::push_vec3( state, check_player( state )->m_angEyeAngles( ) );
    return 1;
}

static int player_velocity( lua_State *state )
{
    script::push_vec3( state, check_player( state )->m_vecVelocity( ) );
    return 1;
}

static int player_flags( lua_State *state )
{
    lua_pushinteger( state, check_player( state )->m_fFlags( ) );
    return 1;
}

static int player_in_cond( lua_State *state )
{
    C_TFPlayer       *player{ check_player( state ) };
    const lua_Integer condition{ luaL_checkinteger( state, 2 ) };

    lua_pushboolean( state, player->InCond( static_cast< ETFCond >( condition ) ) );
    return 1;
}

static int player_active_weapon( lua_State *state )
{
    C_BaseEntity *weapon{ check_player( state )->m_hActiveWeapon( ).Get( ) };
    push_cached( state, weapon, entity_kind_t::weapon );
    return 1;
}

static int weapon_id( lua_State *state )
{
    lua_pushinteger( state, check_weapon( state )->GetWeaponID( ) );
    return 1;
}

static int weapon_item_index( lua_State *state )
{
    lua_pushinteger( state, check_weapon( state )->m_iItemDefinitionIndex( ) );
    return 1;
}

static int weapon_clip( lua_State *state )
{
    lua_pushinteger( state, check_weapon( state )->m_iClip1( ) );
    return 1;
}

static int weapon_name( lua_State *state )
{
    const char *name{ check_weapon( state )->GetWeaponName( ) };
    if ( name ) lua_pushstring( state, name );
    else lua_pushnil( state );
    return 1;
}

static int weapon_slot( lua_State *state )
{
    lua_pushinteger( state, check_weapon( state )->GetSlot( ) );
    return 1;
}

static int weapon_ammo_type( lua_State *state )
{
    lua_pushinteger( state, check_weapon( state )->m_iPrimaryAmmoType( ) );
    return 1;
}

static int weapon_next_attack( lua_State *state )
{
    C_TFWeaponBase *weapon{ check_weapon( state ) };
    lua_pushnumber( state, weapon->m_flNextPrimaryAttack( ) );
    lua_pushnumber( state, weapon->m_flNextSecondaryAttack( ) );
    return 2;
}

static int weapon_energy( lua_State *state )
{
    lua_pushnumber( state, check_weapon( state )->m_flEnergy( ) );
    return 1;
}

static int weapon_reloading( lua_State *state )
{
    lua_pushboolean( state, check_weapon( state )->m_bInReload( ) );
    return 1;
}

static C_BaseObject *check_building( lua_State *state )
{
    return check_kind( state, entity_kind_t::building,
                       ASTRAL_XORSTR( "building expected" ).get( ) )->As< C_BaseObject * >( );
}

static int building_level( lua_State *state )
{
    lua_pushinteger( state, check_building( state )->m_iUpgradeLevel( ) );
    return 1;
}

static int building_mode( lua_State *state )
{
    lua_pushinteger( state, check_building( state )->m_iObjectMode( ) );
    return 1;
}

static int building_sapped( lua_State *state )
{
    lua_pushboolean( state, check_building( state )->m_bHasSapper( ) );
    return 1;
}

static int building_disabled( lua_State *state )
{
    lua_pushboolean( state, check_building( state )->m_bDisabled( ) );
    return 1;
}

static int building_constructing( lua_State *state )
{
    lua_pushboolean( state, check_building( state )->m_bBuilding( ) );
    return 1;
}

ASTRAL_FORCE_INLINE const RecvTable *base_table( const RecvTable *table )
{
    // BEGIN_RECV_TABLE emits "baseclass" as the first prop of every table with a networked parent
    const bool derived{ table->m_nProps > 0 && HASH_RT( table->m_pProps[ 0 ].m_pVarName ) == HASH_CT( "baseclass" ) };
    return derived ? table->m_pProps[ 0 ].m_pDataTable : nullptr;
}

ASTRAL_FORCE_INLINE entity_kind_t kind_of( const entity_cache::entry_t &entry )
{
    switch ( entry.m_class_id )
    {
        case ETFClassID::CTFPlayer: return entity_kind_t::player;
        case ETFClassID::CObjectSentrygun:
        case ETFClassID::CObjectDispenser:
        case ETFClassID::CObjectTeleporter: return entity_kind_t::building;
        default: break;
    }

    for ( const RecvTable *table{ entry.m_client_class->m_pRecvTable }; table; table = base_table( table ) )
    {
        switch ( HASH_RT( table->m_pNetTableName ) )
        {
            case HASH_CT( "DT_BaseCombatWeapon" ): return entity_kind_t::weapon;
            case HASH_CT( "DT_BaseProjectile" ): return entity_kind_t::projectile;
            default: break;
        }
    }

    return entity_kind_t::other;
}

ASTRAL_FORCE_INLINE void push_classified( lua_State *state, C_BaseEntity *entity )
{
    const entity_cache::entry_t *entry{ entity ? g_entity_cache.find( entity ) : nullptr };

    if ( !entry )
    {
        lua_pushnil( state );
        return;
    }

    push_entity( state, entity_ref_t{ entry->m_handle, entry->m_class_id, kind_of( *entry ) } );
}

ASTRAL_FORCE_INLINE void push_handle( lua_State *state, u32 handle )
{
    push_classified( state, g_entity_cache.resolve( handle ) );
}

// dump_recv_table skips "baseclass" and flattens every other sub-table into the table being dumped, so a prop
// is keyed under the class table that declares it, somewhere up the chain
ASTRAL_FORCE_INLINE const netvars::entry *find_in_chain( const RecvTable *table, hash_t prop )
{
    for ( ; table; table = base_table( table ) )
    {
        if ( const netvars::entry *hit{ g_netvars.get_entry( HASH_RT( table->m_pNetTableName ), prop ) } )
        {
            return hit;
        }
    }

    return nullptr;
}

// The leading names of a path ( "localdata", "m_iAmmo" ) are sub-tables the dump already flattened, so they only
// have to exist as tables; the last one is the prop
ASTRAL_FORCE_INLINE const netvars::entry *find_prop( lua_State *state, const RecvTable *table, int first, int last )
{
    const netvars::entry *hit{ };

    for ( int index{ first }; index <= last; ++index )
    {
        hit = find_in_chain( table, HASH_RT( lua_tostring( state, index ) ) );

        if ( !hit || ( index < last && hit->m_prop->m_RecvType != DPT_DataTable ) )
        {
            return nullptr;
        }
    }

    return hit;
}

ASTRAL_FORCE_INLINE const netvars::entry &check_prop( lua_State *state, C_BaseEntity *entity, int first )
{
    const RecvTable *table{ entity->GetClientClass( )->m_pRecvTable };
    const char      *name{ luaL_checkstring( state, first ) };
    const int        last{ lua_gettop( state ) };
    hash_t           path{ };

    for ( int index{ first }; index <= last; ++index )
    {
        name = luaL_checkstring( state, index );
        path = ( path << 5 | path >> 27 ) ^ HASH_RT( name );
    }

    u32           slot{ ( path ^ static_cast< u32 >( reinterpret_cast< uptr >( table ) >> 4 ) ) & PROP_CACHE_MASK };
    prop_cache_t *empty{ };

    for ( usize step{ }; step < PROP_CACHE_CAPACITY; ++step, slot = ( slot + 1 ) & PROP_CACHE_MASK )
    {
        prop_cache_t &cached{ s_props[ slot ] };

        if ( !cached.m_entry )
        {
            empty = &cached;
            break;
        }

        if ( cached.m_table == table && cached.m_path == path && str::equals( cached.m_entry->m_prop->m_pVarName, name ) )
        {
            return *cached.m_entry;
        }
    }

    const netvars::entry *hit{ find_prop( state, table, first, last ) };

    if ( !hit )
    {
        luaL_error( state, ASTRAL_XORSTR( "'%s' is not a prop of %s" ).get( ), name, table->m_pNetTableName );
    }

    if ( empty )
    {
        *empty = prop_cache_t{ table, path, hit };
    }

    return *hit;
}

ASTRAL_FORCE_INLINE u8 *prop_field( C_BaseEntity *entity, i32 offset )
{
    return reinterpret_cast< u8 * >( entity ) + offset;
}

ASTRAL_FORCE_INLINE void check_recv_type( lua_State *state, const RecvProp &prop, bool matches, const char *expected )
{
    if ( !matches )
    {
        luaL_error( state, ASTRAL_XORSTR( "'%s' is not %s" ).get( ), prop.m_pVarName, expected );
    }
}

ASTRAL_FORCE_INLINE lua_Integer read_int( lua_State *state, const RecvProp &prop, netvars::int_kind kind, const u8 *field )
{
    switch ( kind )
    {
        case netvars::int_kind::int8: return *field;
        case netvars::int_kind::int16: return *reinterpret_cast< const u16 * >( field );
        case netvars::int_kind::int32:
        case netvars::int_kind::handle: return *reinterpret_cast< const i32 * >( field );
        default: return luaL_error( state, ASTRAL_XORSTR( "'%s' is not a plain int prop" ).get( ), prop.m_pVarName );
    }
}

ASTRAL_FORCE_INLINE void write_int( lua_State *state, const RecvProp &prop, netvars::int_kind kind, u8 *field, lua_Integer value )
{
    switch ( kind )
    {
        case netvars::int_kind::int8:
        {
            luaL_argcheck( state, value >= 0 && value <= 0xFF, 2, ASTRAL_XORSTR( "value out of range" ).get( ) );
            *field = static_cast< u8 >( value );
            return;
        }

        case netvars::int_kind::int16:
        {
            luaL_argcheck( state, value >= 0 && value <= 0xFFFF, 2, ASTRAL_XORSTR( "value out of range" ).get( ) );
            *reinterpret_cast< u16 * >( field ) = static_cast< u16 >( value );
            return;
        }

        case netvars::int_kind::int32:
        case netvars::int_kind::handle:
        {
            luaL_argcheck( state, value >= -0x80000000LL && value <= 0xFFFFFFFFLL, 2, ASTRAL_XORSTR( "value out of range" ).get( ) );
            *reinterpret_cast< u32 * >( field ) = static_cast< u32 >( value );
            return;
        }

        default: luaL_error( state, ASTRAL_XORSTR( "'%s' is not a plain int prop" ).get( ), prop.m_pVarName ); return;
    }
}

ASTRAL_FORCE_INLINE u32 read_handle( lua_State *state, const RecvProp &prop, netvars::int_kind kind, const u8 *field )
{
    check_recv_type( state, prop, kind == netvars::int_kind::handle, ASTRAL_XORSTR( "an entity handle" ).get( ) );
    return *reinterpret_cast< const u32 * >( field );
}

ASTRAL_FORCE_INLINE float read_float( lua_State *state, const RecvProp &prop, const u8 *field )
{
    check_recv_type( state, prop, prop.m_RecvType == DPT_Float, ASTRAL_XORSTR( "a float prop" ).get( ) );
    return *reinterpret_cast< const float * >( field );
}

static int entity_get_prop_int( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };

    lua_pushinteger( state, read_int( state, *prop.m_prop, prop.m_int, prop_field( entity, prop.m_offset ) ) );
    return 1;
}

static int entity_get_prop_float( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };

    lua_pushnumber( state, read_float( state, *prop.m_prop, prop_field( entity, prop.m_offset ) ) );
    return 1;
}

static int entity_get_prop_bool( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };

    lua_pushboolean( state, read_int( state, *prop.m_prop, prop.m_int, prop_field( entity, prop.m_offset ) ) != 0 );
    return 1;
}

static int entity_get_prop_entity( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };

    push_handle( state, read_handle( state, *prop.m_prop, prop.m_int, prop_field( entity, prop.m_offset ) ) );
    return 1;
}

static int entity_get_prop_vector( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };
    const SendPropType    type{ prop.m_prop->m_RecvType };

    check_recv_type( state, *prop.m_prop, type == DPT_Vector || type == DPT_VectorXY, ASTRAL_XORSTR( "a vector prop" ).get( ) );
    script::push_vec3( state, *reinterpret_cast< const vec3 * >( prop_field( entity, prop.m_offset ) ) );
    return 1;
}

static int entity_get_prop_string( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };

    check_recv_type( state, *prop.m_prop, prop.m_prop->m_RecvType == DPT_String, ASTRAL_XORSTR( "a string prop" ).get( ) );

    const char *text{ reinterpret_cast< const char * >( prop_field( entity, prop.m_offset ) ) };
    usize       size{ };

    while ( size < static_cast< usize >( prop.m_prop->m_StringBufferSize ) && text[ size ] )
    {
        ++size;
    }

    lua_pushlstring( state, text, size );
    return 1;
}

template< typename push_type >
ASTRAL_FORCE_INLINE int push_table_prop( lua_State *state, push_type &&push )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 2 ) };

    check_recv_type( state, *prop.m_prop, prop.m_prop->m_RecvType == DPT_DataTable, ASTRAL_XORSTR( "a data table" ).get( ) );

    const RecvTable &table{ *prop.m_prop->m_pDataTable };
    lua_Integer      slot{ };

    lua_createtable( state, table.m_nProps, 0 );

    for ( i32 index{ }; index < table.m_nProps; ++index )
    {
        const RecvProp &element{ table.m_pProps[ index ] };

        if ( element.m_RecvType == DPT_DataTable )
        {
            continue;
        }

        push( element, prop_field( entity, prop.m_offset + element.m_Offset ) );
        lua_rawseti( state, -2, ++slot );
    }

    return 1;
}

static int entity_get_prop_table_int( lua_State *state )
{
    return push_table_prop( state, [ state ]( const RecvProp &element, const u8 *field )
    {
        lua_pushinteger( state, read_int( state, element, g_netvars.classify( &element ), field ) );
    } );
}

static int entity_get_prop_table_float( lua_State *state )
{
    return push_table_prop( state, [ state ]( const RecvProp &element, const u8 *field )
    {
        lua_pushnumber( state, read_float( state, element, field ) );
    } );
}

static int entity_get_prop_table_bool( lua_State *state )
{
    return push_table_prop( state, [ state ]( const RecvProp &element, const u8 *field )
    {
        lua_pushboolean( state, read_int( state, element, g_netvars.classify( &element ), field ) != 0 );
    } );
}

static int entity_get_prop_table_entity( lua_State *state )
{
    return push_table_prop( state, [ state ]( const RecvProp &element, const u8 *field )
    {
        push_handle( state, read_handle( state, element, g_netvars.classify( &element ), field ) );
    } );
}

static int entity_set_prop_int( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const lua_Integer     value{ luaL_checkinteger( state, 2 ) };
    const netvars::entry &prop{ check_prop( state, entity, 3 ) };

    write_int( state, *prop.m_prop, prop.m_int, prop_field( entity, prop.m_offset ), value );
    return 0;
}

static int entity_set_prop_bool( lua_State *state )
{
    C_BaseEntity *entity{ resolve( state, check_entity( state ) ) };
    luaL_checktype( state, 2, LUA_TBOOLEAN );

    const bool            value{ lua_toboolean( state, 2 ) != 0 };
    const netvars::entry &prop{ check_prop( state, entity, 3 ) };

    write_int( state, *prop.m_prop, prop.m_int, prop_field( entity, prop.m_offset ), value ? 1 : 0 );
    return 0;
}

static int entity_set_prop_float( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const float           value{ static_cast< float >( luaL_checknumber( state, 2 ) ) };
    const netvars::entry &prop{ check_prop( state, entity, 3 ) };

    luaL_argcheck( state, value - value == 0, 2, ASTRAL_XORSTR( "number must be finite" ).get( ) );
    check_recv_type( state, *prop.m_prop, prop.m_prop->m_RecvType == DPT_Float, ASTRAL_XORSTR( "a float prop" ).get( ) );
    *reinterpret_cast< float * >( prop_field( entity, prop.m_offset ) ) = value;
    return 0;
}

static int entity_set_prop_entity( lua_State *state )
{
    C_BaseEntity *entity{ resolve( state, check_entity( state ) ) };
    const u32     handle{ lua_isnil( state, 2 ) ? entity_cache::INVALID_HANDLE
                                                : static_cast< entity_ref_t * >( script::check_object( state, 2, &s_entity_key, ASTRAL_XORSTR( "entity" ).get( ) ) )->m_handle };

    const netvars::entry &prop{ check_prop( state, entity, 3 ) };

    check_recv_type( state, *prop.m_prop, prop.m_int == netvars::int_kind::handle, ASTRAL_XORSTR( "an entity handle" ).get( ) );
    *reinterpret_cast< u32 * >( prop_field( entity, prop.m_offset ) ) = handle;
    return 0;
}

static int entity_set_prop_vector( lua_State *state )
{
    C_BaseEntity         *entity{ resolve( state, check_entity( state ) ) };
    const vec3            value{ *script::check_vec3( state, 2 ) };
    const netvars::entry &prop{ check_prop( state, entity, 3 ) };
    const SendPropType    type{ prop.m_prop->m_RecvType };

    check_recv_type( state, *prop.m_prop, type == DPT_Vector || type == DPT_VectorXY, ASTRAL_XORSTR( "a vector prop" ).get( ) );
    *reinterpret_cast< vec3 * >( prop_field( entity, prop.m_offset ) ) = value;
    return 0;
}

static int entity_attribute_hook_float( lua_State *state )
{
    C_BaseEntity    *entity{ resolve( state, check_entity( state ) ) };
    const char      *name{ luaL_checkstring( state, 2 ) };
    const lua_Number value{ luaL_optnumber( state, 3, 0.0 ) };

    lua_pushnumber( state, g_cl.attrib_hook( entity, static_cast< float >( value ), name ) );
    return 1;
}

static int player_entity_for_loadout_slot( lua_State *state )
{
    C_TFPlayer       *player{ check_player( state ) };
    const lua_Integer slot{ luaL_checkinteger( state, 2 ) };

    push_classified( state, g_tf2.m_C_TFPlayer_GetEntityForLoadoutSlot.call_as< C_BaseEntity * >( player, static_cast< i32 >( slot ), true ) );
    return 1;
}

static int entity_equal( lua_State *state )
{
    const entity_ref_t &left{ *static_cast< entity_ref_t * >( script::check_object( state, 1, &s_entity_key, ASTRAL_XORSTR( "entity" ).get( ) ) ) };
    const entity_ref_t &right{ *static_cast< entity_ref_t * >( script::check_object( state, 2, &s_entity_key, ASTRAL_XORSTR( "entity" ).get( ) ) ) };

    lua_pushboolean( state, left.m_handle == right.m_handle );
    return 1;
}

ASTRAL_FORCE_INLINE int push_group( lua_State *state, entity_cache::view_t view, entity_kind_t kind )
{
    lua_createtable( state, static_cast< int >( view.size( ) ), 0 );
    lua_Integer slot{ };

    for ( const entity_cache::entry_t *entry : view )
    {
        push_entity( state, entity_ref_t{ entry->m_handle, entry->m_class_id, kind } );
        lua_rawseti( state, -2, ++slot );
    }

    return 1;
}

static int entities_players( lua_State *state )
{
    return push_group( state, g_entity_cache.players( ), entity_kind_t::player );
}

static int entities_buildings( lua_State *state )
{
    return push_group( state, g_entity_cache.buildings( ), entity_kind_t::building );
}

static int entities_projectiles( lua_State *state )
{
    return push_group( state, g_entity_cache.projectiles( ), entity_kind_t::projectile );
}

static int entities_local_player( lua_State *state )
{
    push_cached( state, g_cl.m_local, entity_kind_t::player );
    return 1;
}

static int entities_all( lua_State *state )
{
    const entity_cache::view_t view{ g_entity_cache.all( ) };
    lua_createtable( state, static_cast< int >( view.size( ) ), 0 );
    lua_Integer slot{ };
    for ( const entity_cache::entry_t *entry : view )
    {
        push_entity( state, entity_ref_t{ entry->m_handle, entry->m_class_id, kind_of( *entry ) } );
        lua_rawseti( state, -2, ++slot );
    }
    return 1;
}

static int entities_networked( lua_State *state )
{
    const entity_cache::view_t view{ g_entity_cache.networked( ) };
    lua_createtable( state, static_cast< int >( view.size( ) ), 0 );
    lua_Integer slot{ };
    for ( const entity_cache::entry_t *entry : view )
    {
        push_entity( state, entity_ref_t{ entry->m_handle, entry->m_class_id, kind_of( *entry ) } );
        lua_rawseti( state, -2, ++slot );
    }
    return 1;
}

static int entities_by_index( lua_State *state )
{
    const lua_Integer wanted{ luaL_checkinteger( state, 1 ) };
    for ( const entity_cache::entry_t *entry : g_entity_cache.all( ) )
    {
        if ( entry->m_index == wanted )
        {
            push_entity( state, entity_ref_t{ entry->m_handle, entry->m_class_id, kind_of( *entry ) } );
            return 1;
        }
    }
    lua_pushnil( state );
    return 1;
}

static int engine_in_game( lua_State *state )
{
    lua_pushboolean( state, g_tf2.m_engine->IsInGame( ) );
    return 1;
}

static int engine_connected( lua_State *state )
{
    lua_pushboolean( state, g_tf2.m_engine->IsConnected( ) );
    return 1;
}

static int engine_playing_demo( lua_State *state )
{
    lua_pushboolean( state, g_tf2.m_engine->IsPlayingDemo( ) );
    return 1;
}

static int engine_taking_screenshot( lua_State *state )
{
    lua_pushboolean( state, g_tf2.m_engine->IsTakingScreenshot( ) );
    return 1;
}

static int engine_view_angles( lua_State *state )
{
    vec3 angles{ };
    g_tf2.m_engine->GetViewAngles( angles );
    script::push_vec3( state, angles );
    return 1;
}

static int engine_set_view_angles( lua_State *state )
{
    vec3 angles{ *script::check_vec3( state, 1 ) };
    angles.x = math::clamp( angles.x, -89.0f, 89.0f );
    while ( angles.y > 180.0f ) angles.y -= 360.0f;
    while ( angles.y < -180.0f ) angles.y += 360.0f;
    angles.z = 0.0f;
    g_tf2.m_engine->SetViewAngles( angles );
    return 0;
}

static int engine_player_for_user_id( lua_State *state )
{
    lua_pushinteger( state, g_tf2.m_engine->GetPlayerForUserID(
        static_cast< int >( luaL_checkinteger( state, 1 ) ) ) );
    return 1;
}

static int engine_local_index( lua_State *state )
{
    lua_pushinteger( state, g_tf2.m_engine->GetLocalPlayer( ) );
    return 1;
}

static int engine_max_clients( lua_State *state )
{
    lua_pushinteger( state, g_tf2.m_engine->GetMaxClients( ) );
    return 1;
}

static int engine_screen_size( lua_State *state )
{
    lua_pushinteger( state, g_cl.m_screen_width.load( ) );
    lua_pushinteger( state, g_cl.m_screen_height.load( ) );
    return 2;
}

static int engine_cur_time( lua_State *state )
{
    lua_pushnumber( state, g_tf2.m_globals->curtime );
    return 1;
}

static int engine_real_time( lua_State *state )
{
    lua_pushnumber( state, g_tf2.m_globals->realtime );
    return 1;
}

static int engine_frame_time( lua_State *state )
{
    lua_pushnumber( state, g_tf2.m_globals->absoluteframetime );
    return 1;
}

static int engine_tick_count( lua_State *state )
{
    lua_pushinteger( state, g_tf2.m_globals->tickcount );
    return 1;
}

static int engine_interval( lua_State *state )
{
    lua_pushnumber( state, g_tf2.m_globals->interval_per_tick );
    return 1;
}

static int engine_latency( lua_State *state )
{
    CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
    lua_pushnumber( state, channel ? channel->GetLatency( FLOW_OUTGOING ) : 0.0f );
    return 1;
}

static int engine_command( lua_State *state )
{
    g_tf2.m_engine->ClientCmd_Unrestricted( luaL_checkstring( state, 1 ) );
    return 0;
}

static int draw_world_to_screen( lua_State *state )
{
    vec2 screen{ };

    if ( !g_cl.world_to_screen( *script::check_vec3( state, 1 ), screen ) )
    {
        lua_pushnil( state );
        return 1;
    }

    lua_pushnumber( state, screen.x );
    lua_pushnumber( state, screen.y );
    return 2;
}

ASTRAL_FORCE_INLINE void runtime_integer( lua_State *state, const char *name, lua_Integer value )
{
    lua_pushinteger( state, value );
    lua_setfield( state, -2, name );
}

ASTRAL_FORCE_INLINE void runtime_number( lua_State *state, const char *name, lua_Number value )
{
    lua_pushnumber( state, value );
    lua_setfield( state, -2, name );
}

ASTRAL_FORCE_INLINE void runtime_boolean( lua_State *state, const char *name, bool value )
{
    lua_pushboolean( state, value );
    lua_setfield( state, -2, name );
}

ASTRAL_FORCE_INLINE void runtime_string( lua_State *state, const char *name, const char *value )
{
    lua_pushstring( state, value );
    lua_setfield( state, -2, name );
}

static const char *tick_status_name( tick_status_t status )
{
    switch ( status )
    {
        case tick_status_t::unable: return "unable";
        case tick_status_t::not_enough: return "not_enough";
        case tick_status_t::cooldown: return "cooldown";
        case tick_status_t::charging: return "charging";
        case tick_status_t::waiting: return "waiting";
        case tick_status_t::ready: return "ready";
        default: return "unknown";
    }
}

static const char *shift_type_name( shift_type_t type )
{
    switch ( type )
    {
        case shift_type_t::warp: return "warp";
        case shift_type_t::rapidfire: return "rapidfire";
        case shift_type_t::speedhack: return "speedhack";
        default: return "none";
    }
}

static int runtime_tickbase( lua_State *state )
{
    lua_newtable( state );
    runtime_integer( state, "charge", g_tickbase.credits( ) );
    runtime_integer( state, "max_charge", g_tickbase.shift_limit( ) );
    runtime_integer( state, "shown_max_charge", g_tickbase.shown_shift_limit( ) );
    runtime_integer( state, "shift_room", g_tickbase.shift_room( ) );
    runtime_integer( state, "pending_copies", g_tickbase.pending_copies( ) );
    runtime_integer( state, "consuming", g_tickbase.consuming( ) );
    runtime_string( state, "status", tick_status_name( g_tickbase.current_status( ) ) );
    runtime_string( state, "shift_type", shift_type_name( g_tickbase.shift_type( ) ) );
    runtime_boolean( state, "shifting", g_tickbase.shifting( ) );
    runtime_boolean( state, "charging", g_tickbase.recharging( ) );
    runtime_boolean( state, "ready", g_tickbase.has_ticks_stored( ) );
    runtime_boolean( state, "holding_packet", g_tickbase.holds_packet( ) );
    runtime_boolean( state, "withholding_shot", g_tickbase.withholds_shot( ) );
    runtime_boolean( state, "at_budget_cap", g_tickbase.at_budget_cap( ) );
    return 1;
}

static const char *crit_status_name( crits::crit_status status )
{
    switch ( status )
    {
        case crits::crit_status::none: return "none";
        case crits::crit_status::ready: return "ready";
        case crits::crit_status::low_bucket: return "low_bucket";
        case crits::crit_status::streaming_window: return "streaming_window";
        case crits::crit_status::streaming_wait: return "streaming_wait";
        case crits::crit_status::crit_boosted: return "crit_boosted";
        case crits::crit_status::observed_limit: return "observed_limit";
        case crits::crit_status::seed_conflict: return "seed_conflict";
        case crits::crit_status::too_expensive: return "too_expensive";
        default: return "unknown";
    }
}

static int runtime_crits( lua_State *state )
{
    const crits::indicator_view_t indicator{ g_crits.indicator( ) };
    const crit_totals_t totals{ g_crits.totals( ) };
    lua_newtable( state );
    runtime_string( state, "status", crit_status_name( indicator.m_status ) );
    runtime_number( state, "fill", indicator.m_fill );
    runtime_integer( state, "available", indicator.m_available );
    runtime_integer( state, "potential", indicator.m_potential );
    runtime_integer( state, "ban_damage", indicator.m_ban_damage );
    runtime_number( state, "window", indicator.m_window );
    runtime_boolean( state, "resynced", indicator.m_resynced );
    runtime_integer( state, "ranged_damage", totals.m_ranged );
    runtime_integer( state, "crit_damage", totals.m_crit );
    runtime_integer( state, "melee_damage", totals.m_melee );
    runtime_integer( state, "total_damage", totals.total( ) );
    runtime_number( state, "observed", totals.observed( ) );
    return 1;
}

static const char *seed_phase_name( seed_phase_t phase )
{
    switch ( phase )
    {
        case seed_phase_t::empty: return "empty";
        case seed_phase_t::acquiring: return "acquiring";
        case seed_phase_t::tracking: return "tracking";
        case seed_phase_t::uncertain: return "uncertain";
        default: return "unknown";
    }
}

static int runtime_seed_prediction( lua_State *state )
{
    const i32 delay{ static_cast< i32 >( luaL_optinteger(
        state, 1, g_networking.send_delay( ).ticks( ) ) ) };
    lua_newtable( state );
    runtime_string( state, "phase", seed_phase_name( g_nospread.phase( ) ) );
    runtime_integer( state, "seed", g_nospread.seed( ) );
    runtime_boolean( state, "synced", g_nospread.synced( ) );
    runtime_boolean( state, "pending", g_nospread.pending( ) );
    runtime_number( state, "offset", g_nospread.offset( ) );
    runtime_number( state, "uncertainty", g_nospread.uncertainty( ) );
    runtime_integer( state, "samples", static_cast< lua_Integer >( g_nospread.sample_count( ) ) );
    runtime_integer( state, "rejected", g_nospread.rejected_samples( ) );
    runtime_integer( state, "delay", delay );

    lua_newtable( state );
    if ( g_cl.m_cmd )
    {
        seed_candidate_t candidates[ MAX_CANDIDATES * 2 ]{ };
        const usize count{ g_nospread.candidates( delay, candidates ) };
        for ( usize index{ }; index < count; ++index )
        {
            lua_newtable( state );
            runtime_integer( state, "seed", candidates[ index ].m_seed );
            runtime_number( state, "weight", candidates[ index ].m_weight );
            lua_rawseti( state, -2, static_cast< lua_Integer >( index + 1 ) );
        }
    }
    lua_setfield( state, -2, "candidates" );
    return 1;
}

static int runtime_network( lua_State *state )
{
    const send_delay_t delay{ g_networking.send_delay( ) };
    lua_newtable( state );
    runtime_integer( state, "sequence", g_networking.sequence_number( ) );
    runtime_integer( state, "last_sent_command", g_networking.last_sent_command( ) );
    runtime_integer( state, "delay", delay.ticks( ) );
    runtime_integer( state, "psilent_delay", delay.m_psilent_ticks );
    runtime_integer( state, "ack_delay", delay.m_ack_ticks );
    runtime_integer( state, "rate_delay", delay.m_rate_ticks );
    runtime_boolean( state, "can_send", g_networking.can_send_now( ) );
    runtime_boolean( state, "can_run_move", g_networking.can_run_move( ) );
    return 1;
}

static int runtime_prediction( lua_State *state )
{
    lua_newtable( state );
    runtime_boolean( state, "active", g_prediction.m_is_active );
    runtime_integer( state, "network_command", g_prediction.network_command( ) );
    runtime_integer( state, "network_tickbase", g_prediction.network_tickbase( ) );
    runtime_number( state, "weapon_fire_delay", g_prediction.m_weapon_fire_delay );
    return 1;
}

static const char *spectator_mode_name( i32 mode )
{
    switch ( mode )
    {
        case OBS_MODE_DEATHCAM: return "deathcam";
        case OBS_MODE_FREEZECAM: return "freezecam";
        case OBS_MODE_IN_EYE: return "first_person";
        case OBS_MODE_CHASE: return "third_person";
        default: return nullptr;
    }
}

static int runtime_spectators( lua_State *state )
{
    lua_newtable( state );

    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        return 1;
    }

    const i32 local_index{ g_cl.m_local->entindex( ) };
    const i32 max_clients{ math::smaller( g_tf2.m_engine->GetMaxClients( ), 64 ) };
    lua_Integer result_index{ 1 };

    for ( i32 index{ 1 }; index <= max_clients; ++index )
    {
        if ( index == local_index )
        {
            continue;
        }

        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
        if ( !client || client->IsDormant( ) )
        {
            continue;
        }

        C_BaseEntity *entity{ client->GetBaseEntity( ) };
        if ( !entity )
        {
            continue;
        }

        C_TFPlayer *player{ entity->As< C_TFPlayer * >( ) };
        const i32   mode{ player->m_iObserverMode( ) };
        const char *mode_name{ spectator_mode_name( mode ) };
        if ( !mode_name || player->m_hObserverTarget( ).Get( ) != g_cl.m_local )
        {
            continue;
        }

        player_info_t info{ };
        if ( !g_tf2.m_engine->GetPlayerInfo( index, &info ) || info.ishltv || info.isreplay )
        {
            continue;
        }

        info.name[ MAX_PLAYER_NAME_LENGTH - 1 ] = '\0';
        lua_newtable( state );
        runtime_integer( state, "index", index );
        runtime_integer( state, "user_id", info.userID );
        runtime_integer( state, "account_id", info.friendsID );
        runtime_string( state, "name", info.name );
        runtime_integer( state, "mode", mode );
        runtime_string( state, "mode_name", mode_name );
        runtime_boolean( state, "bot", info.fakeplayer );
        push_cached( state, entity, entity_kind_t::player );
        lua_setfield( state, -2, "entity" );
        lua_rawseti( state, -2, result_index++ );
    }

    return 1;
}

static int runtime_seed_request( lua_State *state )
{
    ASTRAL_UNUSED_PARAM( state );
    g_nospread.request( );
    return 0;
}

static int runtime_clear_tickbase( lua_State *state )
{
    ASTRAL_UNUSED_PARAM( state );
    g_tickbase.clear( );
    return 0;
}

static int runtime_clear_crits( lua_State *state )
{
    ASTRAL_UNUSED_PARAM( state );
    g_crits.clear( );
    return 0;
}

static int runtime_clear_seed( lua_State *state )
{
    ASTRAL_UNUSED_PARAM( state );
    g_nospread.clear( );
    return 0;
}

ASTRAL_FORCE_INLINE ConVar *check_cvar( lua_State *state )
{
    return *static_cast< ConVar ** >( script::check_object( state, 1, &s_cvar_key, ASTRAL_XORSTR( "cvar" ).get( ) ) );
}

static int cvar_find( lua_State *state )
{
    ConVar *variable{ g_tf2.m_cvar->FindVar( luaL_checkstring( state, 1 ) ) };

    if ( !variable )
    {
        lua_pushnil( state );
        return 1;
    }

    *static_cast< ConVar ** >( script::new_object( state, sizeof( ConVar * ), &s_cvar_key ) ) = variable;
    return 1;
}

static int cvar_get_int( lua_State *state )
{
    lua_pushinteger( state, check_cvar( state )->GetInt( ) );
    return 1;
}

static int cvar_get_float( lua_State *state )
{
    lua_pushnumber( state, check_cvar( state )->GetFloat( ) );
    return 1;
}

static int cvar_get_string( lua_State *state )
{
    lua_pushstring( state, check_cvar( state )->GetString( ) );
    return 1;
}

static int cvar_set_int( lua_State *state )
{
    ConVar   *variable{ check_cvar( state ) };
    const int value{ static_cast< int >( luaL_checkinteger( state, 2 ) ) };

    variable->SetValue( value );
    return 0;
}

static int cvar_set_float( lua_State *state )
{
    ConVar     *variable{ check_cvar( state ) };
    const float value{ static_cast< float >( luaL_checknumber( state, 2 ) ) };

    variable->SetValue( value );
    return 0;
}

static int cvar_set_string( lua_State *state )
{
    ConVar     *variable{ check_cvar( state ) };
    const char *value{ luaL_checkstring( state, 2 ) };

    variable->SetValue( value );
    return 0;
}

ASTRAL_FORCE_INLINE string_view pop_segment( string_view &path )
{
    const usize       dot{ path.find( '.' ) };
    const string_view segment{ path.substr( 0, dot ) };

    path = dot == str::npos ? string_view{ } : path.substr( dot + 1 );
    return segment;
}

struct config_access
{
    lua_State  *m_state{ };
    const char *m_path{ };
    string_view m_rest{ };
    string_view m_segment{ };
    bool        m_write{ };
    bool        m_active{ };
    bool        m_matched{ };
    bool        m_in_bind{ };

    void fail( ) const
    {
        luaL_error( this->m_state, ASTRAL_XORSTR( "'%s' is not a config value" ).get( ), this->m_path );
    }

    template< typename value_type >
    void operator( )( const char *name, value_type &value )
    {
        if ( this->m_matched || this->m_segment != string_view{ name } )
        {
            return;
        }

        this->m_matched = true;
        this->visit( value );
    }

    template< typename value_type, usize size >
    void visit( value_type ( &value )[ size ] )
    {
        const string_view segment{ pop_segment( this->m_rest ) };
        usize             index{ };

        if ( segment.empty( ) )
        {
            this->fail( );
        }

        for ( usize i{ }; i < segment.size( ); ++i )
        {
            if ( segment[ i ] < '0' || segment[ i ] > '9' )
            {
                this->fail( );
            }

            index = index * 10 + static_cast< usize >( segment[ i ] - '0' );
            if ( index >= size )
            {
                this->fail( );
            }
        }

        this->visit( value[ index ] );
    }

    template< usize size >
    void visit( char ( &value )[ size ] )
    {
        if ( !this->m_rest.empty( ) ) this->fail( );
        if ( this->m_write )
        {
            usize length{ };
            const char *text{ luaL_checklstring( this->m_state, 2, &length ) };
            luaL_argcheck( this->m_state, length > 0 && length < size, 2,
                           ASTRAL_XORSTR( "string is empty or too long" ).get( ) );
            str::copy( value, size, text, length );
        }
        else
        {
            lua_pushstring( this->m_state, value );
        }
    }

    template< typename value_type >
    void visit( value_type &value )
    {
        if constexpr ( json::detail::is_same< value_type, color_t > || json::detail::is_number< value_type > || __is_enum( value_type ) )
        {
            if ( !this->m_rest.empty( ) )
            {
                this->fail( );
            }

            if ( this->m_write )
            {
                this->write( value );
            } else
            {
                this->read( value );
            }
        } else
        {
            if ( this->m_rest.empty( ) )
            {
                this->fail( );
            }

            this->m_segment = pop_segment( this->m_rest );
            this->m_matched = false;
            this->m_in_bind = json::detail::is_same< value_type, bind_t >;
            config_fields( value, *this );

            if ( !this->m_matched )
            {
                this->fail( );
            }
        }
    }

    template< typename value_type >
    void read( const value_type &value ) const
    {
        if constexpr ( json::detail::is_same< value_type, color_t > )
        {
            lua_pushinteger( this->m_state, static_cast< lua_Integer >( ( static_cast< u32 >( value.r ) << 24 ) | ( static_cast< u32 >( value.g ) << 16 ) |
                                                                        ( static_cast< u32 >( value.b ) << 8 ) | value.a ) );
        } else if constexpr ( json::detail::is_same< value_type, bool > )
        {
            bool active{ value };
            if ( this->m_active && value )
            {
                for ( usize index{ }; index < s_config_binding_count; ++index )
                {
                    const config_binding_t &binding{ s_config_bindings[ index ] };
                    if ( binding.m_enabled != &value )
                    {
                        continue;
                    }

                    // An empty bind leaves optional gates such as thirdperson ungated.
                    // Hold, Toggle and Always otherwise use the same live state as the feature.
                    active = ( !binding.m_bind->m_key && binding.m_bind->m_mode != bind_mode_t::always ) ||
                             gui::bind_active( *binding.m_bind );
                    break;
                }
            }
            lua_pushboolean( this->m_state, active );
        } else if constexpr ( json::detail::is_real< value_type > )
        {
            lua_pushnumber( this->m_state, value );
        } else
        {
            lua_pushinteger( this->m_state, static_cast< lua_Integer >( value ) );
        }
    }

    template< typename value_type >
    void write( value_type &value ) const
    {
        const value_type previous{ value };

        if constexpr ( json::detail::is_same< value_type, color_t > )
        {
            value = script::check_color( this->m_state, 2 );
        } else if constexpr ( json::detail::is_same< value_type, bool > )
        {
            luaL_checktype( this->m_state, 2, LUA_TBOOLEAN );
            value = lua_toboolean( this->m_state, 2 ) != 0;
        } else if constexpr ( json::detail::is_real< value_type > )
        {
            const value_type number{ static_cast< value_type >( luaL_checknumber( this->m_state, 2 ) ) };
            luaL_argcheck( this->m_state, number - number == 0, 2, ASTRAL_XORSTR( "number must be finite" ).get( ) );
            value = number;
        } else if constexpr ( __is_enum( value_type ) )
        {
            const lua_Integer number{ luaL_checkinteger( this->m_state, 2 ) };

            luaL_argcheck( this->m_state, number >= 0 && number <= config_enum_maximum< value_type >, 2, ASTRAL_XORSTR( "value out of range" ).get( ) );
            value = static_cast< value_type >( number );
        } else
        {
            const lua_Integer number{ luaL_checkinteger( this->m_state, 2 ) };

            luaL_argcheck( this->m_state,
                           static_cast< lua_Integer >( static_cast< value_type >( number ) ) == number &&
                               ( number >= 0 || __is_signed( value_type ) ) &&
                               ( !this->m_in_bind || number <= CONFIG_BIND_KEY_MAXIMUM ),
                           2,
                           ASTRAL_XORSTR( "value out of range" ).get( ) );
            value = static_cast< value_type >( number );
        }

        if ( !valid_config( g_cfg ) )
        {
            value = previous;
            luaL_argerror( this->m_state, 2, ASTRAL_XORSTR( "value out of range" ).get( ) );
        }

        if constexpr ( json::detail::is_same< value_type, color_t > )
        {
            if ( &value == &g_cfg.m_misc.m_accent )
            {
                g_theme.set_accent( value );
            }
        }
    }
};

ASTRAL_FORCE_INLINE void access_config( lua_State *state, bool write, bool active = false )
{
    const char   *path{ luaL_checkstring( state, 1 ) };
    config_access access{ state, path, string_view{ path }, { }, write, active };

    access.visit( g_cfg );
}

static int config_get( lua_State *state )
{
    access_config( state, false );
    return 1;
}

static int config_set( lua_State *state )
{
    access_config( state, true );
    return 0;
}

static int config_active( lua_State *state )
{
    access_config( state, false, true );
    return 1;
}

struct config_element_t
{
    gui::baseobject   *m_element{ };
    gui::bound_value_t m_bound{ };
};

struct config_namer
{
    const atl::vector< config_element_t > &m_elements;
    fixed_string< MENU_LABEL_LENGTH >      m_path{ };

    template< typename value_type >
    void operator( )( const char *name, value_type &value )
    {
        const usize length{ this->m_path.size( ) };

        if ( length )
        {
            this->m_path.push_back( '.' );
        }

        const bool appended{ this->m_path.append( string_view{ name } ) };
        Assert( appended );

        this->visit( value );
        this->m_path.truncate( length );
    }

    template< typename value_type, usize size >
    void visit( value_type ( &value )[ size ] )
    {
        const usize length{ this->m_path.size( ) };
        char        segment[ 24 ]{ };

        str::format_to( segment, sizeof( segment ), ASTRAL_XORSTR( ".0-{}" ).get( ), size - 1 );
        this->m_path.append( string_view{ segment } );
        this->name( &value, sizeof( value ) );
        this->m_path.truncate( length );

        for ( usize i{ }; i < size; ++i )
        {
            str::format_to( segment, sizeof( segment ), ASTRAL_XORSTR( ".{}" ).get( ), i );
            this->m_path.append( string_view{ segment } );
            this->visit( value[ i ] );
            this->m_path.truncate( length );
        }
    }

    template< usize size >
    void visit( char ( &value )[ size ] )
    {
        this->name( &value, sizeof( value ) );
    }

    template< typename value_type >
    void visit( value_type &value )
    {
        this->name( &value, sizeof( value ) );

        if constexpr ( !( json::detail::is_same< value_type, color_t > || json::detail::is_number< value_type > || __is_enum( value_type ) ) )
        {
            config_fields( value, *this );
        }
    }

    void name( const void *address, usize size ) const
    {
        for ( const config_element_t &entry : this->m_elements )
        {
            if ( entry.m_bound.m_address == address && entry.m_bound.m_size == size )
            {
                entry.m_element->set_script_name( this->m_path.view( ) );
            }
        }
    }
};

void name_menu_elements( )
{
    atl::vector< config_element_t > elements{ };
    s_config_binding_count = 0;

    for ( const gui::tab *tab : g_gui.tabs( ) )
    {
        for ( usize i{ }; i < tab->subtab_count( ); ++i )
        {
            for ( const gui::group_box *group : tab->subtab( i )->groups( ) )
            {
                for ( gui::baseobject *element : group->elements( ) )
                {
                    const gui::bound_value_t owner{ element->bound( ) };
                    elements.push_back( config_element_t{ element, owner } );

                    for ( gui::baseobject *module : element->modules( ) )
                    {
                        const gui::bound_value_t bound{ module->bound( ) };
                        elements.push_back( config_element_t{ module, bound } );

                        if ( owner.m_address && owner.m_size == sizeof( bool ) &&
                             bound.m_address && bound.m_size == sizeof( bind_t ) &&
                             s_config_binding_count < CONFIG_BINDING_CAPACITY )
                        {
                            s_config_bindings[ s_config_binding_count++ ] = config_binding_t{
                                static_cast< const bool * >( owner.m_address ),
                                const_cast< bind_t * >( static_cast< const bind_t * >( bound.m_address ) )
                            };
                        }
                    }
                }
            }
        }
    }

    config_namer namer{ elements };
    namer.visit( g_cfg );

    // A bind controls its host option, so expose the host's universal config/Lua path
    // (for example misc.thirdperson) instead of the storage detail used for its key.
    for ( const gui::tab *tab : g_gui.tabs( ) )
    {
        for ( usize i{ }; i < tab->subtab_count( ); ++i )
        {
            for ( const gui::group_box *group : tab->subtab( i )->groups( ) )
            {
                for ( gui::baseobject *element : group->elements( ) )
                {
                    const gui::bound_value_t owner{ element->bound( ) };
                    if ( !owner.m_address || owner.m_size != sizeof( bool ) ||
                         element->script_name( ).empty( ) )
                    {
                        continue;
                    }

                    for ( gui::baseobject *module : element->modules( ) )
                    {
                        const gui::bound_value_t bound{ module->bound( ) };
                        if ( bound.m_address && bound.m_size == sizeof( bind_t ) )
                        {
                            module->set_script_name( element->script_name( ) );
                        }
                    }
                }
            }
        }
    }
}

ASTRAL_FORCE_INLINE CUserCmd *current_command( lua_State *state )
{
    CUserCmd *command{ g_script_api.command( ) };

    if ( !command )
    {
        luaL_error( state, ASTRAL_XORSTR( "the command only exists inside create_move" ).get( ) );
    }

    return command;
}

static int command_index( lua_State *state )
{
    CUserCmd   *command{ current_command( state ) };
    const char *key{ luaL_checkstring( state, 2 ) };

    switch ( HASH_RT( key ) )
    {
        case HASH_CT( "command_number" ): lua_pushinteger( state, command->command_number ); return 1;
        case HASH_CT( "tick_count" ): lua_pushinteger( state, command->tick_count ); return 1;
        case HASH_CT( "viewangles" ): script::push_vec3( state, command->viewangles ); return 1;
        case HASH_CT( "forwardmove" ): lua_pushnumber( state, command->forwardmove ); return 1;
        case HASH_CT( "sidemove" ): lua_pushnumber( state, command->sidemove ); return 1;
        case HASH_CT( "upmove" ): lua_pushnumber( state, command->upmove ); return 1;
        case HASH_CT( "buttons" ): lua_pushinteger( state, command->buttons ); return 1;
        case HASH_CT( "impulse" ): lua_pushinteger( state, command->impulse ); return 1;
        case HASH_CT( "weaponselect" ): lua_pushinteger( state, command->weaponselect ); return 1;
        case HASH_CT( "weaponsubtype" ): lua_pushinteger( state, command->weaponsubtype ); return 1;
        case HASH_CT( "random_seed" ): lua_pushinteger( state, command->random_seed ); return 1;
        case HASH_CT( "server_random_seed" ): lua_pushinteger( state, command->server_random_seed ); return 1;
        case HASH_CT( "mousedx" ): lua_pushinteger( state, command->mousedx ); return 1;
        case HASH_CT( "mousedy" ): lua_pushinteger( state, command->mousedy ); return 1;
        case HASH_CT( "hasbeenpredicted" ): lua_pushboolean( state, command->hasbeenpredicted ); return 1;
        default: lua_pushnil( state ); return 1;
    }
}

static int command_newindex( lua_State *state )
{
    CUserCmd   *command{ current_command( state ) };
    const char *key{ luaL_checkstring( state, 2 ) };

    switch ( HASH_RT( key ) )
    {
        case HASH_CT( "tick_count" ): command->tick_count = static_cast< int >( luaL_checkinteger( state, 3 ) ); return 0;
        case HASH_CT( "viewangles" ): command->viewangles = *script::check_vec3( state, 3 ); return 0;
        case HASH_CT( "forwardmove" ): command->forwardmove = static_cast< float >( luaL_checknumber( state, 3 ) ); return 0;
        case HASH_CT( "sidemove" ): command->sidemove = static_cast< float >( luaL_checknumber( state, 3 ) ); return 0;
        case HASH_CT( "upmove" ): command->upmove = static_cast< float >( luaL_checknumber( state, 3 ) ); return 0;
        case HASH_CT( "buttons" ): command->buttons = static_cast< int >( luaL_checkinteger( state, 3 ) ); return 0;
        case HASH_CT( "impulse" ):
        {
            const lua_Integer value{ luaL_checkinteger( state, 3 ) };
            luaL_argcheck( state, value >= 0 && value <= 255, 3, ASTRAL_XORSTR( "impulse out of range" ).get( ) );
            command->impulse = static_cast< u8 >( value );
            return 0;
        }
        case HASH_CT( "weaponselect" ): command->weaponselect = static_cast< int >( luaL_checkinteger( state, 3 ) ); return 0;
        case HASH_CT( "weaponsubtype" ): command->weaponsubtype = static_cast< int >( luaL_checkinteger( state, 3 ) ); return 0;
        case HASH_CT( "random_seed" ): command->random_seed = static_cast< int >( luaL_checkinteger( state, 3 ) ); return 0;
        case HASH_CT( "server_random_seed" ): command->server_random_seed = static_cast< int >( luaL_checkinteger( state, 3 ) ); return 0;
        case HASH_CT( "mousedx" ):
        case HASH_CT( "mousedy" ):
        {
            const lua_Integer value{ luaL_checkinteger( state, 3 ) };
            luaL_argcheck( state, value >= -32768 && value <= 32767, 3,
                           ASTRAL_XORSTR( "mouse delta out of range" ).get( ) );
            if ( HASH_RT( key ) == HASH_CT( "mousedx" ) ) command->mousedx = static_cast< short >( value );
            else command->mousedy = static_cast< short >( value );
            return 0;
        }
        case HASH_CT( "hasbeenpredicted" ):
            luaL_checktype( state, 3, LUA_TBOOLEAN );
            command->hasbeenpredicted = lua_toboolean( state, 3 ) != 0;
            return 0;
        default: return luaL_error( state, ASTRAL_XORSTR( "'%s' cannot be written" ).get( ), key );
    }
}

ASTRAL_FORCE_INLINE IGameEvent *current_event( lua_State *state )
{
    IGameEvent *event{ g_script_api.event( ) };

    if ( !event )
    {
        luaL_error( state, ASTRAL_XORSTR( "the event only exists inside its handler" ).get( ) );
    }

    return event;
}

static int event_name( lua_State *state )
{
    lua_pushstring( state, current_event( state )->GetName( ) );
    return 1;
}

static int event_get_int( lua_State *state )
{
    IGameEvent       *event{ current_event( state ) };
    const char       *key{ luaL_checkstring( state, 2 ) };
    const lua_Integer fallback{ luaL_optinteger( state, 3, 0 ) };

    lua_pushinteger( state, event->GetInt( key, static_cast< int >( fallback ) ) );
    return 1;
}

static int event_get_float( lua_State *state )
{
    IGameEvent      *event{ current_event( state ) };
    const char      *key{ luaL_checkstring( state, 2 ) };
    const lua_Number fallback{ luaL_optnumber( state, 3, 0.0 ) };

    lua_pushnumber( state, event->GetFloat( key, static_cast< float >( fallback ) ) );
    return 1;
}

static int event_get_string( lua_State *state )
{
    IGameEvent *event{ current_event( state ) };
    const char *key{ luaL_checkstring( state, 2 ) };

    lua_pushstring( state, event->GetString( key, "" ) );
    return 1;
}

static int event_get_bool( lua_State *state )
{
    IGameEvent *event{ current_event( state ) };
    const char *key{ luaL_checkstring( state, 2 ) };

    lua_pushboolean( state, event->GetBool( key, false ) );
    return 1;
}

static int events_listen( lua_State *state )
{
    const char *name{ luaL_checkstring( state, 1 ) };
    luaL_checktype( state, 2, LUA_TFUNCTION );

    if ( !g_script_api.listen( name ) )
    {
        return luaL_error( state, ASTRAL_XORSTR( "unknown game event '%s'" ).get( ), name );
    }

    lua_rawgetp( state, LUA_REGISTRYINDEX, &s_listeners_key );

    if ( lua_getfield( state, -1, name ) != LUA_TTABLE )
    {
        lua_pop( state, 1 );
        lua_newtable( state );
        lua_pushvalue( state, -1 );
        lua_setfield( state, -3, name );
    }

    lua_pushvalue( state, 2 );
    lua_rawseti( state, -2, static_cast< lua_Integer >( lua_rawlen( state, -2 ) ) + 1 );
    return 0;
}

static int dispatch_event( lua_State *state )
{
    const char *name{ static_cast< const char * >( lua_touserdata( state, 1 ) ) };

    lua_rawgetp( state, LUA_REGISTRYINDEX, &s_listeners_key );

    if ( lua_getfield( state, -1, name ) != LUA_TTABLE )
    {
        return 0;
    }

    const lua_Integer count{ static_cast< lua_Integer >( lua_rawlen( state, -1 ) ) };

    for ( lua_Integer index{ 1 }; index <= count; ++index )
    {
        lua_rawgeti( state, -1, index );
        lua_rawgetp( state, LUA_REGISTRYINDEX, &s_event_key );
        lua_call( state, 1, 0 );
    }

    return 0;
}

static i32 push_command( lua_State *state )
{
    lua_rawgetp( state, LUA_REGISTRYINDEX, &s_command_key );
    return 1;
}

static i32 push_stage( lua_State *state )
{
    lua_pushinteger( state, g_script_api.stage( ) );
    return 1;
}

ASTRAL_FORCE_INLINE void set_integer( lua_State *state, const char *name, lua_Integer value )
{
    lua_pushinteger( state, value );
    lua_setfield( state, -2, name );
}

void script_api::open( lua_State *state )
{
    using script::set_function;

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "__eq" ).get( ), &entity_equal );
    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "valid" ).get( ), &entity_valid );
    set_function( state, ASTRAL_XORSTR( "index" ).get( ), &entity_index );
    set_function( state, ASTRAL_XORSTR( "handle" ).get( ), &entity_handle );
    set_function( state, ASTRAL_XORSTR( "class_id" ).get( ), &entity_class_id );
    set_function( state, ASTRAL_XORSTR( "kind" ).get( ), &entity_kind );
    set_function( state, ASTRAL_XORSTR( "origin" ).get( ), &entity_origin );
    set_function( state, ASTRAL_XORSTR( "center" ).get( ), &entity_center );
    set_function( state, ASTRAL_XORSTR( "bounds" ).get( ), &entity_bounds );
    set_function( state, ASTRAL_XORSTR( "classname" ).get( ), &entity_classname );
    set_function( state, ASTRAL_XORSTR( "distance" ).get( ), &entity_distance );
    set_function( state, ASTRAL_XORSTR( "team" ).get( ), &entity_team );
    set_function( state, ASTRAL_XORSTR( "dormant" ).get( ), &entity_dormant );
    set_function( state, ASTRAL_XORSTR( "is_enemy" ).get( ), &entity_is_enemy );
    set_function( state, ASTRAL_XORSTR( "is_local" ).get( ), &entity_is_local );
    set_function( state, ASTRAL_XORSTR( "health" ).get( ), &entity_health );
    set_function( state, ASTRAL_XORSTR( "max_health" ).get( ), &entity_max_health );
    set_function( state, ASTRAL_XORSTR( "alive" ).get( ), &player_alive );
    set_function( state, ASTRAL_XORSTR( "class" ).get( ), &player_class );
    set_function( state, ASTRAL_XORSTR( "name" ).get( ), &player_name );
    set_function( state, ASTRAL_XORSTR( "user_id" ).get( ), &player_user_id );
    set_function( state, ASTRAL_XORSTR( "account_id" ).get( ), &player_account_id );
    set_function( state, ASTRAL_XORSTR( "ping" ).get( ), &player_ping );
    set_function( state, ASTRAL_XORSTR( "is_bot" ).get( ), &player_is_bot );
    set_function( state, ASTRAL_XORSTR( "is_friend" ).get( ), &player_is_friend );
    set_function( state, ASTRAL_XORSTR( "party" ).get( ), &player_party );
    set_function( state, ASTRAL_XORSTR( "is_astral_user" ).get( ), &player_is_astral_user );
    set_function( state, ASTRAL_XORSTR( "eye_position" ).get( ), &player_eye_position );
    set_function( state, ASTRAL_XORSTR( "eye_angles" ).get( ), &player_eye_angles );
    set_function( state, ASTRAL_XORSTR( "velocity" ).get( ), &player_velocity );
    set_function( state, ASTRAL_XORSTR( "flags" ).get( ), &player_flags );
    set_function( state, ASTRAL_XORSTR( "in_cond" ).get( ), &player_in_cond );
    set_function( state, ASTRAL_XORSTR( "active_weapon" ).get( ), &player_active_weapon );
    set_function( state, ASTRAL_XORSTR( "weapon_id" ).get( ), &weapon_id );
    set_function( state, ASTRAL_XORSTR( "weapon_name" ).get( ), &weapon_name );
    set_function( state, ASTRAL_XORSTR( "weapon_slot" ).get( ), &weapon_slot );
    set_function( state, ASTRAL_XORSTR( "item_index" ).get( ), &weapon_item_index );
    set_function( state, ASTRAL_XORSTR( "clip" ).get( ), &weapon_clip );
    set_function( state, ASTRAL_XORSTR( "ammo_type" ).get( ), &weapon_ammo_type );
    set_function( state, ASTRAL_XORSTR( "next_attack" ).get( ), &weapon_next_attack );
    set_function( state, ASTRAL_XORSTR( "energy" ).get( ), &weapon_energy );
    set_function( state, ASTRAL_XORSTR( "reloading" ).get( ), &weapon_reloading );
    set_function( state, ASTRAL_XORSTR( "building_level" ).get( ), &building_level );
    set_function( state, ASTRAL_XORSTR( "building_mode" ).get( ), &building_mode );
    set_function( state, ASTRAL_XORSTR( "building_sapped" ).get( ), &building_sapped );
    set_function( state, ASTRAL_XORSTR( "building_disabled" ).get( ), &building_disabled );
    set_function( state, ASTRAL_XORSTR( "building_constructing" ).get( ), &building_constructing );
    set_function( state, ASTRAL_XORSTR( "GetPropInt" ).get( ), &entity_get_prop_int );
    set_function( state, ASTRAL_XORSTR( "GetPropFloat" ).get( ), &entity_get_prop_float );
    set_function( state, ASTRAL_XORSTR( "GetPropBool" ).get( ), &entity_get_prop_bool );
    set_function( state, ASTRAL_XORSTR( "GetPropEntity" ).get( ), &entity_get_prop_entity );
    set_function( state, ASTRAL_XORSTR( "GetPropVector" ).get( ), &entity_get_prop_vector );
    set_function( state, ASTRAL_XORSTR( "GetPropString" ).get( ), &entity_get_prop_string );
    set_function( state, ASTRAL_XORSTR( "GetPropDataTableInt" ).get( ), &entity_get_prop_table_int );
    set_function( state, ASTRAL_XORSTR( "GetPropDataTableFloat" ).get( ), &entity_get_prop_table_float );
    set_function( state, ASTRAL_XORSTR( "GetPropDataTableBool" ).get( ), &entity_get_prop_table_bool );
    set_function( state, ASTRAL_XORSTR( "GetPropDataTableEntity" ).get( ), &entity_get_prop_table_entity );
    set_function( state, ASTRAL_XORSTR( "SetPropInt" ).get( ), &entity_set_prop_int );
    set_function( state, ASTRAL_XORSTR( "SetPropFloat" ).get( ), &entity_set_prop_float );
    set_function( state, ASTRAL_XORSTR( "SetPropBool" ).get( ), &entity_set_prop_bool );
    set_function( state, ASTRAL_XORSTR( "SetPropEntity" ).get( ), &entity_set_prop_entity );
    set_function( state, ASTRAL_XORSTR( "SetPropVector" ).get( ), &entity_set_prop_vector );
    set_function( state, ASTRAL_XORSTR( "AttributeHookFloat" ).get( ), &entity_attribute_hook_float );
    set_function( state, ASTRAL_XORSTR( "GetEntityForLoadoutSlot" ).get( ), &player_entity_for_loadout_slot );
    lua_setfield( state, -2, ASTRAL_XORSTR( "__index" ).get( ) );
    lua_rawsetp( state, LUA_REGISTRYINDEX, &s_entity_key );

    lua_newtable( state );
    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "get_int" ).get( ), &cvar_get_int );
    set_function( state, ASTRAL_XORSTR( "get_float" ).get( ), &cvar_get_float );
    set_function( state, ASTRAL_XORSTR( "get_string" ).get( ), &cvar_get_string );
    set_function( state, ASTRAL_XORSTR( "set_int" ).get( ), &cvar_set_int );
    set_function( state, ASTRAL_XORSTR( "set_float" ).get( ), &cvar_set_float );
    set_function( state, ASTRAL_XORSTR( "set_string" ).get( ), &cvar_set_string );
    lua_setfield( state, -2, ASTRAL_XORSTR( "__index" ).get( ) );
    lua_rawsetp( state, LUA_REGISTRYINDEX, &s_cvar_key );

    lua_newtable( state );
    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "__index" ).get( ), &command_index );
    set_function( state, ASTRAL_XORSTR( "__newindex" ).get( ), &command_newindex );
    lua_setmetatable( state, -2 );
    lua_rawsetp( state, LUA_REGISTRYINDEX, &s_command_key );

    lua_newtable( state );
    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "name" ).get( ), &event_name );
    set_function( state, ASTRAL_XORSTR( "get_int" ).get( ), &event_get_int );
    set_function( state, ASTRAL_XORSTR( "get_float" ).get( ), &event_get_float );
    set_function( state, ASTRAL_XORSTR( "get_string" ).get( ), &event_get_string );
    set_function( state, ASTRAL_XORSTR( "get_bool" ).get( ), &event_get_bool );
    lua_setfield( state, -2, ASTRAL_XORSTR( "__index" ).get( ) );
    lua_setmetatable( state, -2 );
    lua_rawsetp( state, LUA_REGISTRYINDEX, &s_event_key );

    lua_newtable( state );
    lua_rawsetp( state, LUA_REGISTRYINDEX, &s_listeners_key );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "all" ).get( ), &entities_all );
    set_function( state, ASTRAL_XORSTR( "networked" ).get( ), &entities_networked );
    set_function( state, ASTRAL_XORSTR( "by_index" ).get( ), &entities_by_index );
    set_function( state, ASTRAL_XORSTR( "players" ).get( ), &entities_players );
    set_function( state, ASTRAL_XORSTR( "buildings" ).get( ), &entities_buildings );
    set_function( state, ASTRAL_XORSTR( "projectiles" ).get( ), &entities_projectiles );
    set_function( state, ASTRAL_XORSTR( "local_player" ).get( ), &entities_local_player );
    set_function( state, ASTRAL_XORSTR( "GetLocalPlayer" ).get( ), &entities_local_player );
    lua_setglobal( state, ASTRAL_XORSTR( "entities" ).get( ) );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "in_game" ).get( ), &engine_in_game );
    set_function( state, ASTRAL_XORSTR( "connected" ).get( ), &engine_connected );
    set_function( state, ASTRAL_XORSTR( "playing_demo" ).get( ), &engine_playing_demo );
    set_function( state, ASTRAL_XORSTR( "taking_screenshot" ).get( ), &engine_taking_screenshot );
    set_function( state, ASTRAL_XORSTR( "local_index" ).get( ), &engine_local_index );
    set_function( state, ASTRAL_XORSTR( "player_for_user_id" ).get( ), &engine_player_for_user_id );
    set_function( state, ASTRAL_XORSTR( "max_clients" ).get( ), &engine_max_clients );
    set_function( state, ASTRAL_XORSTR( "screen_size" ).get( ), &engine_screen_size );
    set_function( state, ASTRAL_XORSTR( "cur_time" ).get( ), &engine_cur_time );
    set_function( state, ASTRAL_XORSTR( "real_time" ).get( ), &engine_real_time );
    set_function( state, ASTRAL_XORSTR( "frame_time" ).get( ), &engine_frame_time );
    set_function( state, ASTRAL_XORSTR( "tick_count" ).get( ), &engine_tick_count );
    set_function( state, ASTRAL_XORSTR( "interval" ).get( ), &engine_interval );
    set_function( state, ASTRAL_XORSTR( "latency" ).get( ), &engine_latency );
    set_function( state, ASTRAL_XORSTR( "view_angles" ).get( ), &engine_view_angles );
    set_function( state, ASTRAL_XORSTR( "set_view_angles" ).get( ), &engine_set_view_angles );
    set_function( state, ASTRAL_XORSTR( "command" ).get( ), &engine_command );
    lua_setglobal( state, ASTRAL_XORSTR( "engine" ).get( ) );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "Command" ).get( ), &engine_command );
    lua_setglobal( state, ASTRAL_XORSTR( "client" ).get( ) );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "find" ).get( ), &cvar_find );
    lua_setglobal( state, ASTRAL_XORSTR( "cvar" ).get( ) );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "get" ).get( ), &config_get );
    set_function( state, ASTRAL_XORSTR( "set" ).get( ), &config_set );
    set_function( state, ASTRAL_XORSTR( "active" ).get( ), &config_active );
    lua_setglobal( state, ASTRAL_XORSTR( "config" ).get( ) );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "listen" ).get( ), &events_listen );
    lua_setglobal( state, ASTRAL_XORSTR( "events" ).get( ) );

    lua_newtable( state );
    set_function( state, ASTRAL_XORSTR( "tickbase" ).get( ), &runtime_tickbase );
    set_function( state, ASTRAL_XORSTR( "crits" ).get( ), &runtime_crits );
    set_function( state, ASTRAL_XORSTR( "seed_prediction" ).get( ), &runtime_seed_prediction );
    set_function( state, ASTRAL_XORSTR( "network" ).get( ), &runtime_network );
    set_function( state, ASTRAL_XORSTR( "prediction" ).get( ), &runtime_prediction );
    set_function( state, ASTRAL_XORSTR( "spectators" ).get( ), &runtime_spectators );
    set_function( state, ASTRAL_XORSTR( "request_seed" ).get( ), &runtime_seed_request );
    set_function( state, ASTRAL_XORSTR( "clear_tickbase" ).get( ), &runtime_clear_tickbase );
    set_function( state, ASTRAL_XORSTR( "clear_crits" ).get( ), &runtime_clear_crits );
    set_function( state, ASTRAL_XORSTR( "clear_seed_prediction" ).get( ), &runtime_clear_seed );
    lua_setglobal( state, ASTRAL_XORSTR( "runtime" ).get( ) );

    lua_newtable( state );
    set_integer( state, ASTRAL_XORSTR( "attack" ).get( ), IN_ATTACK );
    set_integer( state, ASTRAL_XORSTR( "jump" ).get( ), IN_JUMP );
    set_integer( state, ASTRAL_XORSTR( "duck" ).get( ), IN_DUCK );
    set_integer( state, ASTRAL_XORSTR( "forward" ).get( ), IN_FORWARD );
    set_integer( state, ASTRAL_XORSTR( "back" ).get( ), IN_BACK );
    set_integer( state, ASTRAL_XORSTR( "use" ).get( ), IN_USE );
    set_integer( state, ASTRAL_XORSTR( "move_left" ).get( ), IN_MOVELEFT );
    set_integer( state, ASTRAL_XORSTR( "move_right" ).get( ), IN_MOVERIGHT );
    set_integer( state, ASTRAL_XORSTR( "attack2" ).get( ), IN_ATTACK2 );
    set_integer( state, ASTRAL_XORSTR( "reload" ).get( ), IN_RELOAD );
    set_integer( state, ASTRAL_XORSTR( "attack3" ).get( ), IN_ATTACK3 );
    lua_setglobal( state, ASTRAL_XORSTR( "buttons" ).get( ) );

    lua_pushglobaltable( state );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_INVALID" ).get( ), LOADOUT_POSITION_INVALID );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_PRIMARY" ).get( ), LOADOUT_POSITION_PRIMARY );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_SECONDARY" ).get( ), LOADOUT_POSITION_SECONDARY );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_MELEE" ).get( ), LOADOUT_POSITION_MELEE );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_UTILITY" ).get( ), LOADOUT_POSITION_UTILITY );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_BUILDING" ).get( ), LOADOUT_POSITION_BUILDING );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_PDA" ).get( ), LOADOUT_POSITION_PDA );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_PDA2" ).get( ), LOADOUT_POSITION_PDA2 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_HEAD" ).get( ), LOADOUT_POSITION_HEAD );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_MISC" ).get( ), LOADOUT_POSITION_MISC );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_ACTION" ).get( ), LOADOUT_POSITION_ACTION );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_MISC2" ).get( ), LOADOUT_POSITION_MISC2 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT" ).get( ), LOADOUT_POSITION_TAUNT );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT2" ).get( ), LOADOUT_POSITION_TAUNT2 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT3" ).get( ), LOADOUT_POSITION_TAUNT3 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT4" ).get( ), LOADOUT_POSITION_TAUNT4 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT5" ).get( ), LOADOUT_POSITION_TAUNT5 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT6" ).get( ), LOADOUT_POSITION_TAUNT6 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT7" ).get( ), LOADOUT_POSITION_TAUNT7 );
    set_integer( state, ASTRAL_XORSTR( "LOADOUT_POSITION_TAUNT8" ).get( ), LOADOUT_POSITION_TAUNT8 );
    lua_pop( state, 1 );

    lua_getglobal( state, ASTRAL_XORSTR( "draw" ).get( ) );
    set_function( state, ASTRAL_XORSTR( "world_to_screen" ).get( ), &draw_world_to_screen );
    lua_pop( state, 1 );
}

void script_api::paint( )
{
    g_scripts.service( );
    g_scripts.dispatch( script::event_t::paint );
}

void script_api::create_move_pre_pred( CUserCmd *cmd )
{
    this->m_cmd = cmd;
    g_scripts.dispatch( script::event_t::create_move_pre_pred, &push_command );
    this->m_cmd = nullptr;
}

void script_api::create_move( CUserCmd *cmd )
{
    this->m_cmd = cmd;
    g_scripts.dispatch( script::event_t::create_move, &push_command );
    this->m_cmd = nullptr;
}

void script_api::create_move_post_pred( CUserCmd *cmd )
{
    this->m_cmd = cmd;
    g_scripts.dispatch( script::event_t::create_move_post_pred, &push_command );
    this->m_cmd = nullptr;
}

void script_api::frame_stage( ClientFrameStage_t stage )
{
    this->m_stage = stage;
    g_scripts.dispatch( script::event_t::frame_stage, &push_stage );
}

void script_api::level_init( )
{
    g_scripts.dispatch( script::event_t::level_init );
}

void script_api::level_shutdown( )
{
    g_scripts.dispatch( script::event_t::level_shutdown );
}

void script_api::shutdown( )
{
    g_scripts.shutdown( );

    if ( this->m_listening )
    {
        g_tf2.m_game_events->RemoveListener( this );
        this->m_listening = false;
    }
}

bool script_api::listen( const char *name )
{
    if ( g_tf2.m_game_events->FindListener( this, name ) )
    {
        return true;
    }

    if ( !g_tf2.m_game_events->AddListener( this, name, false ) )
    {
        return false;
    }

    this->m_listening = true;
    return true;
}

void script_api::FireGameEvent( IGameEvent *event )
{
    if ( !g_cl.m_is_running )
    {
        return;
    }

    const char *name{ event->GetName( ) };
    this->m_event = event;

    g_scripts.for_each_state( [ name ]( lua_State *state )
    {
        lua_pushcfunction( state, &dispatch_event );
        lua_pushlightuserdata( state, const_cast< char * >( name ) );
        g_scripts.call( state, 1 );
    } );

    this->m_event = nullptr;
}

int script_api::GetEventDebugID( )
{
    return 42;
}
