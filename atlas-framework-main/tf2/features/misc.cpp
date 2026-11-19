#include "misc.hpp"
#include "movement_surface.hpp"

#include "gui/gui.hpp"

#include "movement_recorder.hpp"

ATLAS_NO_DESTROY misc g_misc{ };

namespace
{
    constexpr u32   MOVEMENT_LAB_MAGIC{ 0x42414C4D }; // MLAB
    constexpr u16   MOVEMENT_LAB_VERSION{ 1 };
    constexpr usize MOVEMENT_LAB_PATH_CAPACITY{ 1024 };

    struct movement_lab_file_t
    {
        u32                  m_magic{ MOVEMENT_LAB_MAGIC };
        u16                  m_version{ MOVEMENT_LAB_VERSION };
        u16                  m_point_count{ };
        u16                  m_route_count{ };
        u8                   m_checkpoint_valid{ };
        u8                   m_reserved[ 3 ]{ };
        vec3                 m_checkpoint_origin{ };
        vec3                 m_checkpoint_angles{ };
        movement_lab_point_t m_points[ 64 ]{ };
        vec3                 m_route_points[ 32 ]{ };
    };

    bool movement_lab_path( wchar_t ( &path )[ MOVEMENT_LAB_PATH_CAPACITY ], const char *map )
    {
        u32 length{ I( GetModuleFileNameW )( nullptr, path, MOVEMENT_LAB_PATH_CAPACITY - 128 ) };
        if ( !length || length >= MOVEMENT_LAB_PATH_CAPACITY - 128 || !map || !*map )
        {
            return false;
        }
        while ( length && path[ length - 1 ] != L'\\' )
        {
            --length;
        }
        path[ length ] = L'\0';
        str::append( path, MOVEMENT_LAB_PATH_CAPACITY, ATLAS_XORWSTR( L"AstralMod" ).get( ) );
        I( CreateDirectoryW )( path, nullptr );
        str::append( path, MOVEMENT_LAB_PATH_CAPACITY, ATLAS_XORWSTR( L"\\MoveLab" ).get( ) );
        I( CreateDirectoryW )( path, nullptr );
        usize cursor{ str::length( path ) };
        path[ cursor++ ] = L'\\';
        while ( *map && cursor + 6 < MOVEMENT_LAB_PATH_CAPACITY )
        {
            path[ cursor++ ] = static_cast< wchar_t >( *map++ );
        }
        path[ cursor ] = L'\0';
        str::append( path, MOVEMENT_LAB_PATH_CAPACITY, ATLAS_XORWSTR( L".mlab" ).get( ) );
        return true;
    }

    constexpr u64 FULLUPDATE_STALL_THRESHOLD{ 1000 };
    constexpr u64 FULLUPDATE_COOLDOWN{ 30 * 1000 };

    struct mvm_upgrade_t
    {
        i32 m_index;
        i32 m_slot;
        i32 m_levels;
    };

    constexpr mvm_upgrade_t SCOUT_UPGRADES[]{ { 52, -1, 3 }, { 53, -1, 3 }, { 54, -1, 3 }, { 55, -1, 3 }, { 46, 1, 1 }, { 0, 0, 4 }, { 2, 0, 4 }, { 5, 0, 4 } };
    constexpr mvm_upgrade_t SOLDIER_UPGRADES[]{ { 52, -1, 3 }, { 0, 0, 4 }, { 53, -1, 3 }, { 2, 0, 4 }, { 54, -1, 3 }, { 5, 0, 4 }, { 47, 0, 4 }, { 33, 0, 4 } };
    constexpr mvm_upgrade_t PYRO_UPGRADES[]{ { 52, -1, 3 }, { 53, -1, 3 }, { 54, -1, 3 }, { 55, -1, 3 }, { 30, 0, 4 }, { 31, 0, 4 }, { 0, 0, 4 }, { 2, 0, 4 } };
    constexpr mvm_upgrade_t DEMOMAN_UPGRADES[]{ { 52, -1, 3 }, { 0, 0, 4 }, { 53, -1, 3 }, { 2, 0, 4 }, { 54, -1, 3 }, { 5, 0, 4 }, { 8, 0, 6 }, { 27, 0, 4 } };
    constexpr mvm_upgrade_t HEAVY_UPGRADES[]{ { 52, -1, 3 }, { 53, -1, 3 }, { 54, -1, 3 }, { 55, -1, 3 }, { 0, 0, 4 }, { 2, 0, 4 }, { 5, 0, 4 }, { 11, 0, 4 } };
    constexpr mvm_upgrade_t ENGINEER_UPGRADES[]{ { 22, 5, 3 }, { 20, 5, 3 }, { 52, -1, 3 }, { 53, -1, 3 }, { 54, -1, 3 }, { 55, -1, 3 }, { 57, -1, 5 }, { 4, 2, 4 } };
    constexpr mvm_upgrade_t SNIPER_UPGRADES[]{ { 17, 0, 4 }, { 35, 0, 3 }, { 2, 0, 4 }, { 5, 0, 4 }, { 6, 0, 3 }, { 12, 0, 1 }, { 40, 0, 3 }, { 41, 0, 4 } };
    constexpr mvm_upgrade_t SPY_UPGRADES[]{ { 37, 3, 3 }, { 52, -1, 3 }, { 53, -1, 3 }, { 54, -1, 3 }, { 55, -1, 3 }, { 4, 2, 4 }, { 0, 2, 4 } };

    template< usize size >
    constexpr const mvm_upgrade_t *mvm_plan( const mvm_upgrade_t ( &plan )[ size ], usize &count )
    {
        count = size;
        return plan;
    }

    const mvm_upgrade_t *mvm_class_plan( i32 player_class, usize &count )
    {
        switch ( player_class )
        {
            case TF_CLASS_SCOUT: return mvm_plan( SCOUT_UPGRADES, count );
            case TF_CLASS_SNIPER: return mvm_plan( SNIPER_UPGRADES, count );
            case TF_CLASS_SOLDIER: return mvm_plan( SOLDIER_UPGRADES, count );
            case TF_CLASS_DEMOMAN: return mvm_plan( DEMOMAN_UPGRADES, count );
            case TF_CLASS_HEAVYWEAPONS: return mvm_plan( HEAVY_UPGRADES, count );
            case TF_CLASS_PYRO: return mvm_plan( PYRO_UPGRADES, count );
            case TF_CLASS_SPY: return mvm_plan( SPY_UPGRADES, count );
            case TF_CLASS_ENGINEER: return mvm_plan( ENGINEER_UPGRADES, count );
            default: count = 0; return nullptr;
        }
    }

    const char *mvm_class_name( i32 player_class )
    {
        switch ( player_class )
        {
            case TF_CLASS_SCOUT: return ATLAS_XORSTR_STATIC( "scout" );
            case TF_CLASS_SNIPER: return ATLAS_XORSTR_STATIC( "sniper" );
            case TF_CLASS_SOLDIER: return ATLAS_XORSTR_STATIC( "soldier" );
            case TF_CLASS_DEMOMAN: return ATLAS_XORSTR_STATIC( "demoman" );
            case TF_CLASS_MEDIC: return ATLAS_XORSTR_STATIC( "medic" );
            case TF_CLASS_HEAVYWEAPONS: return ATLAS_XORSTR_STATIC( "heavyweapons" );
            case TF_CLASS_PYRO: return ATLAS_XORSTR_STATIC( "pyro" );
            case TF_CLASS_SPY: return ATLAS_XORSTR_STATIC( "spy" );
            case TF_CLASS_ENGINEER: return ATLAS_XORSTR_STATIC( "engineer" );
            default: return nullptr;
        }
    }

    struct game_repeated_strings_t
    {
        void **m_elements;
        i32    m_current;
        i32    m_total;
        i32    m_capacity;

        void clear( )
        {
            for ( i32 index{ }; m_elements && index < m_current; ++index )
            {
                if ( m_elements[ index ] )
                    g_tf2.m_Protobuf_String_Assign.call_as< void >( m_elements[ index ], "", static_cast< usize >( 0 ) );
            }
            m_current = 0;
        }

        bool add( const char *value )
        {
            if ( !value )
                return false;
            void *string{ };
            if ( m_current >= m_total )
            {
                if ( m_total == m_capacity )
                    g_tf2.m_Protobuf_RepeatedStrings_Reserve.call_as< void >( this, m_capacity + 1 );
                string = g_tf2.m_Protobuf_RepeatedStrings_NewElement.call_as< void * >( );
                if ( !string || !m_elements )
                    return false;
                m_elements[ m_current ] = string;
                ++m_total;
            } else
            {
                string = m_elements ? m_elements[ m_current ] : nullptr;
                if ( !string )
                    return false;
            }
            ++m_current;
            g_tf2.m_Protobuf_String_Assign.call_as< void >( string, value, str::length( value ) );
            return true;
        }
    };

    struct mvm_tour_pair_t
    {
        i32 m_mission;
        i32 m_badge;
    };

    bool apply_mvm_queue_criteria( C_TFPartyClient *party, i32 match_group )
    {
        if ( !party || g_tf2.m_Protobuf_RepeatedStrings_Reserve.is_null( ) ||
             g_tf2.m_Protobuf_RepeatedStrings_NewElement.is_null( ) || g_tf2.m_Protobuf_String_Assign.is_null( ) )
            return false;

        void *schema{ g_tf2.m_GetItemSchema.call_as< void * >( ) };
        if ( !schema )
            return false;
        const i32 mission_count{ *reinterpret_cast< i32 * >( static_cast< u8 * >( schema ) + 2336 ) };
        const i32 tour_count{ *reinterpret_cast< i32 * >( static_cast< u8 * >( schema ) + 2368 ) };
        u8       *missions_base{ *reinterpret_cast< u8 ** >( static_cast< u8 * >( schema ) + 2320 ) };
        u8       *tours_base{ *reinterpret_cast< u8 ** >( static_cast< u8 * >( schema ) + 2352 ) };
        if ( !missions_base || !tours_base || mission_count <= 0 || mission_count > 1024 || tour_count <= 0 || tour_count > 256 )
            return false;

        void *criteria{ reinterpret_cast< u8 * >( party ) + 432 };
        void *proto{ util::call_virtual< void *( * )( void * ) >( criteria, 1 )( criteria ) };
        if ( !proto )
            return false;
        auto *field{ reinterpret_cast< game_repeated_strings_t * >( static_cast< u8 * >( proto ) +
                                                                    ( match_group == k_eTFMatchGroup_MvM_Practice ? 64 : 40 ) ) };

        const char *selected[ 31 ]{ };
        usize       selected_count{ };
        const char *tour_name{ "" };
        if ( match_group == k_eTFMatchGroup_MvM_MannUp )
        {
            const i32 tour_index{ g_cfg.m_mvm.m_mann_up_tour - 1 };
            if ( tour_index >= 0 && tour_index < tour_count )
            {
                u8 *tour{ tours_base + 88 * tour_index };
                tour_name = *reinterpret_cast< const char ** >( tour );
                auto     *pairs{ *reinterpret_cast< mvm_tour_pair_t ** >( tour + 40 ) };
                const i32 pair_count{ *reinterpret_cast< i32 * >( tour + 56 ) };
                u32       completed{ };
                i32       unused{ };
                if ( g_cfg.m_mvm.m_mann_up_uncompleted && !g_tf2.m_GetMvmTourBadgeMask.is_null( ) )
                    g_tf2.m_GetMvmTourBadgeMask.call_as< bool >( tour_index, &unused, &completed );
                for ( i32 index{ }; pairs && index < pair_count && selected_count < 31; ++index )
                {
                    const i32 mission{ pairs[ index ].m_mission };
                    const i32 badge{ pairs[ index ].m_badge };
                    if ( mission < 0 || mission >= mission_count || badge < 0 ||
                         ( g_cfg.m_mvm.m_mann_up_uncompleted && ( completed & ( 1u << badge ) ) ) )
                        continue;
                    const char *name{ *reinterpret_cast< const char ** >( missions_base + 48 * mission + 8 ) };
                    if ( name && *name )
                        selected[ selected_count++ ] = name;
                }
            }
            void *tour_string{ *reinterpret_cast< void ** >( static_cast< u8 * >( proto ) + 32 ) };
            if ( tour_string )
                g_tf2.m_Protobuf_String_Assign.call_as< void >( tour_string, tour_name ? tour_name : "",
                                                                tour_name ? str::length( tour_name ) : 0 );
        } else if ( g_cfg.m_mvm.m_boot_camp_missions )
        {
            usize bit{ };
            for ( i32 tour_index{ }; tour_index < tour_count && bit < 31; ++tour_index )
            {
                u8       *tour{ tours_base + 88 * tour_index };
                auto     *pairs{ *reinterpret_cast< mvm_tour_pair_t ** >( tour + 40 ) };
                const i32 pair_count{ *reinterpret_cast< i32 * >( tour + 56 ) };
                for ( i32 index{ }; pairs && index < pair_count && bit < 31; ++index )
                {
                    if ( pairs[ index ].m_badge >= 0 || pairs[ index ].m_mission < 0 || pairs[ index ].m_mission >= mission_count )
                        continue;
                    const char *name{ *reinterpret_cast< const char ** >( missions_base + 48 * pairs[ index ].m_mission + 8 ) };
                    bool        duplicate{ false };
                    for ( usize old{ }; old < bit; ++old )
                        if ( selected[ old ] == name )
                            duplicate = true;
                    if ( duplicate )
                        continue;
                    if ( g_cfg.m_mvm.m_boot_camp_missions & ( 1u << bit ) )
                        selected[ selected_count++ ] = name;
                    ++bit;
                }
            }
        }

        field->clear( );
        for ( usize index{ }; index < selected_count; ++index )
            if ( !field->add( selected[ index ] ) )
                return false;
        return true;
    }
} // namespace

void misc::on_level_init( const char *map_name )
{
    // Level init normally follows shutdown, but preserve the old map if the engine skips it.
    this->save_movement_lab( );
    this->m_lab_point_count = 0;
    this->m_route_point_count = 0;
    this->m_route_index = 0;
    this->m_checkpoint_valid = false;
    this->m_movement_lab_dirty = false;

    usize cursor{ };
    for ( ; map_name && *map_name && cursor + 1 < sizeof( this->m_movement_lab_map ); ++map_name )
    {
        const char value{ str::to_lower( *map_name ) };
        this->m_movement_lab_map[ cursor++ ] =
            str::is_alpha( value ) || str::is_digit( value ) || value == '-' ? value : '_';
    }
    this->m_movement_lab_map[ cursor ] = '\0';
    this->load_movement_lab( );
}

void misc::on_level_shutdown( )
{
    this->save_movement_lab( );
    this->m_lab_point_count = 0;
    this->m_route_point_count = 0;
    this->m_route_index = 0;
    this->m_checkpoint_valid = false;
    this->m_movement_lab_map[ 0 ] = '\0';
    this->m_movement_lab_dirty = false;
}

void misc::load_movement_lab( )
{
    wchar_t path[ MOVEMENT_LAB_PATH_CAPACITY ]{ };
    if ( !movement_lab_path( path, this->m_movement_lab_map ) )
    {
        return;
    }
    void *file{ I( CreateFileW )( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr ) };
    if ( file == INVALID_HANDLE_VALUE )
    {
        return;
    }
    movement_lab_file_t data{ };
    DWORD read{ };
    const bool valid{ I( ReadFile )( file, &data, sizeof( data ), &read, nullptr ) && read == sizeof( data ) &&
                      data.m_magic == MOVEMENT_LAB_MAGIC && data.m_version == MOVEMENT_LAB_VERSION &&
                      data.m_point_count <= 64 && data.m_route_count <= 32 };
    I( CloseHandle )( file );
    if ( !valid )
    {
        return;
    }
    this->m_lab_point_count = data.m_point_count;
    this->m_route_point_count = data.m_route_count;
    this->m_checkpoint_valid = data.m_checkpoint_valid != 0;
    this->m_checkpoint_origin = data.m_checkpoint_origin;
    this->m_checkpoint_angles = data.m_checkpoint_angles;
    atl::memcpy( this->m_lab_points, data.m_points, sizeof( this->m_lab_points ) );
    atl::memcpy( this->m_route_points, data.m_route_points, sizeof( this->m_route_points ) );
}

void misc::save_movement_lab( )
{
    if ( !this->m_movement_lab_dirty || !this->m_movement_lab_map[ 0 ] )
    {
        return;
    }
    wchar_t path[ MOVEMENT_LAB_PATH_CAPACITY ]{ };
    if ( !movement_lab_path( path, this->m_movement_lab_map ) )
    {
        return;
    }
    movement_lab_file_t data{ };
    data.m_point_count = static_cast< u16 >( this->m_lab_point_count );
    data.m_route_count = static_cast< u16 >( this->m_route_point_count );
    data.m_checkpoint_valid = this->m_checkpoint_valid ? 1 : 0;
    data.m_checkpoint_origin = this->m_checkpoint_origin;
    data.m_checkpoint_angles = this->m_checkpoint_angles;
    atl::memcpy( data.m_points, this->m_lab_points, sizeof( this->m_lab_points ) );
    atl::memcpy( data.m_route_points, this->m_route_points, sizeof( this->m_route_points ) );

    void *file{ I( CreateFileW )( path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr ) };
    if ( file == INVALID_HANDLE_VALUE )
    {
        return;
    }
    DWORD written{ };
    const bool saved{ I( WriteFile )( file, &data, sizeof( data ), &written, nullptr ) &&
                      written == sizeof( data ) && I( FlushFileBuffers )( file ) };
    I( CloseHandle )( file );
    if ( saved )
    {
        this->m_movement_lab_dirty = false;
    }
}

void misc::auto_fullupdate( )
{
    mvm_automation( );
    const u64 now{ I( GetTickCount64 )( ) };
    if ( !g_tf2.m_engine->IsInGame( ) || !g_cl.m_local )
    {
        this->m_fullupdate_last_frame = now;
        this->m_fullupdate_armed      = false;
        return;
    }

    if ( !this->m_fullupdate_armed )
    {
        this->m_fullupdate_last_frame = now;
        this->m_fullupdate_armed      = true;
        return;
    }

    const u64 elapsed{ now - this->m_fullupdate_last_frame };
    this->m_fullupdate_last_frame = now;
    if ( elapsed < FULLUPDATE_STALL_THRESHOLD || now < this->m_fullupdate_cooldown )
    {
        return;
    }

    g_tf2.m_engine->ClientCmd_Unrestricted( ATLAS_XORSTR( "cl_fullupdate" ).get( ) );
    this->m_fullupdate_cooldown = now + FULLUPDATE_COOLDOWN;
}

// https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/tf/tf_weapon_shotgun.cpp#L331
ATLAS_FORCE_INLINE f32 fan_jump_pitch( f32 speed, f32 rise )
{
    f32 best_pitch{ };
    f32 best_climb{ };

    for ( i32 degrees{ 1 }; degrees < 90; ++degrees )
    {
        const f32 pitch{ static_cast< f32 >( degrees ) };
        const f32 sine{ math::sin( DEG2RAD( pitch ) ) };
        const f32 cosine{ math::cos( DEG2RAD( pitch ) ) };
        const f32 climb{ FAN_KNOCKBACK_SPEED * sine + ( speed * sine + rise * cosine ) * cosine };

        if ( climb > best_climb )
        {
            best_climb = climb;
            best_pitch = pitch;
        }
    }

    return best_pitch;
}

void misc::on_pre_prediction( )
{
    this->m_movement_owner = movement_owner_t::none;
    this->m_movement_debug_valid = false;
    movement_lab_hotkeys( );
    freecam_input( );
    null_movement( );
    fast_stop( );
    fast_accel( );
    blockbot( );
    route_follow( );
    auto_bounce( );
    goomba( );
    wall_climb( );
    pixel_surf( );
    texture_bug( );
    air_stuck( );
    pixel_jump( );
    movement_finder( );
    fake_duck( );
    mini_jump( );
    auto_crouch( );
    bhop( );
    delayed_hop( );
    auto_strafe( );
    half_sideways( );
    double_jump_strafe( );
    strafe_optimizer( );
    avoid_strafe_walls( );
    jump_stats( );
    taunt_spin( );
    fan_jump( );
    mvm_station_move( );

    this->m_edge_grounded = g_cl.m_local->IsOnGround( );
}

bool misc::claim_movement( movement_owner_t owner )
{
    if ( static_cast< u8 >( owner ) < static_cast< u8 >( this->m_movement_owner ) )
    {
        return false;
    }
    this->m_movement_owner = owner;
    return true;
}

void misc::mvm_station_move( )
{
    if ( !g_cfg.m_mvm.m_buybot || this->m_mvm_buy_finished || !g_cl.m_local || !g_cl.m_cmd ||
         !g_cl.m_local->IsAlive( ) || g_cl.m_local->m_bInUpgradeZone( ) )
    {
        this->m_mvm_station_best_distance = FLT_MAX;
        this->m_mvm_station_progress_time = 0;
        return;
    }

    C_TFGameRules *rules{ TFGameRules( ) };
    if ( !rules || !rules->m_bPlayingMannVsMachine( ) )
        return;

    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    vec3       target{ };
    f32        nearest{ FLT_MAX };
    const i32  highest{ g_tf2.m_entity_list->GetHighestEntityIndex( ) };
    for ( i32 index{ g_tf2.m_engine->GetMaxClients( ) + 1 }; index <= highest; ++index )
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_BaseEntity  *entity{ client ? static_cast< C_BaseEntity * >( client ) : nullptr };
        const char    *class_name{ entity ? entity->GetClassname( ) : nullptr };
        if ( !class_name || HASH_RT( class_name ) != HASH_CT( "func_upgradestation" ) )
            continue;

        const vec3 point{ entity->WorldSpaceCenter( ) };
        const f32  distance{ origin.dist_to_sqr( point ) };
        if ( distance < nearest )
        {
            nearest = distance;
            target  = point;
        }
    }

    if ( nearest == FLT_MAX )
        return;
    const vec3 offset{ target - origin };
    const f32  length{ offset.length2d( ) };
    if ( length <= 8.0f )
        return;

    const f32 direction_x{ offset.x / length };
    const f32 direction_y{ offset.y / length };
    const f32 yaw{ DEG2RAD( g_cl.m_cmd->viewangles.y ) };
    const f32 forward_x{ math::cos( yaw ) };
    const f32 forward_y{ math::sin( yaw ) };
    const f32 right_x{ -forward_y };
    const f32 right_y{ forward_x };
    g_cl.m_cmd->forwardmove = ( direction_x * forward_x + direction_y * forward_y ) * 450.0f;
    g_cl.m_cmd->sidemove    = ( direction_x * right_x + direction_y * right_y ) * 450.0f;

    const u64 now{ I( GetTickCount64 )( ) };
    if ( length + 8.0f < this->m_mvm_station_best_distance )
    {
        this->m_mvm_station_best_distance = length;
        this->m_mvm_station_progress_time = now;
    } else if ( this->m_mvm_station_progress_time && now - this->m_mvm_station_progress_time >= 1250 )
    {
        g_cl.m_cmd->buttons |= IN_JUMP;
        this->m_mvm_station_best_distance = length;
        this->m_mvm_station_progress_time = now;
    }
}

void misc::mvm_automation( )
{
    C_TFGameRules   *rules{ TFGameRules( ) };
    const u64        now{ I( GetTickCount64 )( ) };
    C_TFPartyClient *party{ TFPartyClient( ) };
    if ( party && !g_tf2.m_engine->IsInGame( ) && !g_tf2.m_engine->IsConnected( ) &&
         now >= this->m_mvm_queue_time )
    {
        const i32 match_group{ g_cfg.m_mvm.m_auto_mann_up_queue ? k_eTFMatchGroup_MvM_MannUp
                                                                : k_eTFMatchGroup_MvM_Practice };
        if ( ( g_cfg.m_mvm.m_auto_mann_up_queue || g_cfg.m_mvm.m_auto_boot_camp_queue ) &&
             !party->BInQueueForMatchGroup( match_group ) )
        {
            apply_mvm_queue_criteria( party, match_group );
            party->RequestQueueForMatch( match_group );
        }
        this->m_mvm_queue_time = now + 5000;
    }

    if ( !rules || !rules->m_bPlayingMannVsMachine( ) || !g_cl.m_local )
    {
        this->m_mvm_buy_step          = 0;
        this->m_mvm_buy_class         = 0;
        this->m_mvm_buy_finished      = false;
        this->m_mvm_buy_finished_time = 0;
        this->m_mvm_respec_attempts   = 0;
        return;
    }

    if ( g_cfg.m_mvm.m_buybot_auto_class && now >= this->m_mvm_class_time )
    {
        const bool funded{ g_cfg.m_mvm.m_buybot_max_cash <= 0 ||
                           g_cl.m_local->m_nCurrency( ) >= g_cfg.m_mvm.m_buybot_max_cash };
        i32        desired{ funded ? math::clamp( g_cfg.m_mvm.m_buybot_class, 1, 9 ) : TF_CLASS_MEDIC };
        if ( desired == TF_CLASS_MEDIC && funded )
            desired = TF_CLASS_HEAVYWEAPONS;
        const i32 rental{ desired == TF_CLASS_MEDIC ? 998 : desired == TF_CLASS_SNIPER ? 752
                                                                                       : 0 };
        if ( g_cfg.m_mvm.m_rent_items && rental &&
             ( rental != this->m_mvm_rented_definition || now >= this->m_mvm_rent_time ) )
        {
            g_tf2.m_CStorePage_DoPreviewItem.call_as< void >( nullptr, static_cast< u16 >( rental ) );
            this->m_mvm_rented_definition = rental;
            this->m_mvm_rent_time         = now + 30000;
        }
        if ( desired != g_cl.m_local->m_iClass( ) )
        {
            if ( const char *name{ mvm_class_name( desired ) } )
            {
                char command[ 48 ]{ };
                str::format_to( command, sizeof( command ), ATLAS_XORSTR( "joinclass {}" ).get( ), name );
                g_tf2.m_engine->ClientCmd_Unrestricted( command );
                this->m_mvm_class_time = now + 1000;
                return;
            }
        }
    }

    if ( !g_cfg.m_mvm.m_buybot )
    {
        this->m_mvm_buy_step          = 0;
        this->m_mvm_buy_class         = 0;
        this->m_mvm_buy_finished      = false;
        this->m_mvm_buy_finished_time = 0;
        this->m_mvm_respec_attempts   = 0;
    } else if ( g_cl.m_local->IsAlive( ) && g_cl.m_local->m_bInUpgradeZone( ) &&
                now >= this->m_mvm_buy_time && !this->m_mvm_buy_finished )
    {
        const i32 player_class{ g_cl.m_local->m_iClass( ) };
        if ( this->m_mvm_buy_class != player_class )
        {
            this->m_mvm_buy_class = player_class;
            this->m_mvm_buy_step  = 0;
        }

        usize                count{ };
        const mvm_upgrade_t *plan{ mvm_class_plan( player_class, count ) };
        if ( !plan || this->m_mvm_buy_step >= count )
        {
            this->m_mvm_buy_finished      = true;
            this->m_mvm_buy_finished_time = now;
        } else
        {
            const mvm_upgrade_t &upgrade{ plan[ this->m_mvm_buy_step++ ] };
            g_tf2.m_engine->ServerCmdKeyValues( new KeyValues( ATLAS_XORSTR_STATIC( "MvM_UpgradesBegin" ) ) );
            for ( i32 level{ }; level < upgrade.m_levels; ++level )
            {
                KeyValues *message{ new KeyValues( ATLAS_XORSTR_STATIC( "MVM_Upgrade" ) ) };
                if ( KeyValues * data{ message->FindKey( ATLAS_XORSTR_STATIC( "Upgrade" ), true ) } )
                {
                    data->SetInt( ATLAS_XORSTR_STATIC( "itemslot" ), upgrade.m_slot );
                    data->SetInt( ATLAS_XORSTR_STATIC( "upgrade" ), upgrade.m_index );
                    data->SetInt( ATLAS_XORSTR_STATIC( "count" ), 1 );
                }
                g_tf2.m_engine->ServerCmdKeyValues( message );
            }
            KeyValues *done{ new KeyValues( ATLAS_XORSTR_STATIC( "MvM_UpgradesDone" ) ) };
            done->SetInt( ATLAS_XORSTR_STATIC( "num_upgrades" ), upgrade.m_levels );
            g_tf2.m_engine->ServerCmdKeyValues( done );
            this->m_mvm_buy_time = now + 100;
        }
    }

    if ( g_cfg.m_mvm.m_buybot && g_cfg.m_mvm.m_respec_recovery && this->m_mvm_buy_finished &&
         g_cl.m_local->m_bInUpgradeZone( ) && this->m_mvm_buy_finished_time &&
         now - this->m_mvm_buy_finished_time >= 2000 && this->m_mvm_respec_attempts < 3 )
    {
        ConVar    *respec{ g_tf2.m_cvar->FindVar( ATLAS_XORSTR( "tf_mvm_respec_enabled" ).get( ) ) };
        const bool still_funded{ g_cfg.m_mvm.m_buybot_max_cash > 0 &&
                                 g_cl.m_local->m_nCurrency( ) >= g_cfg.m_mvm.m_buybot_max_cash };
        if ( respec && respec->GetInt( ) == 1 && still_funded )
        {
            g_tf2.m_engine->ServerCmdKeyValues( new KeyValues( ATLAS_XORSTR_STATIC( "MVM_Respec" ) ) );
            KeyValues *done{ new KeyValues( ATLAS_XORSTR_STATIC( "MvM_UpgradesDone" ) ) };
            done->SetInt( ATLAS_XORSTR_STATIC( "num_upgrades" ), -1 );
            g_tf2.m_engine->ServerCmdKeyValues( done );
            ++this->m_mvm_respec_attempts;
            this->m_mvm_buy_step          = 0;
            this->m_mvm_buy_finished      = false;
            this->m_mvm_buy_finished_time = 0;
            this->m_mvm_buy_time          = now + 500;
        } else
        {
            this->m_mvm_buy_finished_time = 0;
        }
    }

    if ( g_cfg.m_mvm.m_instant_respawn && !g_cl.m_local->IsAlive( ) && now >= this->m_mvm_respawn_time )
    {
        KeyValues *response{ new KeyValues( ATLAS_XORSTR_STATIC( "MVM_Revive_Response" ) ) };
        response->SetInt( ATLAS_XORSTR_STATIC( "accepted" ), 1 );
        g_tf2.m_engine->ServerCmdKeyValues( response );
        this->m_mvm_respawn_time = now + 250;
    }

    if ( g_cfg.m_mvm.m_auto_ready && rules->m_iRoundState( ) == GR_STATE_BETWEEN_RNDS && now >= this->m_mvm_ready_time )
    {
        g_tf2.m_engine->ClientCmd_Unrestricted( ATLAS_XORSTR( "tournament_player_readystate 1" ).get( ) );
        this->m_mvm_ready_time = now + 1000;
    }
}

void misc::on_prediction( )
{
    edge_jump( );
    long_jump( );
}

void misc::on_post_prediction( )
{
    perform_taunt_cancel_exploit( );
    anti_afk( );
    noisemaker_spam( );
}

static bool movement_bind_active( bind_t &bind )
{
    return bind.m_key == 0 ? true : gui::bind_active( bind );
}

void misc::bhop( )
{
    if ( !g_cfg.m_misc.m_bhop || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    const bool held{ ( g_cl.m_cmd->buttons & IN_JUMP ) != 0 };
    const bool pressed{ held && !this->m_bhop_jump_held };

    this->m_bhop_jump_held = held;

    if ( !held )
    {
        return;
    }

    if ( pressed && ( g_cl.m_local->IsOnGround( ) ||
                      g_cl.m_local->m_iClass( ) == TF_CLASS_SCOUT ) )
    {
        return;
    }

    if ( !g_cl.m_local->IsOnGround( ) || g_cl.m_local->IsDucked( ) ||
         ( g_cl.m_local->m_afButtonLast( ) & IN_JUMP ) )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
    }
}

void misc::delayed_hop( )
{
    if ( !g_cfg.m_misc.m_delayed_hop ||
         !movement_bind_active( g_cfg.m_misc.m_delayed_hop_key ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        this->m_delayed_hop_landing_ticks = 0;
        return;
    }

    if ( !g_cl.m_local->IsOnGround( ) )
    {
        this->m_delayed_hop_landing_ticks = 0;
        return;
    }

    if ( !( g_cl.m_cmd->buttons & IN_JUMP ) )
    {
        this->m_delayed_hop_landing_ticks = 0;
        return;
    }

    const i32 delay{ math::clamp( g_cfg.m_misc.m_delayed_hop_ticks, 1, 32 ) };
    if ( this->m_delayed_hop_landing_ticks++ < delay )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
    }
}

void misc::null_movement( )
{
    if ( !g_cfg.m_misc.m_null_movement )
    {
        return;
    }

    if ( ( g_cl.m_cmd->buttons & ( IN_FORWARD | IN_BACK ) ) == ( IN_FORWARD | IN_BACK ) )
    {
        g_cl.m_cmd->buttons &= ~( IN_FORWARD | IN_BACK );
        g_cl.m_cmd->forwardmove = 0.0f;
    }
    if ( ( g_cl.m_cmd->buttons & ( IN_MOVELEFT | IN_MOVERIGHT ) ) == ( IN_MOVELEFT | IN_MOVERIGHT ) )
    {
        g_cl.m_cmd->buttons &= ~( IN_MOVELEFT | IN_MOVERIGHT );
        g_cl.m_cmd->sidemove = 0.0f;
    }
}

void misc::mini_jump( )
{
    if ( this->m_mini_jump_release )
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
        this->m_mini_jump_release = false;
    }

    if ( this->m_mini_jump_holding )
    {
        if ( g_cl.m_cmd->buttons & IN_JUMP )
        {
            g_cl.m_cmd->buttons |= IN_DUCK;
        } else
        {
            g_cl.m_cmd->buttons &= ~IN_DUCK;
            this->m_mini_jump_holding = false;
        }
    }

    if ( !g_cfg.m_misc.m_mini_jump || !movement_bind_active( g_cfg.m_misc.m_mini_jump_key ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->IsDucked( ) )
    {
        return;
    }


    const bool grounded_jump{ g_cl.m_local->IsOnGround( ) && ( g_cl.m_cmd->buttons & IN_JUMP ) };
    const bool edge_jump{ g_cfg.m_misc.m_mini_jump_edge && this->m_edge_grounded &&
                          !g_cl.m_local->IsOnGround( ) };
    if ( !grounded_jump && !edge_jump )
    {
        return;
    }

    // Start the duck transition on the takeoff command, then release it on the first air
    // command.  This produces the short TF2 mini-jump without completing an airborne crouch.
    g_cl.m_cmd->buttons |= IN_DUCK | ( edge_jump ? IN_JUMP : 0 );
    if ( g_cfg.m_misc.m_mini_jump_hold_duck )
    {
        this->m_mini_jump_holding = true;
    } else
    {
        this->m_mini_jump_release = true;
    }
}

void misc::long_jump( )
{
    if ( !g_cfg.m_misc.m_long_jump || !movement_bind_active( g_cfg.m_misc.m_long_jump_key ) ||
         !this->m_edge_grounded || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK )
    {
        return;
    }

    g_cl.m_cmd->buttons |= IN_JUMP | IN_DUCK;
    this->m_long_jump_duck_ticks = g_cfg.m_misc.m_long_jump_adaptive
                                      ? math::clamp( g_cfg.m_misc.m_long_jump_release_ticks, 1, 64 )
                                      : 1;
}

void misc::auto_crouch( )
{
    if ( g_cfg.m_misc.m_long_jump_adaptive && this->m_long_jump_duck_ticks > 0 &&
         ( g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_vecVelocity( ).z <= 0.0f ) )
    {
        this->m_long_jump_duck_ticks = 0;
        g_cl.m_cmd->buttons &= ~IN_DUCK;
    }

    if ( this->m_long_jump_duck_ticks > 0 )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
        --this->m_long_jump_duck_ticks;
    }

    if ( !g_cfg.m_misc.m_auto_crouch || !movement_bind_active( g_cfg.m_misc.m_auto_crouch_key ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_vecVelocity( ).z >= 0.0f )
    {
        return;
    }

    const i32 ticks{ math::clamp( g_cfg.m_misc.m_auto_crouch_ticks, 1, 32 ) };
    const f32 time{ TICKS_TO_TIME( ticks ) };
    const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) *
                       ( g_cl.m_local->m_flGravity( ) != 0.0f ? g_cl.m_local->m_flGravity( ) : 1.0f ) };
    const f32 fall{ -g_cl.m_local->m_vecVelocity( ).z * time + 0.5f * gravity * time * time };

    Ray_t              ray{ };
    trace_t            trace{ };
    CTraceFilterSimple filter{ g_cl.m_local, COLLISION_GROUP_PLAYER_MOVEMENT };
    const vec3         origin{ g_cl.m_local->GetAbsOrigin( ) };
    ray.Init( origin, origin - vec3( 0.0f, 0.0f, fall ), g_cl.m_local->m_vecMins( ), g_cl.m_local->m_vecMaxs( ) );
    g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );
    if ( trace.fraction < 1.0f && !trace.startsolid )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    }
}

void misc::jump_stats( )
{
    if ( !g_cfg.m_misc.m_jump_stats )
    {
        this->m_stats_active   = false;
        this->m_stats_grounded = g_cl.m_local->IsOnGround( );
        return;
    }

    const bool grounded{ g_cl.m_local->IsOnGround( ) };
    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    const f32  speed{ g_cl.m_local->m_vecVelocity( ).length2d( ) };
    if ( this->m_stats_grounded && !grounded )
    {
        this->m_stats_active      = true;
        this->m_stats_start       = origin;
        this->m_stats_start_speed = this->m_stats_max_speed = speed;
        this->m_stats_max_height                            = 0.0f;
        this->m_stats_strafes                               = 0;
        this->m_stats_last_side                             = 0;
    } else if ( this->m_stats_active && !grounded )
    {
        this->m_stats_max_speed  = math::bigger( this->m_stats_max_speed, speed );
        this->m_stats_max_height = math::bigger( this->m_stats_max_height, origin.z - this->m_stats_start.z );
        const i32 side{ g_cl.m_cmd->sidemove > 1.0f ? 1 : g_cl.m_cmd->sidemove < -1.0f ? -1
                                                                                       : 0 };
        if ( side && side != this->m_stats_last_side )
        {
            ++this->m_stats_strafes;
            this->m_stats_last_side = side;
        }
    } else if ( this->m_stats_active && grounded )
    {
        const f32 distance{ ( origin - this->m_stats_start ).length2d( ) };
        char      message[ 192 ]{ };
        str::format_to( message, sizeof( message ),
                        ATLAS_XORSTR( "Distance {:.1f}u | pre {:.0f} | max {:.0f} | height {:.1f} | strafes {}" ).get( ),
                        distance, this->m_stats_start_speed, this->m_stats_max_speed,
                        this->m_stats_max_height, this->m_stats_strafes );
        push_notification( notify_kind_t::information, ICON_USER, ATLAS_XORSTR_STATIC( "Jump stats" ), message );
        this->m_stats_active = false;
    }
    this->m_stats_grounded = grounded;
}

void misc::edge_jump( )
{
    if ( !g_cfg.m_misc.m_edge_jump )
    {
        return;
    }

    const bind_t &key{ g_cfg.m_misc.m_edge_jump_key };
    const bool    gated{ key.m_key != 0 || key.m_mode == bind_mode_t::always };

    if ( gated && !gui::bind_active( g_cfg.m_misc.m_edge_jump_key ) )
    {
        return;
    }

    if ( !this->m_edge_grounded || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    g_cl.m_cmd->buttons |= IN_JUMP;
}

void misc::fan_jump( )
{
    if ( !g_cfg.m_misc.m_fan_jump || g_gui.open( ) || g_recorder.replaying( ) ||
         !g_cl.has_live_weapon_command( ) )
    {
        return;
    }

    if ( !gui::bind_active( g_cfg.m_misc.m_fan_jump_key ) )
    {
        return;
    }

    C_TFPlayer     *local{ g_cl.m_local };
    C_TFWeaponBase *weapon{ g_cl.m_weapon };

    if ( local->m_MoveType( ) != MOVETYPE_WALK || local->m_nWaterLevel( ) > 1 ||
         local->m_bScattergunJump( ) )
    {
        return;
    }

    if ( g_cl.attrib_hook( weapon, 0.0f, ATLAS_XORSTR( "set_scattergun_has_knockback" ).get( ) ) != 1.0f )
    {
        return;
    }

    const f32 time{ TICKS_TO_TIME( local->m_nTickBase( ) ) };

    if ( weapon->m_iClip1( ) <= 0 || weapon->m_flNextPrimaryAttack( ) > time ||
         !g_cl.weapon_attack_allowed( time ) )
    {
        return;
    }

    const vec3 velocity{ local->m_vecVelocity( ) };
    f32        rise{ velocity.z };

    if ( local->IsOnGround( ) )
    {
        if ( local->m_nButtons( ) & IN_JUMP )
        {
            g_cl.m_cmd->buttons &= ~IN_JUMP;
            return;
        }

        if ( local->IsDucked( ) )
        {
            return;
        }

        const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) * ( local->m_flGravity( ) != 0.0f ? local->m_flGravity( ) : 1.0f ) };
        const f32 jump_speed{ JUMP_SPEED * g_cl.attrib_hook( weapon,
                                                             g_cl.attrib_hook( local, 1.0f, ATLAS_XORSTR( "mod_jump_height" ).get( ) ),
                                                             ATLAS_XORSTR( "mod_jump_height_from_weapon" ).get( ) ) };

        g_cl.m_cmd->buttons |= IN_JUMP;
        rise = jump_speed - gravity * TICK_INTERVAL;
    }

    const f32 speed{ velocity.length2d( ) };
    const f32 yaw{ speed > 0.0f ? RAD2DEG( math::atan2( velocity.y, velocity.x ) ) : g_cl.m_cmd->viewangles.y };

    g_cl.m_cmd->viewangles = vec3{ fan_jump_pitch( speed, rise ), yaw, 0.0f };
    g_cl.m_cmd->buttons |= IN_ATTACK;
    g_cl.m_psilent_choked = true;
}

void misc::fast_stop( )
{
    if ( !g_cfg.m_misc.m_fast_stop )
    {
        return;
    }

    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT };
    if ( g_cl.m_cmd->buttons & ( MOVE_BUTTONS | IN_JUMP ) )
    {
        return;
    }

    if ( g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || ( !g_cl.m_local->IsOnGround( ) && !g_cfg.m_misc.m_air_stop ) ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;

    const float speed{ velocity.length2d( ) };
    const float surface{ g_cl.m_local->m_surfaceFriction( ) };
    const float friction{ g_tf2.sv_friction->GetFloat( ) * surface };
    const float accelerate{ g_tf2.sv_accelerate->GetFloat( ) };
    const float remaining{ speed - math::bigger( speed, g_tf2.sv_stopspeed->GetFloat( ) ) * friction * TICK_INTERVAL };
    const float brake{ remaining / ( accelerate * TICK_INTERVAL * surface ) };

    if ( brake < 100.0f * friction / accelerate )
    {
        g_cl.m_cmd->forwardmove = 0.0f;
        g_cl.m_cmd->sidemove    = 0.0f;
        return;
    }

    const float crop{ g_cl.m_local->IsDucked( ) ? 1.0f / 3.0f : 1.0f };
    const float push{ math::smaller( brake / crop, g_cl.m_local->m_flMaxspeed( ) ) };
    const float velocity_yaw{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
    const float push_delta{ DEG2RAD( math::normalize_yaw( velocity_yaw + 180.0f -
                                                          g_cl.m_cmd->viewangles.y ) ) };

    g_cl.m_cmd->forwardmove = push * math::cos( push_delta );
    g_cl.m_cmd->sidemove    = -push * math::sin( push_delta );
}

void misc::fast_accel( )
{
    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT };
    if ( !g_cfg.m_misc.m_fast_accel || !g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->IsDucked( ) || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_nWaterLevel( ) > 1 || g_cl.m_cmd->buttons & IN_ATTACK ||
         !( g_cl.m_cmd->buttons & MOVE_BUTTONS ) || ( g_cl.m_cmd->tick_count & 1 ) )
    {
        return;
    }

    const vec3 move{ g_cl.m_cmd->forwardmove, g_cl.m_cmd->sidemove, 0.0f };
    const f32 magnitude{ move.length2d( ) };
    if ( magnitude < 1.0f )
    {
        return;
    }
    if ( !this->claim_movement( movement_owner_t::fast_accel ) )
    {
        return;
    }

    const f32 reverse_yaw{ RAD2DEG( math::atan2( -move.y, -move.x ) ) };
    g_cl.m_cmd->forwardmove = -magnitude;
    g_cl.m_cmd->sidemove    = 0.0f;
    g_cl.m_cmd->viewangles.y = math::normalize_yaw( g_cl.m_cmd->viewangles.y - reverse_yaw );
    g_cl.m_cmd->viewangles.z = 270.0f;
    g_cl.m_psilent_choked = true;
}

void misc::blockbot( )
{
    if ( !g_cfg.m_misc.m_blockbot || !movement_bind_active( g_cfg.m_misc.m_blockbot_key ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK )
    {
        return;
    }

    C_TFPlayer *best{ };
    f32 best_distance{ FLT_MAX };
    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    const i32 local_team{ g_cl.m_local->m_iTeamNum( ) };
    for ( i32 index{ 1 }; index <= g_tf2.m_engine->GetMaxClients( ); ++index )
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_TFPlayer *player{ client ? client->GetBaseEntity( )->As< C_TFPlayer * >( ) : nullptr };
        if ( !player || player == g_cl.m_local || player->IsDormant( ) || !player->IsAlive( ) ||
             ( !g_cfg.m_misc.m_blockbot_teammates && player->m_iTeamNum( ) == local_team ) )
        {
            continue;
        }

        const f32 distance{ origin.dist_to_sqr( player->GetAbsOrigin( ) ) };
        if ( distance < best_distance )
        {
            best_distance = distance;
            best = player;
        }
    }

    if ( !best )
    {
        return;
    }
    if ( !this->claim_movement( movement_owner_t::blockbot ) )
    {
        return;
    }

    const vec3 target_origin{ best->GetAbsOrigin( ) };
    const vec3 target_velocity{ best->m_vecVelocity( ) };
    if ( target_origin.z < origin.z && best_distance < 100.0f * 100.0f )
    {
        const vec3 wish{ target_velocity * 2.0f - g_cl.m_local->m_vecVelocity( ) };
        const f32 yaw{ DEG2RAD( g_cl.m_cmd->viewangles.y ) };
        g_cl.m_cmd->forwardmove = math::clamp( math::cos( yaw ) * wish.x + math::sin( yaw ) * wish.y, -450.0f, 450.0f );
        g_cl.m_cmd->sidemove = math::clamp( math::sin( yaw ) * wish.x - math::cos( yaw ) * wish.y, -450.0f, 450.0f );
        return;
    }

    const vec3 delta{ target_origin - origin };
    const f32 yaw_delta{ math::normalize_yaw( RAD2DEG( math::atan2( delta.y, delta.x ) ) - g_cl.m_cmd->viewangles.y ) };
    if ( math::abs( yaw_delta ) > 0.25f )
    {
        g_cl.m_cmd->sidemove = ( yaw_delta > 0.0f ? -1.0f : 1.0f ) *
                               math::clamp( target_velocity.length2d( ), 50.0f, 450.0f );
    }
}

void misc::auto_bounce( )
{
    if ( !g_cfg.m_misc.m_auto_bounce ||
         !movement_bind_active( g_cfg.m_misc.m_auto_bounce_key ) ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_nWaterLevel( ) > 1 || g_cl.m_local->m_vecVelocity( ).z >= 0.0f )
    {
        return;
    }

    const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) *
                       ( g_cl.m_local->m_flGravity( ) != 0.0f ? g_cl.m_local->m_flGravity( ) : 1.0f ) };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_auto_bounce_reach, 16.0f, 512.0f ) };
    const vec3 mins{ g_cl.m_local->m_vecMins( ) };
    const vec3 maxs{ g_cl.m_local->m_vecMaxs( ) };
    CTraceFilterSimple filter{ g_cl.m_local, COLLISION_GROUP_PLAYER_MOVEMENT };

    vec3 position{ g_cl.m_local->GetAbsOrigin( ) };
    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    f32 travelled{ };
    trace_t contact{ };
    i32 contact_tick{ };
    for ( i32 tick{ 1 }; tick <= 48 && travelled < reach; ++tick )
    {
        velocity.z -= gravity * TICK_INTERVAL;
        const vec3 next{ position + velocity * TICK_INTERVAL };
        Ray_t ray{ };
        ray.Init( position, next, mins, maxs );
        g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &contact );
        travelled += position.dist_to( contact.endpos );
        position = contact.endpos;

        if ( contact.fraction < 1.0f )
        {
            const f32 normal_z{ contact.plane.normal.z };
            if ( !contact.startsolid && normal_z > 0.05f && normal_z < 0.7f )
            {
                contact_tick = tick;
            }
            break;
        }
    }

    if ( contact_tick == 0 )
    {
        return;
    }

    const f32 into{ velocity.dot( contact.plane.normal ) };
    if ( into >= 0.0f )
    {
        return;
    }

    const vec3 exit{ velocity - contact.plane.normal * into };
    const f32 gain{ exit.length2d( ) - g_cl.m_local->m_vecVelocity( ).length2d( ) };
    if ( gain < g_cfg.m_misc.m_auto_bounce_min_gain )
    {
        return;
    }
    if ( !this->claim_movement( movement_owner_t::auto_bounce ) )
    {
        return;
    }
    this->m_movement_debug_point = contact.endpos;
    this->m_movement_debug_normal = contact.plane.normal;
    this->m_movement_debug_valid = true;

    const usize mode{ math::clamp< usize >( g_cfg.m_misc.m_auto_bounce_mode, 0, 2 ) };
    if ( mode != 0 )
    {
        vec3 wish{ mode == 1 ? exit : -g_cl.m_local->m_vecVelocity( ) };
        wish.z = 0.0f;
        if ( wish.normalize( ) > 0.0f )
        {
            wish *= g_cl.m_local->m_flMaxspeed( );
            vec3 forward{ }, right{ };
            math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
            g_cl.m_cmd->forwardmove = forward.dot( wish );
            g_cl.m_cmd->sidemove = right.dot( wish );
        }
    }

    if ( contact_tick <= 2 )
    {
        if ( g_cfg.m_misc.m_auto_bounce_jump )
        {
            g_cl.m_cmd->buttons |= IN_JUMP;
        }
        if ( g_cfg.m_misc.m_auto_bounce_crouch )
        {
            g_cl.m_cmd->buttons |= IN_DUCK;
        }
    }
}

void misc::goomba( )
{
    if ( !g_cfg.m_misc.m_goomba || !movement_bind_active( g_cfg.m_misc.m_goomba_key ) ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_vecVelocity( ).z >= -1.0f )
    {
        return;
    }

    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_goomba_reach, 32.0f, 400.0f ) };
    const f32 gravity{ g_tf2.sv_gravity->GetFloat( ) *
                       ( g_cl.m_local->m_flGravity( ) != 0.0f ? g_cl.m_local->m_flGravity( ) : 1.0f ) };
    C_TFPlayer *best{ };
    vec3 best_point{ };
    f32 best_distance{ FLT_MAX };
    const i32 local_team{ g_cl.m_local->m_iTeamNum( ) };

    for ( i32 index{ 1 }; index <= g_tf2.m_engine->GetMaxClients( ); ++index )
    {
        IClientEntity *client{ g_tf2.m_entity_list->GetClientEntity( index ) };
        C_TFPlayer *player{ client ? client->GetBaseEntity( )->As< C_TFPlayer * >( ) : nullptr };
        if ( !player || player == g_cl.m_local || player->IsDormant( ) || !player->IsAlive( ) ||
             ( !g_cfg.m_misc.m_goomba_teammates && player->m_iTeamNum( ) == local_team ) ||
             player->GetAbsOrigin( ).z >= origin.z )
        {
            continue;
        }

        const f32 drop{ origin.z - player->GetAbsOrigin( ).z };
        const f32 vz{ g_cl.m_local->m_vecVelocity( ).z };
        const f32 discriminant{ vz * vz + 2.0f * gravity * drop };
        if ( discriminant < 0.0f )
        {
            continue;
        }
        const f32 time{ ( vz + math::sqrt( discriminant ) ) / gravity +
                        TICKS_TO_TIME( math::clamp( g_cfg.m_misc.m_goomba_lead_ticks, -8, 16 ) ) };
        if ( time <= 0.0f || time > 1.5f )
        {
            continue;
        }

        const vec3 predicted{ player->GetAbsOrigin( ) + player->m_vecVelocity( ) * time };
        const vec3 delta{ predicted - origin };
        const f32 distance{ delta.length2d( ) };
        if ( distance <= reach && distance < best_distance )
        {
            best = player;
            best_point = predicted;
            best_distance = distance;
        }
    }

    if ( !best || !this->claim_movement( movement_owner_t::goomba ) )
    {
        return;
    }

    vec3 wish{ best_point - origin };
    wish.z = 0.0f;
    if ( wish.normalize( ) <= 0.0f )
    {
        return;
    }
    wish *= g_cl.m_local->m_flMaxspeed( );
    vec3 forward{ }, right{ };
    math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
    g_cl.m_cmd->forwardmove = forward.dot( wish );
    g_cl.m_cmd->sidemove = right.dot( wish );

    this->m_movement_debug_point = best_point;
    this->m_movement_debug_normal = vec3( 0.0f, 0.0f, 1.0f );
    this->m_movement_debug_valid = true;
}

void misc::wall_climb( )
{
    if ( this->m_wall_climb_jump_release )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
        this->m_wall_climb_jump_release = false;
    }

    if ( !g_cfg.m_misc.m_wall_climb ||
         !movement_bind_active( g_cfg.m_misc.m_wall_climb_key ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    movement_surface_t wall{ };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_wall_climb_reach, 2.0f, 48.0f ) };
    if ( !movement_surface_scanner::nearest_wall( g_cl.m_local, reach, wall ) )
    {
        return;
    }
    if ( !this->claim_movement( movement_owner_t::wall_climb ) )
    {
        return;
    }
    this->m_movement_debug_point = wall.m_point;
    this->m_movement_debug_normal = wall.m_normal;
    this->m_movement_debug_valid = true;

    if ( g_cl.m_local->IsOnGround( ) )
    {
        if ( ( g_cl.m_cmd->buttons & IN_JUMP ) &&
             ( g_cl.m_local->m_nButtons( ) & IN_JUMP ) )
        {
            g_cl.m_cmd->buttons &= ~IN_JUMP;
            this->m_wall_climb_jump_release = true;
        }
        return;
    }

    const vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    vec3 tangent{ -wall.m_normal.y, wall.m_normal.x, 0.0f };
    if ( tangent.dot( velocity ) < 0.0f )
    {
        tangent = -tangent;
    }

    vec3 wish{ -wall.m_normal * g_cl.m_local->m_flMaxspeed( ) };
    if ( g_cfg.m_misc.m_wall_climb_prevent_slow && velocity.length2d( ) > 200.0f )
    {
        wish += tangent * math::abs( tangent.dot( velocity ) );
    }
    if ( g_cfg.m_misc.m_wall_climb_seam_steer )
    {
        wish += tangent * ( 30.0f * math::clamp( g_cfg.m_misc.m_wall_climb_steer_scale, 0.25f, 2.0f ) );
    }

    wish.z = 0.0f;
    if ( wish.normalize( ) <= 0.0f )
    {
        return;
    }
    wish *= g_cl.m_local->m_flMaxspeed( );

    vec3 forward{ }, right{ };
    math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
    g_cl.m_cmd->forwardmove = forward.dot( wish );
    g_cl.m_cmd->sidemove = right.dot( wish );

    if ( g_cfg.m_misc.m_wall_climb_duck )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    }
}

void misc::pixel_surf( )
{
    if ( !g_cfg.m_misc.m_pixel_surf ||
         !movement_bind_active( g_cfg.m_misc.m_pixel_surf_key ) ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        this->m_pixel_surf_hold = 0;
        this->m_pixel_surf_park_ticks = 0;
        return;
    }

    movement_surface_t surface{ };
    i32 contact_tick{ };
    const i32 predict{ math::clamp( g_cfg.m_misc.m_pixel_surf_predict_ticks, 1, 32 ) };
    bool acquired{ movement_surface_scanner::first_wall_on_path( g_cl.m_local, predict,
                                                                 surface, contact_tick ) };
    if ( acquired )
    {
        const vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
        const f32 into{ velocity.dot( surface.m_normal ) };
        const f32 clipped_z{ velocity.z - surface.m_normal.z * into };
        acquired = into < 0.0f && math::abs( surface.m_normal.z ) > 0.01f &&
                   clipped_z > velocity.z;
    }

    if ( acquired )
    {
        this->m_pixel_surf_hold = contact_tick + 3;
        this->m_pixel_surf_park_ticks = 0;
        this->m_pixel_surf_normal = surface.m_normal;
        this->m_movement_debug_point = surface.m_point;
        this->m_movement_debug_normal = surface.m_normal;
        this->m_movement_debug_valid = true;
    } else if ( this->m_pixel_surf_hold > 0 )
    {
        --this->m_pixel_surf_hold;
        movement_surface_t nearby{ };
        if ( movement_surface_scanner::nearest_wall( g_cl.m_local, 12.0f, nearby ) )
        {
            this->m_pixel_surf_normal = nearby.m_normal;
        }
        if ( this->m_pixel_surf_hold == 0 && g_cfg.m_misc.m_pixel_surf_park )
        {
            this->m_pixel_surf_park_ticks = 4;
        }
    } else
    {
        if ( this->m_pixel_surf_park_ticks > 0 )
        {
            if ( !this->claim_movement( movement_owner_t::pixel_surf ) )
            {
                return;
            }
            --this->m_pixel_surf_park_ticks;
            vec3 brake{ -g_cl.m_local->m_vecVelocity( ).x,
                        -g_cl.m_local->m_vecVelocity( ).y, 0.0f };
            if ( brake.normalize( ) > 0.0f )
            {
                brake *= g_cl.m_local->m_flMaxspeed( );
                vec3 forward{ }, right{ };
                math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
                g_cl.m_cmd->forwardmove = forward.dot( brake );
                g_cl.m_cmd->sidemove = right.dot( brake );
            }
        }
        return;
    }

    if ( !this->claim_movement( movement_owner_t::pixel_surf ) )
    {
        return;
    }

    vec3 wish{ -this->m_pixel_surf_normal * g_cl.m_local->m_flMaxspeed( ) };
    const usize wall_strafe{ math::clamp< usize >( g_cfg.m_misc.m_pixel_surf_wall_strafe, 0, 2 ) };
    if ( wall_strafe != 0 && ( wall_strafe == 2 || this->m_pixel_surf_hold <= 3 ) )
    {
        vec3 tangent{ -this->m_pixel_surf_normal.y, this->m_pixel_surf_normal.x, 0.0f };
        if ( tangent.dot( g_cl.m_local->m_vecVelocity( ) ) < 0.0f )
        {
            tangent = -tangent;
        }
        wish += tangent * g_cl.m_local->m_vecVelocity( ).length2d( );
    }

    wish.z = 0.0f;
    if ( wish.normalize( ) > 0.0f )
    {
        wish *= g_cl.m_local->m_flMaxspeed( );
        vec3 forward{ }, right{ };
        math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
        g_cl.m_cmd->forwardmove = forward.dot( wish );
        g_cl.m_cmd->sidemove = right.dot( wish );
    }
}

void misc::texture_bug( )
{
    if ( !g_cfg.m_misc.m_texture_bug ||
         !movement_bind_active( g_cfg.m_misc.m_texture_bug_key ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        this->m_texture_bug_hold = 0;
        return;
    }

    movement_surface_t seam{ };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_texture_bug_reach, 1.0f, 48.0f ) };
    if ( movement_surface_scanner::nearest_seam( g_cl.m_local, reach, seam ) )
    {
        this->m_texture_bug_hold = 4;
        this->m_texture_bug_normal = seam.m_normal;
        this->m_movement_debug_point = seam.m_point;
        this->m_movement_debug_normal = seam.m_normal;
        this->m_movement_debug_valid = true;
    } else if ( this->m_texture_bug_hold > 0 )
    {
        --this->m_texture_bug_hold;
    } else
    {
        return;
    }
    if ( !this->claim_movement( movement_owner_t::texture_bug ) )
    {
        return;
    }

    vec3 wish{ -this->m_texture_bug_normal * g_cl.m_local->m_flMaxspeed( ) };
    if ( g_cfg.m_misc.m_texture_bug_wall_strafe )
    {
        vec3 tangent{ -this->m_texture_bug_normal.y, this->m_texture_bug_normal.x, 0.0f };
        if ( tangent.dot( g_cl.m_local->m_vecVelocity( ) ) < 0.0f )
        {
            tangent = -tangent;
        }
        wish += tangent * g_cl.m_local->m_vecVelocity( ).length2d( );
    }

    wish.z = 0.0f;
    if ( wish.normalize( ) > 0.0f )
    {
        wish *= g_cl.m_local->m_flMaxspeed( );
        vec3 forward{ }, right{ };
        math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
        g_cl.m_cmd->forwardmove = forward.dot( wish );
        g_cl.m_cmd->sidemove = right.dot( wish );
    }

    const usize stance{ math::clamp< usize >( g_cfg.m_misc.m_texture_bug_stance, 0, 2 ) };
    if ( stance == 1 || ( stance == 0 && g_cfg.m_misc.m_texture_bug_auto_duck ) )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    } else if ( stance == 2 )
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
    }
}

void misc::air_stuck( )
{
    if ( !g_cfg.m_misc.m_air_stuck || !movement_bind_active( g_cfg.m_misc.m_air_stuck_key ) ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        this->m_air_stuck_hold = 0;
        return;
    }

    movement_surface_t surface{ };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_air_stuck_reach, 1.0f, 32.0f ) };
    if ( movement_surface_scanner::nearest_seam( g_cl.m_local, reach, surface ) ||
         movement_surface_scanner::nearest_wall( g_cl.m_local, reach, surface ) )
    {
        this->m_air_stuck_hold = 3;
        this->m_air_stuck_normal = surface.m_normal;
        this->m_movement_debug_point = surface.m_point;
        this->m_movement_debug_normal = surface.m_normal;
        this->m_movement_debug_valid = true;
    } else if ( this->m_air_stuck_hold > 0 )
    {
        --this->m_air_stuck_hold;
    } else
    {
        return;
    }

    if ( !this->claim_movement( movement_owner_t::air_stuck ) )
    {
        return;
    }

    vec3 tangent{ -this->m_air_stuck_normal.y, this->m_air_stuck_normal.x, 0.0f };
    const f32 along{ tangent.dot( g_cl.m_local->m_vecVelocity( ) ) };
    vec3 wish{ -this->m_air_stuck_normal * g_cl.m_local->m_flMaxspeed( ) - tangent * along };
    wish.z = 0.0f;
    if ( wish.normalize( ) > 0.0f )
    {
        wish *= g_cl.m_local->m_flMaxspeed( );
        vec3 forward{ }, right{ };
        math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &forward, &right );
        g_cl.m_cmd->forwardmove = forward.dot( wish );
        g_cl.m_cmd->sidemove = right.dot( wish );
    }
    if ( g_cfg.m_misc.m_air_stuck_crouch )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    }
}

void misc::movement_finder( )
{
    if ( !g_cfg.m_misc.m_movement_finder || !g_cl.m_local || !g_cl.m_local->IsAlive( ) )
    {
        return;
    }
    if ( this->m_lab_scan_cooldown-- > 0 )
    {
        return;
    }
    this->m_lab_scan_cooldown = 3;

    const auto add_point{ [ this ]( const vec3 &position, const vec3 &normal, u8 kind ) {
        const f32 radius{ math::clamp( g_cfg.m_misc.m_movement_finder_radius, 4.0f, 128.0f ) };
        for ( usize index{ }; index < this->m_lab_point_count; ++index )
        {
            if ( this->m_lab_points[ index ].m_position.dist_to_sqr( position ) <= radius * radius )
            {
                return;
            }
        }

        usize slot{ this->m_lab_point_count };
        if ( slot >= 64 )
        {
            slot = 0;
            for ( usize index{ 1 }; index < 64; ++index )
            {
                if ( this->m_lab_points[ index ].m_position.dist_to_sqr( g_cl.m_local->GetAbsOrigin( ) ) >
                     this->m_lab_points[ slot ].m_position.dist_to_sqr( g_cl.m_local->GetAbsOrigin( ) ) )
                {
                    slot = index;
                }
            }
        } else
        {
            ++this->m_lab_point_count;
        }
        this->m_lab_points[ slot ] = movement_lab_point_t{ position, normal, kind };
        this->m_movement_lab_dirty = true;
    } };

    movement_surface_t surface{ };
    if ( movement_surface_scanner::nearest_seam( g_cl.m_local, 32.0f, surface ) )
    {
        add_point( surface.m_point, surface.m_normal, 1 );
    } else if ( movement_surface_scanner::nearest_wall( g_cl.m_local, 24.0f, surface ) )
    {
        add_point( surface.m_point, surface.m_normal, 0 );
    }

    movement_lip_t lip{ };
    if ( movement_surface_scanner::nearest_lip( g_cl.m_local, 4.0f, lip ) )
    {
        add_point( lip.m_point, lip.m_direction, 2 );
    }
}

void misc::movement_lab_hotkeys( )
{
    const auto pressed{ []( bind_t &bind, bool &held ) {
        const bool active{ bind.m_key != 0 && gui::bind_active( bind ) };
        const bool edge{ active && !held };
        held = active;
        return edge;
    } };

    if ( pressed( g_cfg.m_misc.m_checkpoint_save_key, this->m_checkpoint_save_held ) )
    {
        this->m_checkpoint_origin = g_cl.m_local->GetAbsOrigin( );
        this->m_checkpoint_angles = g_cl.m_cmd->viewangles;
        this->m_checkpoint_valid = true;
        this->m_movement_lab_dirty = true;
        push_notification( notify_kind_t::success, ICON_USER, ATLAS_XORSTR_STATIC( "MoveLab" ),
                           ATLAS_XORSTR_STATIC( "Practice checkpoint saved" ) );
    }

    if ( pressed( g_cfg.m_misc.m_checkpoint_load_key, this->m_checkpoint_load_held ) &&
         this->m_checkpoint_valid )
    {
        CNetChannel *channel{ g_tf2.m_engine->GetNetChannelInfo( ) };
        if ( channel && channel->IsLoopback( ) )
        {
            char command[ 192 ]{ };
            str::format_to( command, sizeof( command ),
                            ATLAS_XORSTR( "setpos_exact {:.3f} {:.3f} {:.3f}; setang_exact {:.3f} {:.3f} {:.3f}" ).get( ),
                            this->m_checkpoint_origin.x, this->m_checkpoint_origin.y, this->m_checkpoint_origin.z,
                            this->m_checkpoint_angles.x, this->m_checkpoint_angles.y, this->m_checkpoint_angles.z );
            g_tf2.m_engine->ClientCmd_Unrestricted( command );
        } else
        {
            push_notification( notify_kind_t::warning, ICON_USER, ATLAS_XORSTR_STATIC( "MoveLab" ),
                               ATLAS_XORSTR_STATIC( "Checkpoint restore requires a local server" ) );
        }
    }

    vec3 forward{ };
    math::angle_vectors( g_cl.m_cmd->viewangles, &forward );
    const vec3 eye{ g_cl.m_local->GetAbsOrigin( ) + g_cl.m_local->m_vecViewOffset( ) };
    Ray_t ray{ };
    trace_t trace{ };
    CTraceFilterSimple filter{ g_cl.m_local, COLLISION_GROUP_NONE };
    ray.Init( eye, eye + forward * 8192.0f );
    g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );

    if ( pressed( g_cfg.m_misc.m_route_add_key, this->m_route_add_held ) &&
         trace.fraction < 1.0f && this->m_route_point_count < 32 )
    {
        this->m_route_points[ this->m_route_point_count++ ] = trace.endpos + trace.plane.normal * 2.0f;
        this->m_movement_lab_dirty = true;
    }
    if ( pressed( g_cfg.m_misc.m_route_delete_key, this->m_route_delete_held ) &&
         this->m_route_point_count > 0 )
    {
        usize nearest{ };
        f32 distance{ FLT_MAX };
        for ( usize index{ }; index < this->m_route_point_count; ++index )
        {
            const f32 candidate{ this->m_route_points[ index ].dist_to_sqr( trace.endpos ) };
            if ( candidate < distance )
            {
                distance = candidate;
                nearest = index;
            }
        }
        for ( usize index{ nearest + 1 }; index < this->m_route_point_count; ++index )
        {
            this->m_route_points[ index - 1 ] = this->m_route_points[ index ];
        }
        --this->m_route_point_count;
        this->m_movement_lab_dirty = true;
        this->m_route_index = math::smaller( this->m_route_index, this->m_route_point_count );
    }
    if ( pressed( g_cfg.m_misc.m_route_clear_key, this->m_route_clear_held ) )
    {
        this->m_route_point_count = 0;
        this->m_route_index = 0;
        this->m_movement_lab_dirty = true;
    }
}

void misc::route_follow( )
{
    if ( !g_cfg.m_misc.m_route_follow ||
         !movement_bind_active( g_cfg.m_misc.m_route_follow_key ) ||
         this->m_route_point_count == 0 || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK )
    {
        return;
    }

    this->m_route_index = math::smaller( this->m_route_index, this->m_route_point_count - 1 );
    const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_route_reach, 8.0f, 128.0f ) };
    if ( origin.dist_to_sqr( this->m_route_points[ this->m_route_index ] ) <= reach * reach )
    {
        if ( this->m_route_index + 1 >= this->m_route_point_count )
        {
            return;
        }
        ++this->m_route_index;
    }

    if ( !this->claim_movement( movement_owner_t::route ) )
    {
        return;
    }
    vec3 wish{ this->m_route_points[ this->m_route_index ] - origin };
    wish.z = 0.0f;
    if ( wish.normalize( ) > 0.0f )
    {
        wish *= g_cl.m_local->m_flMaxspeed( );
        vec3 cmd_forward{ }, cmd_right{ };
        math::angle_vectors( vec3{ 0.0f, g_cl.m_cmd->viewangles.y, 0.0f }, &cmd_forward, &cmd_right );
        g_cl.m_cmd->forwardmove = cmd_forward.dot( wish );
        g_cl.m_cmd->sidemove = cmd_right.dot( wish );
    }
}

void misc::pixel_jump( )
{
    if ( this->m_pixel_jump_release )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
        this->m_pixel_jump_release = false;
    }

    if ( !g_cfg.m_misc.m_pixel_jump ||
         !movement_bind_active( g_cfg.m_misc.m_pixel_jump_key ) ||
         !g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    movement_lip_t lip{ };
    const f32 reach{ math::clamp( g_cfg.m_misc.m_pixel_jump_reach, 0.5f, 12.0f ) };
    if ( !movement_surface_scanner::nearest_lip( g_cl.m_local, reach, lip ) )
    {
        return;
    }

    vec3 travel{ g_cl.m_local->m_vecVelocity( ) };
    travel.z = 0.0f;
    if ( travel.normalize( ) <= 0.0f || travel.dot( lip.m_direction ) < 0.5f )
    {
        return;
    }
    if ( !this->claim_movement( movement_owner_t::pixel_jump ) )
    {
        return;
    }
    this->m_movement_debug_point = lip.m_point;
    this->m_movement_debug_normal = lip.m_direction;
    this->m_movement_debug_valid = true;

    if ( g_cfg.m_misc.m_pixel_jump_hop && ( g_cl.m_local->m_nButtons( ) & IN_JUMP ) )
    {
        g_cl.m_cmd->buttons &= ~IN_JUMP;
        this->m_pixel_jump_release = true;
    } else
    {
        g_cl.m_cmd->buttons |= IN_JUMP;
    }

    if ( g_cfg.m_misc.m_pixel_jump_crouch )
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
    }
}

void misc::paint( ) const
{
    if ( !g_cfg.m_misc.m_movement_debug || !g_tf2.m_engine->IsInGame( ) ||
         !g_cl.m_local || !g_cl.m_local->IsAlive( ) )
    {
        return;
    }

    static const char *const OWNERS[]{
        ATLAS_XORSTR_STATIC( "NONE" ), ATLAS_XORSTR_STATIC( "FAST ACCEL" ),
        ATLAS_XORSTR_STATIC( "BLOCKBOT" ), ATLAS_XORSTR_STATIC( "ROUTE" ), ATLAS_XORSTR_STATIC( "AUTO BOUNCE" ),
        ATLAS_XORSTR_STATIC( "GOOMBA" ), ATLAS_XORSTR_STATIC( "WALL CLIMB" ), ATLAS_XORSTR_STATIC( "PIXEL SURF" ),
        ATLAS_XORSTR_STATIC( "TEXTURE BUG" ), ATLAS_XORSTR_STATIC( "AIR STUCK" ),
        ATLAS_XORSTR_STATIC( "PIXEL JUMP" ),
    };
    const usize owner{ static_cast< usize >( this->m_movement_owner ) };
    if ( owner > 0 && owner < static_cast< usize >( movement_owner_t::pixel_jump ) + 1 )
    {
        g_draw.text( ATLAS_XORSTR( "ESP Small" ).get( ),
                     point_t{ g_cl.m_screen_width / 2, g_cl.m_screen_height / 2 + 48 },
                     color_t{ 120, 210, 255 }, OWNERS[ owner ], true,
                     render::text_align::center );
    }

    if ( this->m_movement_debug_valid )
    {
        const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) + vec3( 0.0f, 0.0f, 24.0f ) };
        g_cl.draw_3d_line( origin, this->m_movement_debug_point,
                           color_t{ 120, 210, 255 }, false );
        g_cl.draw_3d_line( this->m_movement_debug_point,
                           this->m_movement_debug_point + this->m_movement_debug_normal * 18.0f,
                           color_t{ 255, 170, 80 }, false );
    }

    if ( g_cfg.m_misc.m_movement_finder )
    {
        const vec3 origin{ g_cl.m_local->GetAbsOrigin( ) };
        const f32 maximum{ math::clamp( g_cfg.m_misc.m_movement_finder_draw_distance, 100.0f, 5000.0f ) };
        const color_t colors[]{ color_t{ 90, 220, 140 }, color_t{ 80, 200, 255 }, color_t{ 255, 210, 80 } };
        for ( usize index{ }; index < this->m_lab_point_count; ++index )
        {
            const movement_lab_point_t &point{ this->m_lab_points[ index ] };
            if ( origin.dist_to_sqr( point.m_position ) > maximum * maximum )
            {
                continue;
            }
            const color_t color{ colors[ math::clamp< usize >( point.m_kind, 0, 2 ) ] };
            g_cl.draw_3d_line( point.m_position - vec3( 4.0f, 0.0f, 0.0f ),
                               point.m_position + vec3( 4.0f, 0.0f, 0.0f ), color, false );
            g_cl.draw_3d_line( point.m_position - vec3( 0.0f, 4.0f, 0.0f ),
                               point.m_position + vec3( 0.0f, 4.0f, 0.0f ), color, false );
            g_cl.draw_3d_line( point.m_position, point.m_position + point.m_normal * 12.0f,
                               color, false );
        }
    }

    for ( usize index{ }; index < this->m_route_point_count; ++index )
    {
        const color_t color{ index == this->m_route_index ? color_t{ 255, 210, 80 }
                                                          : color_t{ 180, 120, 255 } };
        const vec3 point{ this->m_route_points[ index ] };
        g_cl.draw_3d_line( point - vec3( 5.0f, 0.0f, 0.0f ), point + vec3( 5.0f, 0.0f, 0.0f ), color, false );
        g_cl.draw_3d_line( point - vec3( 0.0f, 5.0f, 0.0f ), point + vec3( 0.0f, 5.0f, 0.0f ), color, false );
        if ( index > 0 )
        {
            g_cl.draw_3d_line( this->m_route_points[ index - 1 ], point, color_t{ 180, 120, 255 }, false );
        }
    }
}

void misc::fake_duck( )
{
    if ( !g_cfg.m_exploits.m_fake_duck || !gui::bind_active( g_cfg.m_exploits.m_fake_duck_key ) ||
         !g_cl.m_local->IsAlive( ) || !g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK )
    {
        return;
    }

    // Keep the server alternating between the standing and ducked hull while the client
    // remains visually stable.  Leave enough headroom for the networking manager to flush.
    constexpr i32 duck_at{ 7 };
    if ( g_tf2.m_cl->chokedcommands < duck_at )
    {
        g_cl.m_cmd->buttons &= ~IN_DUCK;
        g_cl.m_send_packet = false;
    } else
    {
        g_cl.m_cmd->buttons |= IN_DUCK;
        g_cl.m_send_packet = true;
    }
}

void misc::auto_strafe( )
{
    if ( !g_cfg.m_misc.m_auto_strafe || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    if ( ( g_cl.m_cmd->buttons & IN_JUMP ) && !( g_cl.m_local->m_nButtons( ) & IN_JUMP ) )
    {
        return;
    }

    const bool  follow_wasd{ g_cfg.m_misc.m_auto_strafe_mode == 1 };
    const float forward_move{ g_cl.m_cmd->forwardmove };
    const float side_move{ g_cl.m_cmd->sidemove };

    if ( follow_wasd && forward_move == 0.0f && side_move == 0.0f )
    {
        return;
    }

    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;

    const float speed{ velocity.length2d( ) };
    const float velocity_yaw{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
    const float view_yaw{ g_cl.m_cmd->viewangles.y };

    // ( sv_airaccelerate * wishspeed * frametime )
    const float wish_speed{ math::smaller( 450.f, g_cl.m_local->m_flMaxspeed( ) ) };
    const float max_air_accel{ math::smaller( 30.f, g_tf2.sv_airaccelerate->GetFloat( ) * wish_speed * TICK_INTERVAL ) };

    if ( max_air_accel <= 0.f )
    {
        return;
    }

    const float gain_offset{ RAD2DEG( math::asin( max_air_accel * 0.5f / speed ) ) };
    const float turn_max{ RAD2DEG( math::asin( max_air_accel / speed ) ) };

    float wish_yaw{ view_yaw };
    float turn_limit{ turn_max };

    if ( follow_wasd )
    {
        wish_yaw   = math::normalize_yaw( view_yaw + RAD2DEG( math::atan2( -side_move, forward_move ) ) );
        turn_limit = turn_max * math::clamp( g_cfg.m_misc.m_auto_strafe_retrack / 100.0f, 0.0f, 1.0f );
    }

    this->m_strafe_side = this->m_strafe_side > 0.0f ? -1.0f : 1.0f;

    const float wish_delta{ math::normalize_yaw( wish_yaw - velocity_yaw ) };

    float push_yaw{ };

    if ( math::abs( wish_delta ) > turn_limit )
    {
        push_yaw = velocity_yaw + ( wish_delta > 0.0f ? 1.0f : -1.0f ) * ( 90.0f + turn_limit );
    } else
    {
        push_yaw = wish_yaw + this->m_strafe_side * ( 90.0f - gain_offset );
    }

    const float push_delta{ DEG2RAD( math::normalize_yaw( push_yaw - view_yaw ) ) };

    g_cl.m_cmd->forwardmove = 450.f * math::cos( push_delta );
    g_cl.m_cmd->sidemove    = -450.f * math::sin( push_delta );
}

void misc::half_sideways( )
{
    if ( !g_cfg.m_misc.m_half_sideways ||
         !movement_bind_active( g_cfg.m_misc.m_half_sideways_key ) ||
         g_cfg.m_misc.m_auto_strafe || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    const f32 forward{ g_cl.m_cmd->forwardmove };
    const f32 side{ g_cl.m_cmd->sidemove };
    if ( math::abs( forward ) < 1.0f && math::abs( side ) < 1.0f )
    {
        return;
    }

    f32 direction{ side < 0.0f ? -1.0f : side > 0.0f ? 1.0f
                                                       : g_cl.m_cmd->mousedx < 0 ? -1.0f : 1.0f };
    const f32 magnitude{ math::smaller( 450.0f, math::sqrt( forward * forward + side * side ) ) };
    const f32 diagonal{ magnitude * 0.70710678f };
    g_cl.m_cmd->forwardmove = forward < 0.0f ? -diagonal : diagonal;
    g_cl.m_cmd->sidemove    = diagonal * direction;
}

void misc::double_jump_strafe( )
{
    if ( !g_cfg.m_misc.m_double_jump_strafe ||
         !movement_bind_active( g_cfg.m_misc.m_double_jump_strafe_key ) ||
         g_cl.m_local->m_iClass( ) != TF_CLASS_SCOUT || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->m_iAirDash( ) <= 0 )
    {
        return;
    }

    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;
    if ( velocity.length2d( ) < 1.0f )
    {
        return;
    }

    const f32 velocity_yaw{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
    const f32 delta{ DEG2RAD( math::normalize_yaw( velocity_yaw - g_cl.m_cmd->viewangles.y ) ) };
    g_cl.m_cmd->forwardmove = 450.0f * math::cos( delta );
    g_cl.m_cmd->sidemove    = -450.0f * math::sin( delta );
}

void misc::strafe_optimizer( )
{
    if ( !g_cfg.m_misc.m_strafe_optimizer || g_cfg.m_misc.m_auto_strafe ||
         g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK || g_cl.m_local->IsOnGround( ) ||
         g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    const f32 forward{ g_cl.m_cmd->forwardmove };
    const f32 side{ g_cl.m_cmd->sidemove };
    const f32 magnitude{ math::sqrt( forward * forward + side * side ) };
    vec3      velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;
    const f32 speed{ velocity.length2d( ) };
    if ( magnitude <= 1.0f || speed < static_cast< f32 >( math::clamp( g_cfg.m_misc.m_strafe_optimizer_min_speed, 0, 520 ) ) )
    {
        return;
    }

    const f32 wish_speed{ math::smaller( magnitude, g_cl.m_local->m_flMaxspeed( ) ) };
    const f32 acceleration{ math::smaller( 30.0f, g_tf2.sv_airaccelerate->GetFloat( ) * wish_speed * TICK_INTERVAL ) };
    if ( acceleration <= 0.0f || speed <= acceleration * 0.5f )
    {
        return;
    }

    const f32 view_yaw{ g_cl.m_cmd->viewangles.y };
    const f32 velocity_yaw{ RAD2DEG( math::atan2( velocity.y, velocity.x ) ) };
    const f32 wish_yaw{ math::normalize_yaw( view_yaw + RAD2DEG( math::atan2( -side, forward ) ) ) };
    const f32 direction{ math::normalize_yaw( wish_yaw - velocity_yaw ) >= 0.0f ? 1.0f : -1.0f };
    const f32 ideal{ 90.0f - RAD2DEG( math::asin( acceleration * 0.5f / speed ) ) };
    const f32 optimal{ math::normalize_yaw( velocity_yaw + direction * ideal ) };
    const f32 gain{ math::clamp( g_cfg.m_misc.m_strafe_optimizer_gain / 100.0f, 0.0f, 1.0f ) };
    const f32 corrected{ math::normalize_yaw( wish_yaw + math::normalize_yaw( optimal - wish_yaw ) * gain ) };
    const f32 delta{ DEG2RAD( math::normalize_yaw( corrected - view_yaw ) ) };

    g_cl.m_cmd->forwardmove = magnitude * math::cos( delta );
    g_cl.m_cmd->sidemove    = -magnitude * math::sin( delta );
}

void misc::avoid_strafe_walls( )
{
    if ( !g_cfg.m_misc.m_strafe_avoid_walls || g_cl.m_local->m_MoveType( ) != MOVETYPE_WALK ||
         g_cl.m_local->IsOnGround( ) || g_cl.m_local->m_nWaterLevel( ) > 1 )
    {
        return;
    }

    vec3 velocity{ g_cl.m_local->m_vecVelocity( ) };
    velocity.z = 0.0f;
    if ( velocity.normalize( ) <= 0.0f )
    {
        return;
    }

    const f32          distance{ math::clamp( g_cfg.m_misc.m_strafe_avoid_distance, 8.0f, 200.0f ) };
    const vec3         origin{ g_cl.m_local->GetAbsOrigin( ) };
    Ray_t              ray{ };
    trace_t            trace{ };
    CTraceFilterSimple filter{ g_cl.m_local, COLLISION_GROUP_PLAYER_MOVEMENT };
    ray.Init( origin, origin + velocity * distance, g_cl.m_local->m_vecMins( ), g_cl.m_local->m_vecMaxs( ) );
    g_tf2.m_trace->TraceRay( ray, MASK_PLAYERSOLID, &filter, &trace );
    if ( trace.fraction >= 1.0f || trace.startsolid || math::abs( trace.plane.normal.z ) > 0.7f )
    {
        return;
    }

    vec3 tangent{ -trace.plane.normal.y, trace.plane.normal.x, 0.0f };
    if ( tangent.dot( velocity ) < 0.0f )
    {
        tangent = -tangent;
    }
    const f32 yaw{ RAD2DEG( math::atan2( tangent.y, tangent.x ) ) };
    const f32 delta{ DEG2RAD( math::normalize_yaw( yaw - g_cl.m_cmd->viewangles.y ) ) };
    g_cl.m_cmd->forwardmove = 450.0f * math::cos( delta );
    g_cl.m_cmd->sidemove    = -450.0f * math::sin( delta );
}

void misc::taunt_spin( )
{
    if ( !g_cfg.m_misc.m_taunt_spin || !g_cl.m_local->IsTaunting( ) )
    {
        return;
    }

    this->m_taunt_spin_yaw = math::normalize_yaw( this->m_taunt_spin_yaw +
                                                  static_cast< float >( g_cfg.m_misc.m_taunt_spin_speed ) );

    g_cl.m_cmd->viewangles.y = this->m_taunt_spin_yaw;
}

void misc::on_override_view( CViewSetup *setup, const vec3 &eye )
{
    if ( !setup || !g_cl.m_local )
    {
        return;
    }

    if ( setup && g_cl.m_local && g_cfg.m_misc.m_removals[ config::REMOVAL_VIEWPUNCH ] )
    {
        setup->angles -= g_cl.m_local->m_vecPunchAngle( );
    }

    this->fov( setup );
    this->freecam( setup, eye );
    this->thirdperson( setup, eye );
}

bool misc::overlay_removed( IMaterial *material ) const
{
    if ( !g_cfg.m_misc.m_removals[ config::REMOVAL_OVERLAYS ] || !material )
    {
        return false;
    }

    const char *name{ material->GetName( ) };

    if ( !name )
    {
        return false;
    }

    const char *const invuln{ ATLAS_XORSTR_STATIC( "effects/invuln_overlay" ) };
    if ( str::starts_with( name, str::length( name ), invuln, str::length( invuln ) ) )
    {
        return true;
    }

    const char *const names[]{ ATLAS_XORSTR_STATIC( "effects/jarate_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/bleed_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/stealth_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/dodge_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/gas_overlay" ),
                               ATLAS_XORSTR_STATIC( "effects/imcookin" ) };

    for ( const char *candidate : names )
    {
        if ( str::equals( name, candidate ) )
        {
            return true;
        }
    }

    return false;
}

bool misc::is_footstep( const char *name ) const
{
    if ( !name )
    {
        return false;
    }

    const usize       length{ str::length( name ) };
    const char *const waves{ ATLAS_XORSTR_STATIC( "footsteps" ) };
    if ( str::contains( name, length, waves, str::length( waves ) ) )
    {
        return true;
    }

    const usize dot{ str::rfind( name, length, '.' ) };

    if ( dot == str::npos )
    {
        return false;
    }

    const char *const tail{ name + dot + 1 };

    return str::equals_ignore_case( tail, ATLAS_XORSTR( "StepLeft" ).get( ) ) ||
           str::equals_ignore_case( tail, ATLAS_XORSTR( "StepRight" ).get( ) ) ||
           str::equals_ignore_case( tail, ATLAS_XORSTR( "FootstepLeft" ).get( ) ) ||
           str::equals_ignore_case( tail, ATLAS_XORSTR( "FootstepRight" ).get( ) );
}

bool misc::mutes_footstep( const char *name ) const
{
    return g_cfg.m_misc.m_removals[ config::REMOVAL_FOOTSTEPS ] && this->is_footstep( name );
}

bool misc::is_weapon_sound( const StartSoundParams_t &params ) const
{
    if ( params.entchannel == CHAN_WEAPON )
    {
        return true;
    }

    const char *name{ params.pSfx ? params.pSfx->getname( ) : nullptr };
    if ( !name )
    {
        return false;
    }

    while ( *name && !str::is_alpha( *name ) )
    {
        ++name;
    }

    const char *const folder{ ATLAS_XORSTR_STATIC( "weapons" ) };
    const usize       length{ str::length( folder ) };

    return str::equals_ignore_case( name, length, folder, length );
}

const char *misc::giant_shoot_sound( const char *sound ) const
{
    switch ( HASH_RT( sound ) )
    {
        case HASH_CT( "Weapon_GrenadeLauncher.Single" ):
        case HASH_CT( "Weapon_GrenadeLauncher.SingleCrit" ): return ATLAS_XORSTR_STATIC( "MVM.GiantDemoman_Grenadeshoot" );
        case HASH_CT( "Weapon_FlameThrower.Fire" ): return ATLAS_XORSTR_STATIC( "MVM.GiantPyro_FlameStart" );
        case HASH_CT( "Weapon_FlameThrower.FireLoop" ): return ATLAS_XORSTR_STATIC( "MVM.GiantPyro_FlameLoop" );
        default: break;
    }

    return sound;
}

void misc::noisemaker_spam( )
{
    if ( !g_cfg.m_misc.m_noisemaker_spam ||
         ( g_cfg.m_misc.m_noisemaker_spam_mode == config::NOISEMAKER_SPAM_ON_KILL && !m_should_hit_noisemaker ) )
    {
        return;
    }

    // no noisemaker; return
    if ( g_cl.attrib_hook( g_cl.m_local, 0.0f, ATLAS_XORSTR( "enable_misc2_noisemaker" ).get( ) ) == 0.0f )
    {
        m_should_hit_noisemaker = false;
        return;
    }

    const auto shared_data{ g_cl.m_local->GetSharedPlayerData( ) };
    if ( !shared_data )
    {
        return;
    }

    if ( shared_data->m_flNextNoiseMakerTime( ) > g_tf2.m_globals->curtime )
    {
        return;
    }

    // ServerCmdKeyValues calls SendNetMsg under the hood, CLCKeyValues inherits base CNetMessage which
    // submits every message into the reliable stream. If we send KeyValues while choking, we'll just break prediction.
    if ( !g_cl.m_send_packet )
    {
        return;
    }

    DBGOUT( "misc::noisemaker_spam: Sending to server..." );
    auto kv{ new KeyValues( ATLAS_XORSTR_STATIC( "use_action_slot_item_server" ) ) };
    g_tf2.m_engine->ServerCmdKeyValues( kv );
    m_should_hit_noisemaker = false; // Completed the noisemaker request for this event.
}

void misc::noisemaker_notify_event( IGameEvent *event )
{
    if ( g_cfg.m_misc.m_noisemaker_spam_mode != config::NOISEMAKER_SPAM_ON_KILL ||
         !g_cfg.m_misc.m_noisemaker_spam )
    {
        return;
    }

    i32 killer{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( "attacker" ) ) };
    i32 victim{ g_tf2.m_engine->GetPlayerForUserID( event->GetInt( "userid" ) ) };
    if ( killer == victim )
    {
        return;
    }

    if ( killer != g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return;
    }

    if ( victim == g_tf2.m_engine->GetLocalPlayer( ) )
    {
        return;
    }

    DBGOUT( "misc::noisemaker_notify_event: Got event notification!" );
    m_should_hit_noisemaker = true;
}

bool misc::is_taunt_exploit_taking_place( )
{
    return this->m_is_taunt_exploit_taking_place;
}

void misc::on_enable_fog( bool skybox_pass )
{
    const config::fog_t &wanted{ g_cfg.m_fog };

    if ( wanted.m_mode == config::FOG_OFF || ( skybox_pass && !wanted.m_skybox ) )
    {
        return;
    }

    IMatRenderContext *render_context{ g_tf2.m_material_system->GetRenderContext( ) };

    if ( wanted.m_mode == config::FOG_REMOVE )
    {
        render_context->FogMode( MATERIAL_FOG_NONE );
    } else
    {
        const float color[ 3 ]{ static_cast< float >( wanted.m_color.r ) / 255.0f,
                                static_cast< float >( wanted.m_color.g ) / 255.0f,
                                static_cast< float >( wanted.m_color.b ) / 255.0f };

        render_context->FogMode( MATERIAL_FOG_LINEAR );
        render_context->FogColor3fv( color );
        render_context->FogStart( static_cast< float >( wanted.m_start ) );
        render_context->FogEnd( static_cast< float >( wanted.m_end ) );
        render_context->FogMaxDensity( static_cast< float >( wanted.m_density ) * 0.01f );
    }

    render_context->Release( );
}

void misc::fov( CViewSetup *setup )
{
    if ( !g_cfg.m_misc.m_fov_override && g_cfg.m_misc.m_removals[ config::REMOVAL_SCOPE_ZOOM ] == false )
    {
        return;
    }

    // Let the game handle FOV if we're zoomed without scope zoom removal.
    if ( g_cfg.m_misc.m_removals[ config::REMOVAL_SCOPE_ZOOM ] == false && g_cl.m_local->IsZoomed( ) )
    {
        return;
    }

    f32 fov_to_set = g_tf2.fov_desired->GetFloat( );
    if ( g_cfg.m_misc.m_fov_override )
    {
        fov_to_set = ( f32 )g_cfg.m_misc.m_fov;
    }

    // setting m_iDefaultFOV here as well prevents our fov from getting rescaled by the engine.
    g_cl.m_local->m_iFOV( ) = g_cl.m_local->m_iDefaultFOV( ) = setup->fov = fov_to_set;
}

// Rebuilds the check that is done in CTFPlayer::CheckForIdle.
// If the check for the respawn room wasn't in there, just sending IN_BULLRUSH or IN_GRENADE1 would work...
void misc::anti_afk( )
{
    if ( g_cl.m_local->InCond( TF_COND_TAUNTING ) ||
         g_cl.m_local->InCond( TF_COND_HALLOWEEN_KART ) )
    {
        return;
    }

    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT };

    if ( !( g_cl.m_cmd->buttons & MOVE_BUTTONS ) &&
         g_cl.m_local->m_MoveType( ) == MOVETYPE_WALK )
    {
        g_cl.m_cmd->buttons |= IN_FORWARD;
    }

    i32 AWAY_TICKS{ TIME_TO_TICKS( 1.0f ) };
    if ( g_cl.m_cmd->command_number % ( AWAY_TICKS * 2 ) < AWAY_TICKS )
    {
        g_cl.m_cmd->buttons |= IN_BULLRUSH;
    }
}

void misc::perform_taunt_cancel_exploit( )
{
    if ( g_cl.m_weapon->GetWeaponID( ) != TF_WEAPON_FLAMETHROWER )
    {
        this->m_is_taunt_exploit_taking_place = false;
        return;
    }

    if ( !g_cl.m_local->IsTaunting( ) || g_cl.m_local->m_nWaterLevel( ) < 1 )
    {
        this->m_is_taunt_exploit_taking_place = false;
        return;
    }

    i32 charged_airblast{ 0 };
    charged_airblast = g_cl.attrib_hook( g_cl.m_weapon, charged_airblast, ATLAS_XORSTR( "set_charged_airblast" ).get( ) );
    if ( charged_airblast == 0 )
    {
        this->m_is_taunt_exploit_taking_place = false;
        return;
    }

    g_cl.m_cmd->buttons |= IN_DUCK;
    this->m_is_taunt_exploit_taking_place = true;
}

void misc::freecam( CViewSetup *setup, const vec3 &eye )
{
    const bind_t &key{ g_cfg.m_misc.m_freecam_key };
    const bool    gated{ key.m_key != 0 || key.m_mode == bind_mode_t::always };

    const bool wanted{ g_cfg.m_misc.m_freecam && g_cl.m_local->IsAlive( ) &&
                       ( !gated || gui::bind_active( g_cfg.m_misc.m_freecam_key ) ) };

    if ( wanted != this->m_freecam_active )
    {
        this->m_freecam_active = wanted;

        if ( wanted )
        {
            this->m_freecam_origin = eye;
        } else
        {
            g_tf2.m_engine->SetViewAngles( this->m_freecam_angles );
        }
    }

    if ( !wanted )
    {
        return;
    }

    vec3 angles{ };
    g_tf2.m_engine->GetViewAngles( angles );

    vec3 forward{ };
    vec3 right{ };
    math::angle_vectors( angles, &forward, &right );

    const i32  buttons{ this->m_freecam_buttons };
    const auto axis{ [ buttons ]( i32 positive, i32 negative )
    {
        return static_cast< f32 >( ( buttons & positive ) != 0 ) - static_cast< f32 >( ( buttons & negative ) != 0 );
    } };

    const vec3 wish{ forward * axis( IN_FORWARD, IN_BACK ) + right * axis( IN_MOVERIGHT, IN_MOVELEFT ) +
                     vec3{ 0.0f, 0.0f, axis( IN_JUMP, IN_DUCK ) } };

    this->m_freecam_origin += wish * ( static_cast< f32 >( g_cfg.m_misc.m_freecam_speed ) * g_tf2.m_globals->absoluteframetime );

    setup->origin = this->m_freecam_origin;
    setup->angles = angles;
}

void misc::freecam_input( )
{
    constexpr i32 MOVE_BUTTONS{ IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK };

    if ( !this->m_freecam_active )
    {
        this->m_freecam_angles  = g_cl.m_cmd->viewangles;
        this->m_freecam_buttons = 0;
        return;
    }

    this->m_freecam_buttons = g_cl.m_cmd->buttons & MOVE_BUTTONS;

    g_cl.m_cmd->buttons &= ~MOVE_BUTTONS;
    g_cl.m_cmd->forwardmove = 0.0f;
    g_cl.m_cmd->sidemove    = 0.0f;
    g_cl.m_cmd->upmove      = 0.0f;
    g_cl.m_cmd->viewangles  = this->m_freecam_angles;
}

void misc::advance_thirdperson( bool wanted )
{
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const double previous{ this->m_third_time };

    this->m_third_time = now;

    const float seconds{ static_cast< float >( g_cfg.m_misc.m_thirdperson_smooth_time ) / 1000.0f };

    if ( !g_cfg.m_misc.m_thirdperson_smooth || seconds <= 0.0f || previous <= 0.0 )
    {
        this->m_third_fraction = wanted ? 1.0f : 0.0f;
        return;
    }

    const float delta{ math::smaller( static_cast< float >( now - previous ), 0.1f ) };
    const float step{ delta / seconds };

    this->m_third_fraction = math::clamp( this->m_third_fraction + ( wanted ? step : -step ),
                                          0.0f, 1.0f );
}

void misc::restart_minigun_effects( C_TFMinigun *minigun )
{
    if ( minigun->m_iWeaponState( ) != AC_STATE_FIRING )
    {
        return;
    }

    minigun->StartMuzzleEffect( );
    minigun->StartBrassEffect( );
}

void misc::thirdperson( CViewSetup *setup, const vec3 &eye )
{
    if ( g_cl.m_local->IsTaunting( ) )
    {
        return;
    }

    const bind_t &key{ g_cfg.m_misc.m_thirdperson_key };
    const bool    gated{ key.m_key != 0 || key.m_mode == bind_mode_t::always };

    const bool wanted{ g_cfg.m_misc.m_thirdperson && g_cl.m_local->IsAlive( ) &&
                       ( !gated || gui::bind_active( g_cfg.m_misc.m_thirdperson_key ) ) };

    this->advance_thirdperson( wanted );

    const bool pulled_out{ this->m_third_fraction > 0.0f || this->m_freecam_active }; // this name is so bad, but its so funny
    if ( pulled_out != g_tf2.m_input->CAM_IsThirdPerson( ) )
    {
        if ( pulled_out )
        {
            g_tf2.m_input->CAM_ToThirdPerson( );
        } else
        {
            g_tf2.m_input->CAM_ToFirstPerson( );
        }

        g_cl.m_local->ThirdPersonSwitch( pulled_out );

        if ( g_cl.m_weapon && g_cl.m_weapon->GetWeaponID( ) == TF_WEAPON_MINIGUN )
        {
            this->restart_minigun_effects( g_cl.m_weapon->As< C_TFMinigun * >( ) );
        }
    }

    if ( !pulled_out || this->m_freecam_active )
    {
        return;
    }

    vec3 angles{ };
    g_tf2.m_engine->GetViewAngles( angles );

    vec3 forward{ };
    vec3 right{ };
    vec3 up{ };
    math::angle_vectors( angles, &forward, &right, &up );

    const float travel{ this->m_third_fraction };

    const vec3 wanted_origin{ eye - forward * ( static_cast< float >( g_cfg.m_misc.m_thirdperson_distance ) * travel ) +
                              right * ( static_cast< float >( g_cfg.m_misc.m_thirdperson_side ) * travel ) +
                              up * ( static_cast< float >( g_cfg.m_misc.m_thirdperson_height ) * travel ) };

    setup->angles = angles;
    setup->origin = wanted_origin;

    if ( !g_cfg.m_misc.m_thirdperson_collision )
    {
        return;
    }
    trace_t                       trace{ };
    Ray_t                         ray{ };
    CTraceFilterWorldAndPropsOnly filter{ };

    ray.Init( eye, wanted_origin, vec3{ -8.0f, -8.0f, -8.0f }, vec3{ 8.0f, 8.0f, 8.0f } );
    g_tf2.m_trace->TraceRay( ray, MASK_SOLID, &filter, &trace );

    setup->origin = trace.endpos;
}
