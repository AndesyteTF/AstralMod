#include "base.hpp"
#include "gui/theme.hpp"
#include "steam.hpp"

#include "player_cloud.hpp"
#include "streamer_mode.hpp"

extern api g_api;

ATLAS_NO_DESTROY player_cloud g_player_cloud{ };

//FWork - Setup for local use and raise the maximum
//in theory, should be 128mb with over 1mil entries
//if you go beyond this, you're a fucking psycho

static constexpr usize CLOUD_RESPONSE_LIMIT{ 128 * 1024 * 1024 };
static constexpr usize CLOUD_PLAYER_LIMIT{ 1048576 };
static constexpr u64   CLOUD_STEAMID64_BASE{ 76561197960265728ull };

extern "C"
{
    void *ATLAS_STDCALL WinHttpOpen( const wchar_t *, u32, const wchar_t *, const wchar_t *, u32 );
    i32 ATLAS_STDCALL   WinHttpSetTimeouts( void *, i32, i32, i32, i32 );
    void *ATLAS_STDCALL WinHttpConnect( void *, const wchar_t *, u16, u32 );
    void *ATLAS_STDCALL WinHttpOpenRequest( void *, const wchar_t *, const wchar_t *,
                                            const wchar_t *, const wchar_t *, const wchar_t **, u32 );
    i32 ATLAS_STDCALL   WinHttpSendRequest( void *, const wchar_t *, u32, void *, u32, u32, usize );
    i32 ATLAS_STDCALL   WinHttpReceiveResponse( void *, void * );
    i32 ATLAS_STDCALL   WinHttpQueryHeaders( void *, u32, const wchar_t *, void *, u32 *, u32 * );
    i32 ATLAS_STDCALL   WinHttpReadData( void *, void *, u32, u32 * );
    i32 ATLAS_STDCALL   WinHttpCloseHandle( void * );
}

static bool integration_get( const wchar_t *host, const char *path, atl::vector< char > &output )
{
    wchar_t wide_path[ 8192 ]{ };
    const usize length{ str::length( path ) };
    if ( !length || length >= sizeof( wide_path ) / sizeof( wide_path[ 0 ] ) )
    {
        return false;
    }
    for ( usize index{ }; index < length; ++index )
    {
        wide_path[ index ] = static_cast< u8 >( path[ index ] );
    }

    output.clear( );
    void *session{ I( WinHttpOpen )( ATLAS_XORWSTR_STATIC( L"AstralMod" ), 4, nullptr, nullptr, 0 ) };
    if ( !session )
    {
        return false;
    }
    I( WinHttpSetTimeouts )( session, 5000, 5000, 5000, 5000 );
    void *connection{ I( WinHttpConnect )( session, host, 443, 0 ) };
    void *request{ connection ? I( WinHttpOpenRequest )( connection, ATLAS_XORWSTR_STATIC( L"GET" ), wide_path,
                                                         nullptr, nullptr, nullptr, 0x800000 ) : nullptr };
    u32 status{ };
    u32 status_size{ sizeof( status ) };
    bool ok{ request && I( WinHttpSendRequest )( request, nullptr, 0, nullptr, 0, 0, 0 ) &&
             I( WinHttpReceiveResponse )( request, nullptr ) &&
             I( WinHttpQueryHeaders )( request, 19 | 0x20000000, nullptr, &status, &status_size, nullptr ) &&
             status == 200 };
    char buffer[ 4096 ]{ };
    while ( ok )
    {
        u32 read{ };
        if ( !I( WinHttpReadData )( request, buffer, sizeof( buffer ), &read ) )
        {
            ok = false;
            break;
        }
        if ( !read )
        {
            break;
        }
        const usize old_size{ output.size( ) };
        if ( read > CLOUD_RESPONSE_LIMIT - old_size || !output.resize( old_size + read ) )
        {
            ok = false;
            break;
        }
        atl::memcpy( output.data( ) + old_size, buffer, read );
    }
    atl::memset( buffer, 0, sizeof( buffer ) );
    if ( request ) I( WinHttpCloseHandle )( request );
    if ( connection ) I( WinHttpCloseHandle )( connection );
    I( WinHttpCloseHandle )( session );
    if ( !ok ) output.clear( );
    return ok;
}

static bool players_path( wchar_t ( &path )[ 1024 ], const wchar_t *extension )
{
    u32 length{ I( GetModuleFileNameW )( nullptr, path, 900 ) };
    if ( !length || length >= 900 )
    {
        return false;
    }

    while ( length && path[ length - 1 ] != L'\\' )
    {
        --length;
    }

    for ( wchar_t c : L"AstralMod\\Players" )
    {
        path[ length++ ] = c;
    }

    I( CreateDirectoryW )( path, nullptr );

    path[ length - 1 ] = L'\\';
    do
    {
        path[ length++ ] = *extension;
    } while ( *extension++ );

    return true;
}

static bool tf2pl_download( atl::vector< char > &output )
{
    return integration_get( ATLAS_XORWSTR_STATIC( L"lunaris.surf" ),
                            ATLAS_XORSTR_STATIC( "/playerlist.lunaris.json" ), output );
}

static bool surface_download( atl::vector< char > &output )
{
    return integration_get( ATLAS_XORWSTR_STATIC( L"sourcex.club" ),
                            ATLAS_XORSTR_STATIC( "/database.json" ), output );
}

static bool players_read( atl::vector< char > &output )
{
    output.clear( );

    wchar_t path[ 1024 ]{ };
    if ( !players_path( path, L"players.json" ) )
    {
        return false;
    }

    void *file{ I( CreateFileW )( path, 0x80000000u, 1, nullptr, 3, 0x80, nullptr ) };
    if ( file == reinterpret_cast< void * >( -1 ) )
    {
        if ( I( GetLastError )( ) != 2 )
        {
            return false;
        }
        static constexpr char empty[]{ "{\"players\":[]}" };
        return output.assign( empty, sizeof( empty ) - 1 );
    }

    LARGE_INTEGER size{ };
    DWORD         read{ };
    const bool    ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart >= 0 &&
                      size.QuadPart <= static_cast< i64 >( CLOUD_RESPONSE_LIMIT ) &&
                      output.resize( static_cast< usize >( size.QuadPart ) ) &&
                      I( ReadFile )( file, output.data( ), size.LowPart, &read, nullptr ) &&
                      read == size.QuadPart };

    I( CloseHandle )( file );
    return ok;
}

static bool players_write( string_view input )
{
    wchar_t path[ 1024 ]{ };
    wchar_t temporary[ 1024 ]{ };
    if ( input.size( ) > CLOUD_RESPONSE_LIMIT ||
         !players_path( path, L"players.json" ) || !players_path( temporary, L"players.tmp" ) )
    {
        return false;
    }

    void *file{ I( CreateFileW )( temporary, 0x40000000u, 0, nullptr, 2, 0x80, nullptr ) };
    if ( file == reinterpret_cast< void * >( -1 ) )
    {
        return false;
    }

    DWORD written{ };
    bool  ok{ I( WriteFile )( file, input.data( ), static_cast< u32 >( input.size( ) ), &written, nullptr ) &&
              written == input.size( ) && I( FlushFileBuffers )( file ) };

    I( CloseHandle )( file );

    ok = ok && I( MoveFileExW )( temporary, path, 0x9 );
    if ( !ok )
    {
        I( DeleteFileW )( temporary );
    }

    return ok;
}

enum class ban_reason_t : u32
{
    none        = 0,
    blacklisted = 0x41544C42u,
    pedofilia   = 0x41544C50u,
    zoophilia   = 0x41544C5Au,
};

static const char *ban_reason_name( ban_reason_t reason )
{
    switch ( reason )
    {
        case ban_reason_t::blacklisted: return "Blacklisted";
        case ban_reason_t::pedofilia: return "Pedofilia";
        case ban_reason_t::zoophilia: return "Zoophilia";
        default: return nullptr;
    }
}

static const wchar_t *ban_marker_key( ban_reason_t reason )
{
    switch ( reason )
    {
        case ban_reason_t::blacklisted: return ATLAS_XORWSTR_STATIC( L"Software\\AstralMod\\Blacklisted" );
        case ban_reason_t::pedofilia: return ATLAS_XORWSTR_STATIC( L"Software\\AstralMod\\Pedofilia" );
        case ban_reason_t::zoophilia: return ATLAS_XORWSTR_STATIC( L"Software\\AstralMod\\Zoophilia" );
        default: return nullptr;
    }
}

static ban_reason_t access_marker_reason( )
{
    constexpr ban_reason_t reasons[]{ ban_reason_t::blacklisted, ban_reason_t::pedofilia,
                                      ban_reason_t::zoophilia };
    for ( ban_reason_t reason : reasons )
    {
        HKEY key{ };
        if ( I( RegOpenKeyExW )( HKEY_CURRENT_USER, ban_marker_key( reason ), 0,
                                 KEY_READ, &key ) == 0 )
        {
            I( RegCloseKey )( key );
            return reason;
        }
    }
    return ban_reason_t::none;
}

static bool write_access_marker( ban_reason_t reason )
{
    const wchar_t *path{ ban_marker_key( reason ) };
    if ( !path )
    {
        return false;
    }

    HKEY  key{ };
    DWORD disposition{ };
    if ( I( RegCreateKeyExW )( HKEY_CURRENT_USER, path, 0, nullptr, 0,
                               KEY_WRITE, nullptr, &key, &disposition ) != 0 )
    {
        return false;
    }

    I( RegCloseKey )( key );
    return true;
}

#if defined( ATLAS_DISABLE_CLOUD )

void ATLAS_STDCALL player_cloud::worker( void *context )
{
    auto *self{ static_cast< player_cloud * >( context ) };
    if ( self->m_operation == operation_t::integrations )
    {
        char ids[ 2300 ]{ };
        usize at{ };
        for ( usize index{ }; index < self->m_integration_count; ++index )
        {
            char id[ 32 ]{ };
            str::format_to( id, sizeof( id ), ATLAS_XORSTR( "{}" ).get( ),
                            CLOUD_STEAMID64_BASE + self->m_integration_players[ index ].m_account );
            const usize id_length{ str::length( id ) };
            if ( index ) ids[ at++ ] = ',';
            str::copy( ids + at, sizeof( ids ) - at, id, id_length );
            at += id_length;
        }
        char path[ 8192 ]{ };
        bool attempted{ };
        if ( self->m_steam_key[ 0 ] )
        {
            attempted = true;
            str::format_to( path, sizeof( path ), ATLAS_XORSTR( "/ISteamUser/GetPlayerBans/v0001/?key={}&steamids={}" ).get( ),
                            self->m_steam_key, ids );
            integration_get( ATLAS_XORWSTR_STATIC( L"api.steampowered.com" ), path, self->m_response );
        }
        if ( self->m_steam_history_key[ 0 ] )
        {
            attempted = true;
            str::format_to( path, sizeof( path ), ATLAS_XORSTR( "/api/sourcebans?shouldkey=1&key={}&steamids={}" ).get( ),
                            self->m_steam_history_key, ids );
            integration_get( ATLAS_XORWSTR_STATIC( L"steamhistory.net" ), path, self->m_history_response );
        }
        atl::memset( path, 0, sizeof( path ) );
        atl::memset( ids, 0, sizeof( ids ) );
        self->m_success = attempted && ( !self->m_response.empty( ) || !self->m_history_response.empty( ) );
        self->m_status  = self->m_success ? 200 : 0;
        _InterlockedExchange( &self->m_state, 2 );
        return;
    }
    if ( self->m_operation == operation_t::list )
    {
        const bool atlas_ready{ players_read( self->m_response ) };
        bool       tf2pl_ready{ };
        if ( !self->m_tf2pl_fetched )
        {
            self->m_tf2pl_fetched = true;
            tf2pl_ready = tf2pl_download( self->m_tf2pl_response );
        }
        bool surface_ready{ };
        if ( !self->m_surface_fetched )
        {
            self->m_surface_fetched = true;
            surface_ready = surface_download( self->m_surface_response );
        }
        self->m_success = atlas_ready || tf2pl_ready || surface_ready;
        self->m_status  = atlas_ready ? 200 : 0;
    } else
    {
        atl::scoped_lock lock{ self->m_publish };

        usize first{ };
        usize last{ self->m_known.size( ) };
        while ( first < last )
        {
            const usize middle{ first + ( last - first ) / 2 };
            if ( self->m_known[ middle ].m_account < self->m_report.m_account )
            {
                first = middle + 1;
            } else
            {
                last = middle;
            }
        }

        atl::vector< known_t > players{ };
        for ( usize index{ }; index <= self->m_known.size( ); ++index )
        {
            if ( index == first )
            {
                known_t *mark{ players.emplace_back( ) };
                mark->m_account = self->m_report.m_account;
                if ( index < self->m_known.size( ) && self->m_known[ index ].m_account == self->m_report.m_account )
                {
                    mark->m_marks |= self->m_known[ index ].m_marks;
                }
                str::copy( mark->m_alias, sizeof( mark->m_alias ), self->m_report.m_alias );
            }
            if ( index < self->m_known.size( ) &&
                 !( index == first && self->m_known[ index ].m_account == self->m_report.m_account ) )
            {
                known_t *player{ players.emplace_back( ) };
                *player = self->m_known[ index ];
            }
        }

        self->m_known.swap( players );
        self->m_success = self->local_save( );
        self->m_status   = self->m_success ? 201 : 0;
    }
    _InterlockedExchange( &self->m_state, 2 );
}

#else // cloud

void ATLAS_STDCALL player_cloud::worker( void *context )
{
    auto *self{ static_cast< player_cloud * >( context ) };
    if ( self->m_operation == operation_t::integrations )
    {
        char ids[ 2300 ]{ };
        usize at{ };
        for ( usize index{ }; index < self->m_integration_count; ++index )
        {
            char id[ 32 ]{ };
            str::format_to( id, sizeof( id ), ATLAS_XORSTR( "{}" ).get( ),
                            CLOUD_STEAMID64_BASE + self->m_integration_players[ index ].m_account );
            const usize id_length{ str::length( id ) };
            if ( index ) ids[ at++ ] = ',';
            str::copy( ids + at, sizeof( ids ) - at, id, id_length );
            at += id_length;
        }
        char path[ 8192 ]{ };
        bool attempted{ };
        if ( self->m_steam_key[ 0 ] )
        {
            attempted = true;
            str::format_to( path, sizeof( path ), ATLAS_XORSTR( "/ISteamUser/GetPlayerBans/v0001/?key={}&steamids={}" ).get( ),
                            self->m_steam_key, ids );
            integration_get( ATLAS_XORWSTR_STATIC( L"api.steampowered.com" ), path, self->m_response );
        }
        if ( self->m_steam_history_key[ 0 ] )
        {
            attempted = true;
            str::format_to( path, sizeof( path ), ATLAS_XORSTR( "/api/sourcebans?shouldkey=1&key={}&steamids={}" ).get( ),
                            self->m_steam_history_key, ids );
            integration_get( ATLAS_XORWSTR_STATIC( L"steamhistory.net" ), path, self->m_history_response );
        }
        atl::memset( path, 0, sizeof( path ) );
        atl::memset( ids, 0, sizeof( ids ) );
        self->m_success = attempted && ( !self->m_response.empty( ) || !self->m_history_response.empty( ) );
        self->m_status  = self->m_success ? 200 : 0;
        _InterlockedExchange( &self->m_state, 2 );
        return;
    }
    if ( self->m_operation == operation_t::list )
    {
        const bool atlas_ready{ players_read( self->m_response ) };
        bool       tf2pl_ready{ };
        if ( !self->m_tf2pl_fetched )
        {
            self->m_tf2pl_fetched = true;
            tf2pl_ready = tf2pl_download( self->m_tf2pl_response );
        }
        bool surface_ready{ };
        if ( !self->m_surface_fetched )
        {
            self->m_surface_fetched = true;
            surface_ready = surface_download( self->m_surface_response );
        }
        self->m_success = atlas_ready || tf2pl_ready || surface_ready;
        self->m_status  = atlas_ready ? 200 : 0;
    } else
    {
        json::writer writer{ };
        writer.begin_object( );
        writer.member( ATLAS_XORSTR( "account_id" ).get( ), self->m_report.m_account );
        writer.member( ATLAS_XORSTR( "name" ).get( ), string_view{ self->m_report.m_name } );
        writer.member( ATLAS_XORSTR( "reason" ).get( ), string_view{ self->m_report.m_reason } );
        writer.member( ATLAS_XORSTR( "alias" ).get( ), string_view{ self->m_report.m_alias } );
        writer.end_object( );
        self->m_success = writer.ok( ) &&
                          g_api.request( ATLAS_XORWSTR( L"POST" ).get( ), ATLAS_XORWSTR( L"/v1/cheaters/reports" ).get( ), writer.text( ),
                                         self->m_response, CLOUD_RESPONSE_LIMIT, self->m_status );
    }
    _InterlockedExchange( &self->m_state, 2 );
}

#endif

bool player_cloud::start( operation_t operation )
{
    if ( _InterlockedCompareExchange( &this->m_state, 1, 0 ) != 0 )
    {
        return false;
    }
    this->m_operation = operation;
    this->m_success   = false;
    this->m_status    = 0;
    if ( !atl::create_hidden_thread( &player_cloud::worker, this ) )
    {
        _InterlockedExchange( &this->m_state, 0 );
        return false;
    }
    return true;
}

bool player_cloud::parse_list( atl::vector< known_t > *destination )
{
    json::document document{ };
    if ( !document.parse( string_view{ this->m_response.data( ), this->m_response.size( ) } ) )
    {
        return false;
    }
    atl::vector< known_t > players{ };
    const auto account_id = []( const json::value &item ) -> u32
    {
        json::value id{ item[ ATLAS_XORSTR( "account_id" ).get( ) ] };
        if ( !id.valid( ) )
        {
            id = item[ ATLAS_XORSTR( "steamid" ).get( ) ];
        }
        if ( !id.valid( ) )
        {
            id = item[ ATLAS_XORSTR( "steam_id" ).get( ) ];
        }
        if ( id.type( ) == json::type_t::number )
        {
            u64 value{ static_cast< u64 >( id.as< i64 >( ) ) };
            if ( value >= CLOUD_STEAMID64_BASE )
            {
                value -= CLOUD_STEAMID64_BASE;
            }
            return value && value <= 0xFFFFFFFFull ? static_cast< u32 >( value ) : 0;
        }

        const string_view text{ id.as< string_view >( ) };
        usize             first{ };
        usize             last{ text.size( ) };
        if ( text.size( ) >= 7 && text[ 0 ] == '[' && text[ 2 ] == ':' &&
             text[ 4 ] == ':' && text[ text.size( ) - 1 ] == ']' )
        {
            first = 5;
            last  = text.size( ) - 1;
        }
        u64 value{ };
        if ( first == last || last - first > 20 )
        {
            return 0;
        }
        for ( usize index{ first }; index < last; ++index )
        {
            const char character{ text[ index ] };
            if ( character < '0' || character > '9' || value > ( static_cast< u64 >( -1 ) - 9 ) / 10 )
            {
                return 0;
            }
            value = value * 10 + static_cast< u64 >( character - '0' );
        }
        if ( value >= CLOUD_STEAMID64_BASE )
        {
            value -= CLOUD_STEAMID64_BASE;
        }
        return value && value <= 0xFFFFFFFFull ? static_cast< u32 >( value ) : 0;
    };

    const auto tf2pl_marks = []( const json::value &item ) -> u32
    {
        const json::value attributes{ item[ ATLAS_XORSTR( "attributes" ).get( ) ] };
        if ( attributes.type( ) != json::type_t::array )
        {
            return 0;
        }
        struct attribute_mark_t
        {
            const char *m_name;
            u32         m_mark;
        };
        static constexpr attribute_mark_t marks[]{
            { "cheater", mark_cheater }, { "suspicious", mark_suspicious },
            { "exploiter", mark_exploiter }, { "racist", mark_racist },
            { "hostile", mark_hostile }, { "suspected_cheater", mark_suspected_cheater },
            { "blacklisted", mark_blacklisted }, { "vac_banned", mark_vac_banned },
            { "game_banned", mark_game_banned }, { "sourcebanned", mark_sourcebanned },
            { "pedophilia", mark_pedophilia }, { "pedofilia", mark_pedophilia },
            { "ulistuser", mark_ulistuser }, { "zoophile_zoojokes", mark_zoophile_zoojokes },
            { "zoophilia", mark_zoophile_zoojokes }
        };
        u32 result{ };
        for ( usize index{ }; index < attributes.size( ); ++index )
        {
            const string_view attribute{ attributes[ index ].as< string_view >( ) };
            for ( const attribute_mark_t &mark : marks )
            {
                if ( attribute.size( ) != str::length( mark.m_name ) )
                {
                    continue;
                }

                bool equal{ true };
                for ( usize character{ }; character < attribute.size( ); ++character )
                {
                    const char left{ attribute[ character ] >= 'A' && attribute[ character ] <= 'Z'
                                         ? static_cast< char >( attribute[ character ] + ( 'a' - 'A' ) )
                                         : attribute[ character ] };
                    if ( left != mark.m_name[ character ] )
                    {
                        equal = false;
                        break;
                    }
                }
                if ( equal )
                {
                    result |= mark.m_mark;
                    break;
                }
            }
        }
        return result;
    };

    const auto append = [ & ]( json::value items, bool steam_bans, bool tf2pl ) -> bool
    {
        if ( items.type( ) != json::type_t::array ||
             items.size( ) > CLOUD_PLAYER_LIMIT - players.size( ) )
        {
            return false;
        }
        for ( usize index{ }; index < items.size( ); ++index )
        {
            const json::value item{ items[ index ] };
            if ( steam_bans && !item[ ATLAS_XORSTR( "VACBanned" ).get( ) ].as< bool >( ) &&
                 item[ ATLAS_XORSTR( "NumberOfGameBans" ).get( ) ].as< i64 >( ) <= 0 )
            {
                continue;
            }
            const bool has_attributes{ item[ ATLAS_XORSTR( "attributes" ).get( ) ].type( ) == json::type_t::array };
            const u32 marks{ has_attributes ? tf2pl_marks( item )
                                   : steam_bans
                                       ? ( item[ ATLAS_XORSTR( "VACBanned" ).get( ) ].as< bool >( )
                                               ? mark_vac_banned : 0 ) |
                                         ( item[ ATLAS_XORSTR( "NumberOfGameBans" ).get( ) ].as< i64 >( ) > 0
                                               ? mark_game_banned : 0 )
                                       : mark_cheater };
            const u32 account{ account_id( item ) };
            if ( !account )
            {
                return false;
            }

            json::value name{ tf2pl ? json::value{ } : item[ ATLAS_XORSTR( "alias" ).get( ) ] };
            if ( !tf2pl && name.type( ) != json::type_t::string )
            {
                name = item[ ATLAS_XORSTR( "personaname" ).get( ) ];
            }
            if ( !tf2pl && name.type( ) != json::type_t::string )
            {
                name = item[ ATLAS_XORSTR( "name" ).get( ) ];
            }
            const string_view alias{ name.as< string_view >( ) };
            if ( alias.size( ) > 128 )
            {
                return false;
            }
            if ( !marks && alias.empty( ) )
            {
                continue;
            }
            known_t *player{ players.emplace_back( ) };
            if ( !player )
            {
                return false;
            }
            player->m_account = account;
            player->m_marks   = marks;
            str::copy( player->m_alias, sizeof( player->m_alias ), alias.data( ), alias.size( ) );
        }
        return true;
    };

    const json::value root{ document.root( ) };
    const json::value root_players{ root[ ATLAS_XORSTR( "players" ).get( ) ] };
    const bool root_is_tf2pl{ root_players.type( ) == json::type_t::array && root_players.size( ) &&
                              root_players[ 0 ][ ATLAS_XORSTR( "steamid" ).get( ) ].valid( ) &&
                              root_players[ 0 ][ ATLAS_XORSTR( "attributes" ).get( ) ].type( ) == json::type_t::array };
    if ( !append( root_players, false, root_is_tf2pl ) )
    {
        return false;
    }

    //FWork - to crash or not to crash? fuck it i choose not to crash
    json::value steam{ root[ ATLAS_XORSTR( "steam_api" ).get( ) ] };
    if ( steam.valid( ) )
    {
        json::value items{ steam[ ATLAS_XORSTR( "players" ).get( ) ] };
        if ( items.type( ) != json::type_t::array )
        {
            items = steam[ ATLAS_XORSTR( "response" ).get( ) ][ ATLAS_XORSTR( "players" ).get( ) ];
        }
        if ( !append( items, true, false ) )
        {
            return false;
        }
    }
    const char *sources[]{ ATLAS_XORSTR_STATIC( "steam_history_api" ), ATLAS_XORSTR_STATIC( "tf2pl" ) };
    for ( const char *source : sources )
    {
        json::value data{ root[ source ] };
        if ( !data.valid( ) )
        {
            continue;
        }
        json::value items{ data.type( ) == json::type_t::array ? data : data[ ATLAS_XORSTR( "players" ).get( ) ] };
        if ( !append( items, false, string_view{ source } == string_view{ ATLAS_XORSTR_STATIC( "tf2pl" ) } ) )
        {
            return false;
        }
    }

    atl::sort( players, []( const known_t &left, const known_t &right )
    {
        return left.m_account < right.m_account;
    } );
    usize unique{ };
    for ( usize index{ }; index < players.size( ); ++index )
    {
        if ( unique && players[ unique - 1 ].m_account == players[ index ].m_account )
        {
            players[ unique - 1 ].m_marks |= players[ index ].m_marks;
            if ( !players[ unique - 1 ].m_alias[ 0 ] && players[ index ].m_alias[ 0 ] )
            {
                str::copy( players[ unique - 1 ].m_alias, sizeof( players[ unique - 1 ].m_alias ),
                           players[ index ].m_alias );
            }
            continue;
        }
        if ( unique != index )
        {
            players[ unique ] = players[ index ];
        }
        ++unique;
    }
    if ( !players.resize( unique ) )
    {
        return false;
    }

    atl::scoped_lock lock{ this->m_publish };
    if ( destination )
    {
        destination->swap( players );
    } else
    {
        this->m_known.swap( players );
        this->m_loaded = true;
    }
    return true;
}

bool player_cloud::parse_synced_list( atl::vector< char > &response )
{
    if ( response.empty( ) )
    {
        return false;
    }

    atl::vector< known_t > players{ };
    this->m_response.swap( response );
    const bool parsed{ this->parse_list( &players ) };
    this->m_response.swap( response );
    if ( !parsed )
    {
        return false;
    }

    atl::scoped_lock lock{ this->m_publish };
    if ( this->m_known.size( ) > CLOUD_PLAYER_LIMIT ||
         players.size( ) > CLOUD_PLAYER_LIMIT - this->m_known.size( ) )
    {
        return false;
    }
    atl::vector< known_t > merged{ };
    if ( !merged.resize( this->m_known.size( ) + players.size( ) ) )
    {
        return false;
    }
    usize at{ };
    for ( const known_t &player : this->m_known ) merged[ at++ ] = player;
    for ( const known_t &player : players ) merged[ at++ ] = player;
    atl::sort( merged, []( const known_t &left, const known_t &right )
    {
        return left.m_account < right.m_account;
    } );
    usize unique{ };
    for ( usize index{ }; index < merged.size( ); ++index )
    {
        if ( unique && merged[ unique - 1 ].m_account == merged[ index ].m_account )
        {
            merged[ unique - 1 ].m_marks |= merged[ index ].m_marks;
            if ( !merged[ unique - 1 ].m_alias[ 0 ] && merged[ index ].m_alias[ 0 ] )
            {
                str::copy( merged[ unique - 1 ].m_alias, sizeof( merged[ unique - 1 ].m_alias ),
                           merged[ index ].m_alias );
            }
            continue;
        }
        if ( unique != index ) merged[ unique ] = merged[ index ];
        ++unique;
    }
    if ( !merged.resize( unique ) )
    {
        return false;
    }
    this->m_known.swap( merged );
    this->m_loaded = true;
    return this->local_save( );
}

bool player_cloud::parse_integrations( )
{
    json::document steam_document{ };
    json::document history_document{ };
    const bool have_steam{ !this->m_response.empty( ) &&
                           steam_document.parse( string_view{ this->m_response.data( ), this->m_response.size( ) } ) };
    const bool have_history{ !this->m_history_response.empty( ) &&
                             history_document.parse( string_view{ this->m_history_response.data( ), this->m_history_response.size( ) } ) };
    if ( !have_steam && !have_history )
    {
        return false;
    }

    const json::value steam_players{ have_steam
        ? steam_document.root( )[ ATLAS_XORSTR( "players" ).get( ) ] : json::value{ } };
    const json::value history_response{ have_history
        ? history_document.root( )[ ATLAS_XORSTR( "response" ).get( ) ] : json::value{ } };
    if ( have_steam && steam_players.type( ) != json::type_t::array )
    {
        return false;
    }
    if ( have_history && history_response.type( ) != json::type_t::object )
    {
        return false;
    }

    atl::vector< known_t > integrated{ };
    for ( usize player_index{ }; player_index < this->m_integration_count; ++player_index )
    {
        const known_t &pending{ this->m_integration_players[ player_index ] };
        char steam_id[ 32 ]{ };
        str::format_to( steam_id, sizeof( steam_id ), ATLAS_XORSTR( "{}" ).get( ),
                        CLOUD_STEAMID64_BASE + pending.m_account );
        u32 marks{ };

        for ( usize index{ }; have_steam && index < steam_players.size( ); ++index )
        {
            const json::value ban{ steam_players[ index ] };
            if ( ban[ ATLAS_XORSTR( "SteamId" ).get( ) ].as< string_view >( ) != string_view{ steam_id } )
            {
                continue;
            }
            if ( ban[ ATLAS_XORSTR( "CommunityBanned" ).get( ) ].as< bool >( ) ) marks |= mark_blacklisted;
            if ( ban[ ATLAS_XORSTR( "VACBanned" ).get( ) ].as< bool >( ) ||
                 ban[ ATLAS_XORSTR( "NumberOfVACBans" ).get( ) ].as< i64 >( ) > 0 ) marks |= mark_vac_banned;
            if ( ban[ ATLAS_XORSTR( "NumberOfGameBans" ).get( ) ].as< i64 >( ) > 0 ) marks |= mark_game_banned;
            break;
        }

        const json::value source_bans{ have_history ? history_response[ steam_id ] : json::value{ } };
        if ( have_history && source_bans.valid( ) && source_bans.type( ) != json::type_t::array )
        {
            return false;
        }
        for ( usize index{ }; source_bans.type( ) == json::type_t::array && index < source_bans.size( ); ++index )
        {
            const string_view state{ source_bans[ index ][ ATLAS_XORSTR( "CurrentState" ).get( ) ].as< string_view >( ) };
            if ( state == string_view{ ATLAS_XORSTR_STATIC( "Permanent" ) } ||
                 state == string_view{ ATLAS_XORSTR_STATIC( "Temp-Ban" ) } )
            {
                marks |= mark_sourcebanned;
                break;
            }
        }
        if ( marks )
        {
            known_t *entry{ integrated.emplace_back( ) };
            if ( !entry )
            {
                return false;
            }
            *entry = pending;
            entry->m_marks = marks;
        }
    }
    atl::sort( integrated, []( const known_t &left, const known_t &right )
    {
        return left.m_account < right.m_account;
    } );
    atl::scoped_lock lock{ this->m_publish };
    for ( const known_t &player : integrated )
    {
        usize first{ };
        usize last{ this->m_integrated.size( ) };
        while ( first < last )
        {
            const usize middle{ first + ( last - first ) / 2 };
            if ( this->m_integrated[ middle ].m_account < player.m_account ) first = middle + 1;
            else last = middle;
        }
        if ( first < this->m_integrated.size( ) && this->m_integrated[ first ].m_account == player.m_account )
        {
            this->m_integrated[ first ] = player;
            continue;
        }
        known_t *slot{ this->m_integrated.emplace_back( ) };
        if ( !slot ) return false;
        for ( usize index{ this->m_integrated.size( ) - 1 }; index > first; --index )
        {
            this->m_integrated[ index ] = this->m_integrated[ index - 1 ];
        }
        this->m_integrated[ first ] = player;
    }
    return true;
}

void player_cloud::queue_integrations( )
{
    if ( this->m_state || ( !this->m_steam_key[ 0 ] && !this->m_steam_history_key[ 0 ] ) )
    {
        return;
    }
    this->m_integration_count = 0;
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32 maximum{ math::smaller( g_tf2.m_engine->GetMaxClients( ), 100 ) };
    for ( i32 index{ 1 }; index <= maximum; ++index )
    {
        player_info_t info{ };
        if ( index == local || !g_tf2.m_engine->GetPlayerInfo( index, &info ) || !info.friendsID ||
             info.fakeplayer || info.ishltv || info.isreplay )
        {
            continue;
        }
        known_t &pending{ this->m_integration_players[ this->m_integration_count++ ] };
        pending.m_account = info.friendsID;
        pending.m_marks   = 0;
        str::copy( pending.m_alias, sizeof( pending.m_alias ), info.name );
    }
    if ( this->m_integration_count )
    {
        if ( this->start( operation_t::integrations ) )
        {
            for ( i32 index{ 1 }; index <= maximum; ++index )
            {
                player_info_t info{ };
                if ( index != local && g_tf2.m_engine->GetPlayerInfo( index, &info ) && info.friendsID &&
                     !info.fakeplayer && !info.ishltv && !info.isreplay )
                {
                    this->m_integration_seen[ index ] = info.friendsID;
                }
            }
        }
    }
}

bool player_cloud::queue_integration( u32 account, string_view name )
{
    if ( this->m_state || !account || ( !this->m_steam_key[ 0 ] && !this->m_steam_history_key[ 0 ] ) )
    {
        return false;
    }
    this->m_integration_count = 1;
    known_t &pending{ this->m_integration_players[ 0 ] };
    pending = known_t{ };
    pending.m_account = account;
    pending.m_marks   = 0;
    str::copy( pending.m_alias, sizeof( pending.m_alias ), name.data( ), name.size( ) );
    return this->start( operation_t::integrations );
}

bool player_cloud::known( u32 account, char ( &alias )[ 129 ] ) const
{
    return ( this->marks( account, alias ) & mark_cheating ) != 0;
}

u32 player_cloud::marks( u32 account, char ( &alias )[ 129 ] ) const
{
    alias[ 0 ] = 0;
    if ( !account ) return 0;
    atl::scoped_lock lock{ this->m_publish };
    const auto find = [ account ]( const atl::vector< known_t > &players ) -> const known_t *
    {
        usize first{ };
        usize last{ players.size( ) };
        while ( first < last )
        {
            const usize middle{ first + ( last - first ) / 2 };
            if ( players[ middle ].m_account < account ) first = middle + 1;
            else last = middle;
        }
        return first < players.size( ) && players[ first ].m_account == account ? &players[ first ] : nullptr;
    };
    u32 result{ };
    const atl::vector< known_t > *sources[]{ &this->m_known, &this->m_integrated };
    for ( const atl::vector< known_t > *source : sources )
    {
        if ( const known_t *player{ find( *source ) } )
        {
            result |= player->m_marks;
            if ( !alias[ 0 ] && player->m_alias[ 0 ] ) str::copy( alias, sizeof( alias ), player->m_alias );
        }
    }
    return result;
}

bool player_cloud::remove_mark( u32 account, u32 mark )
{
    if ( !account || !mark || ( mark & ~mark_all ) || this->m_state ) return false;
    atl::scoped_lock lock{ this->m_publish };
    bool changed{ };
    atl::vector< known_t > *sources[]{ &this->m_known, &this->m_integrated };
    for ( atl::vector< known_t > *source : sources )
    {
        for ( usize index{ }; index < source->size( ); ++index )
        {
            known_t &player{ ( *source )[ index ] };
            if ( player.m_account == account && ( player.m_marks & mark ) )
            {
                player.m_marks &= ~mark;
                changed = true;
            }
        }
    }
    return changed && this->local_save( );
}

bool player_cloud::set_local_alias( u32 account, string_view alias )
{
    if ( !account || alias.size( ) > 128 || this->m_state ) return false;
    atl::scoped_lock lock{ this->m_publish };
    usize first{ };
    usize last{ this->m_known.size( ) };
    while ( first < last )
    {
        const usize middle{ first + ( last - first ) / 2 };
        if ( this->m_known[ middle ].m_account < account ) first = middle + 1;
        else last = middle;
    }
    if ( first == this->m_known.size( ) || this->m_known[ first ].m_account != account )
    {
        known_t *slot{ this->m_known.emplace_back( ) };
        if ( !slot ) return false;
        for ( usize index{ this->m_known.size( ) - 1 }; index > first; --index )
            this->m_known[ index ] = this->m_known[ index - 1 ];
        this->m_known[ first ] = known_t{ };
        this->m_known[ first ].m_account = account;
    }
    atl::memset( this->m_known[ first ].m_alias, 0, sizeof( this->m_known[ first ].m_alias ) );
    str::copy( this->m_known[ first ].m_alias, sizeof( this->m_known[ first ].m_alias ),
               alias.data( ), alias.size( ) );
    return this->local_save( );
}

bool player_cloud::refresh_lists( )
{
    if ( this->m_state ) return false;
    this->m_tf2pl_fetched = false;
    this->m_surface_fetched = false;
    return this->start( operation_t::list );
}

void player_cloud::set_integration_keys( string_view steam, string_view steam_history )
{
    if ( steam.size( ) > 64 || steam_history.size( ) > 128 || this->m_state )
    {
        return;
    }
    atl::scoped_lock lock{ this->m_publish };
    atl::memset( this->m_steam_key, 0, sizeof( this->m_steam_key ) );
    atl::memset( this->m_steam_history_key, 0, sizeof( this->m_steam_history_key ) );
    str::copy( this->m_steam_key, sizeof( this->m_steam_key ), steam.data( ), steam.size( ) );
    str::copy( this->m_steam_history_key, sizeof( this->m_steam_history_key ), steam_history.data( ), steam_history.size( ) );
}

bool player_cloud::load_tf2pl_file( string_view path )
{
    if ( this->m_state || path.empty( ) || path.size( ) >= 1024 )
    {
        return false;
    }
    wchar_t wide_path[ 1024 ]{ };
    for ( usize index{ }; index < path.size( ); ++index )
    {
        const u8 character{ static_cast< u8 >( path[ index ] ) };
        if ( character >= 0x80 )
        {
            return false;
        }
        wide_path[ index ] = character;
    }
    void *file{ I( CreateFileW )( wide_path, 0x80000000u, 1, nullptr, 3, 0x80, nullptr ) };
    if ( file == reinterpret_cast< void * >( -1 ) )
    {
        return false;
    }
    LARGE_INTEGER size{ };
    DWORD read{ };
    atl::vector< char > data{ };
    const bool loaded{ I( GetFileSizeEx )( file, &size ) && size.QuadPart > 0 &&
                       size.QuadPart <= static_cast< i64 >( CLOUD_RESPONSE_LIMIT ) &&
                       data.resize( static_cast< usize >( size.QuadPart ) ) &&
                       I( ReadFile )( file, data.data( ), size.LowPart, &read, nullptr ) &&
                       read == size.QuadPart };
    I( CloseHandle )( file );
    if ( !loaded )
    {
        return false;
    }
    atl::vector< known_t > imported{ };
    this->m_response.swap( data );
    const bool parsed{ this->parse_list( &imported ) };
    this->m_response.clear( );
    this->m_response.swap( data );
    if ( !parsed )
    {
        return false;
    }

    atl::scoped_lock lock{ this->m_publish };
    if ( this->m_known.size( ) > CLOUD_PLAYER_LIMIT ||
         imported.size( ) > CLOUD_PLAYER_LIMIT - this->m_known.size( ) )
    {
        return false;
    }
    atl::vector< known_t > merged{ };
    if ( !merged.resize( this->m_known.size( ) + imported.size( ) ) )
    {
        return false;
    }
    usize at{ };
    for ( const known_t &player : this->m_known ) merged[ at++ ] = player;
    for ( const known_t &player : imported ) merged[ at++ ] = player;
    atl::sort( merged, []( const known_t &left, const known_t &right )
    {
        return left.m_account < right.m_account;
    } );

    usize unique{ };
    for ( usize index{ }; index < merged.size( ); ++index )
    {
        if ( unique && merged[ unique - 1 ].m_account == merged[ index ].m_account )
        {
            merged[ unique - 1 ].m_marks |= merged[ index ].m_marks;
            if ( !merged[ unique - 1 ].m_alias[ 0 ] && merged[ index ].m_alias[ 0 ] )
            {
                str::copy( merged[ unique - 1 ].m_alias, sizeof( merged[ unique - 1 ].m_alias ),
                           merged[ index ].m_alias );
            }
            continue;
        }
        if ( unique != index ) merged[ unique ] = merged[ index ];
        ++unique;
    }
    if ( !merged.resize( unique ) )
    {
        return false;
    }
    this->m_known.swap( merged );
    this->m_loaded = true;
    return this->local_save( );
}

bool player_cloud::has_steam_key( ) const
{
    return this->m_steam_key[ 0 ] != 0;
}

bool player_cloud::has_steam_history_key( ) const
{
    return this->m_steam_history_key[ 0 ] != 0;
}

bool player_cloud::local_save( ) const
{
    json::writer writer{ };
    writer.begin_object( );
    writer.key( ATLAS_XORSTR( "players" ).get( ) );
    writer.begin_array( );
    for ( usize index{ }; index < this->m_known.size( ); ++index )
    {
        const known_t &player{ this->m_known[ index ] };
        if ( !player.m_marks && !player.m_alias[ 0 ] ) continue;
        writer.begin_object( );
        writer.member( ATLAS_XORSTR( "account_id" ).get( ), player.m_account );
        writer.member( ATLAS_XORSTR( "alias" ).get( ), string_view{ player.m_alias } );
        writer.key( ATLAS_XORSTR( "attributes" ).get( ) );
        writer.begin_array( );
        static constexpr const char *names[]{
            "cheater", "suspicious", "exploiter", "racist", "hostile", "suspected_cheater",
            "blacklisted", "vac_banned", "game_banned", "sourcebanned", "pedophilia",
            "ulistuser", "zoophile_zoojokes"
        };
        for ( usize mark{ }; mark < sizeof( names ) / sizeof( names[ 0 ] ); ++mark )
        {
            if ( player.m_marks & ( 1u << mark ) ) writer.write( string_view{ names[ mark ] } );
        }
        writer.end_array( );
        writer.end_object( );
    }
    writer.end_array( );
    writer.end_object( );

    return writer.ok( ) && players_write( writer.text( ) );
}

#if defined( ATLAS_DISABLE_CLOUD )

player_cloud::report_result_t player_cloud::submit_report( u32 account, string_view name,
                                                          string_view reason,
                                                          string_view alias )
{
    if ( !account || name.empty( ) || name.size( ) >= sizeof( this->m_report.m_name ) ||
         reason.empty( ) || reason.size( ) >= sizeof( this->m_report.m_reason ) ||
         alias.size( ) >= sizeof( this->m_report.m_alias ) )
    {
        return report_result_t::invalid;
    }
    if ( this->m_state )
    {
        return report_result_t::busy;
    }
    this->m_report           = report_t{ };
    this->m_report.m_account = account;
    str::copy( this->m_report.m_name, sizeof( this->m_report.m_name ),
               name.data( ), name.size( ) );
    str::copy( this->m_report.m_reason, sizeof( this->m_report.m_reason ),
               reason.data( ), reason.size( ) );
    str::copy( this->m_report.m_alias, sizeof( this->m_report.m_alias ),
               alias.data( ), alias.size( ) );
    return this->start( operation_t::report ) ? report_result_t::started
                                              : report_result_t::unavailable;
}

#else // cloud

player_cloud::report_result_t player_cloud::submit_report( u32 account, string_view name,
                                                           string_view reason, string_view alias )
{
    if ( !account || name.empty( ) || name.size( ) >= sizeof( this->m_report.m_name ) ||
         reason.empty( ) || reason.size( ) >= sizeof( this->m_report.m_reason ) ||
         alias.size( ) >= sizeof( this->m_report.m_alias ) )
    {
        return report_result_t::invalid;
    }
    if ( !g_api.has_config_token( ) )
    {
        return report_result_t::no_session;
    }
    if ( this->m_state )
    {
        return report_result_t::busy;
    }
    this->m_report           = report_t{ };
    this->m_report.m_account = account;
    str::copy( this->m_report.m_name, sizeof( this->m_report.m_name ),
               name.data( ), name.size( ) );
    str::copy( this->m_report.m_reason, sizeof( this->m_report.m_reason ),
               reason.data( ), reason.size( ) );
    str::copy( this->m_report.m_alias, sizeof( this->m_report.m_alias ),
               alias.data( ), alias.size( ) );
    return this->start( operation_t::report ) ? report_result_t::started
                                              : report_result_t::unavailable;
}

#endif

void player_cloud::scan_players( )
{
    const i32 local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32 maximum{ math::smaller( g_tf2.m_engine->GetMaxClients( ),
                                      MAX_TRACKED_PLAYERS - 1 ) };
    for ( i32 index{ 1 }; index <= maximum; ++index )
    {
        player_info_t info{ };
        if ( index == local || !g_tf2.m_engine->GetPlayerInfo( index, &info ) ||
             !info.friendsID || info.fakeplayer || info.ishltv || info.isreplay )
        {
            this->m_notified[ index ]         = 0;
            this->m_integration_seen[ index ] = 0;
            continue;
        }

        if ( this->m_integration_seen[ index ] != info.friendsID &&
             this->queue_integration( info.friendsID, string_view{ info.name } ) )
        {
            this->m_integration_seen[ index ] = info.friendsID;
        }

        char cloud_alias[ 129 ]{ };
        if ( !this->known( info.friendsID, cloud_alias ) )
        {
            this->m_notified[ index ] = 0;
            continue;
        }

        if ( this->m_notified[ index ] == info.friendsID )
        {
            continue;
        }

        this->m_notified[ index ] = info.friendsID;

        const bool  hidden{ g_streamer_mode.should_hide_player( index ) };
        const char *name{ hidden ? g_streamer_mode.get_ideal_player_name( index ) : info.name };
        const char *alias{ hidden ? nullptr : g_cfg.player_alias( info.friendsID ) };
        if ( !alias && !hidden && cloud_alias[ 0 ] )
        {
            alias = cloud_alias;
        }

        // NOTE: WE SHOULD HANDLE THIS IN A BETTER WAY.
        //FWork - probably will later
        char message[ 512 ]{ };
        if ( alias )
        {
            str::format_to( message, sizeof( message ), ATLAS_XORSTR( "The player \"{}\" is a known cheater and is commonly referred to as \"{}\"." ).get( ), name, alias );
        } else
        {
            str::format_to( message, sizeof( message ), ATLAS_XORSTR( "The player \"{}\" is a known cheater." ).get( ), name );
        }

        push_notification( notify_kind_t::warning, ICON_BAN, name, message );

        if ( g_cfg.m_misc.m_watchdog_notify_party )
        {
            if ( alias )
            {
                str::format_to( message, sizeof( message ), ATLAS_XORSTR( "[AstralMod] The player \"{}\" is a known cheater and is commonly referred to as \"{}\"." ).get( ), info.name, alias );
            } else
            {
                str::format_to( message, sizeof( message ), ATLAS_XORSTR( "[AstralMod] The player \"{}\" is a known cheater." ).get( ), info.name );
            }

            TFPartyClient( )->SendPartyChat( message );
        }
    }
}

bool player_cloud::access_allowed( )
{
    if ( this->m_access_denied )
    {
        return false;
    }
    if ( this->m_access_checked )
    {
        return true;
    }

    const ban_reason_t marker_reason{ access_marker_reason( ) };
    ban_reason_t       reason{ marker_reason };
    if ( reason == ban_reason_t::none )
    {
        if ( !this->m_loaded )
        {
            return true;
        }

        const u32 account{ g_steam.local_account_id( ) };
        if ( !account )
        {
            return true;
        }

        char      alias[ 129 ]{ };
        const u32 account_marks{ this->marks( account, alias ) };
        if ( account_marks & mark_blacklisted )
        {
            reason = ban_reason_t::blacklisted;
        } else if ( account_marks & mark_pedophilia )
        {
            reason = ban_reason_t::pedofilia;
        } else if ( account_marks & mark_zoophile_zoojokes )
        {
            reason = ban_reason_t::zoophilia;
        }
        if ( reason != ban_reason_t::none )
        {
            write_access_marker( reason );
        }
    }

    if ( reason == ban_reason_t::none )
    {
        this->m_access_checked = true;
        return true;
    }

    this->m_access_denied = true;
    I( RaiseException )( ASTRAL_ACCESS_DENIED_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, nullptr );

    // RaiseException should never return after the vectored handler sees this
    // code. Retain a silent hard stop in case another handler intercepts it.
    I( ExitProcess )( ASTRAL_ACCESS_DENIED_EXCEPTION );
    return false;
}

#if defined( ATLAS_DISABLE_CLOUD )

void player_cloud::poll( )
{
    if ( !g_cl.m_is_running )
    {
        return;
    }
    if ( !this->access_allowed( ) )
    {
        return;
    }
    const u64 now{ I( GetTickCount64 )( ) };
    if ( _InterlockedCompareExchange( &this->m_state, 2, 2 ) == 2 )
    {
        if ( this->m_operation == operation_t::list )
        {
            const bool atlas_ready{ this->m_status == 200 && this->parse_list( ) };
            const bool tf2pl_ready{ this->parse_synced_list( this->m_tf2pl_response ) };
            const bool surface_ready{ this->parse_synced_list( this->m_surface_response ) };
            this->m_success = atlas_ready || tf2pl_ready || surface_ready;
        } else if ( this->m_operation == operation_t::integrations )
        {
            this->m_success = this->m_success && this->parse_integrations( );
        } else
        {
            const bool  sent{ this->m_success && this->m_status == 201 };
            const char *message{ sent ? ATLAS_XORSTR_STATIC( "Mark saved to local database" )
                                      : ATLAS_XORSTR_STATIC( "Failed to save mark" ) };
            push_notification( sent ? notify_kind_t::success : notify_kind_t::error,
                               ICON_PLAYERS, ATLAS_XORSTR_STATIC( "Player mark" ),
                               message );
        }
        this->m_response.clear( );
        this->m_history_response.clear( );
        this->m_tf2pl_response.clear( );
        this->m_surface_response.clear( );
        _InterlockedExchange( &this->m_state, 0 );
    }

    if ( !this->m_list_started && !this->m_state )
    {
        this->m_list_started = true;
        this->start( operation_t::list );
    }
    if ( now < this->m_next_scan )
    {
        return;
    }
    this->m_next_scan = now + 1000;
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        if ( this->m_in_game )
        {
            for ( u32 &account : this->m_notified )
            {
                account = 0;
            }
        }
        this->m_in_game = false;
    } else
    {
        if ( !this->m_in_game && !this->m_state )
        {
            {
                atl::scoped_lock lock{ this->m_publish };
                this->m_integrated.clear( );
            }
            for ( u32 &account : this->m_integration_seen ) account = 0;
            this->queue_integrations( );
            this->m_in_game = true;
        }
        this->scan_players( );
    }
}

#else // cloud

void player_cloud::poll( )
{
    if ( !g_cl.m_is_running )
    {
        return;
    }
    if ( !this->access_allowed( ) )
    {
        return;
    }
    const u64 now{ I( GetTickCount64 )( ) };
    if ( _InterlockedCompareExchange( &this->m_state, 2, 2 ) == 2 )
    {
        if ( this->m_operation == operation_t::list )
        {
            const bool atlas_ready{ this->m_status == 200 && this->parse_list( ) };
            const bool tf2pl_ready{ this->parse_synced_list( this->m_tf2pl_response ) };
            const bool surface_ready{ this->parse_synced_list( this->m_surface_response ) };
            this->m_success = atlas_ready || tf2pl_ready || surface_ready;
        } else if ( this->m_operation == operation_t::integrations )
        {
            this->m_success = this->m_success && this->parse_integrations( );
        } else
        {
            const bool  sent{ this->m_success && this->m_status == 201 };
            const char *message{ sent ? ATLAS_XORSTR_STATIC( "Report sent for review" )
                                 : this->m_status == 404
                                     ? ATLAS_XORSTR_STATIC( "Cloud report API not deployed" )
                                 : this->m_status == 409
                                     ? ATLAS_XORSTR_STATIC( "An open report already exists" )
                                 : this->m_status == 401 || this->m_status == 403
                                     ? ATLAS_XORSTR_STATIC( "Cloud session rejected" )
                                 : this->m_status == 429
                                     ? ATLAS_XORSTR_STATIC( "Cloud rate limit reached" )
                                 : this->m_status
                                     ? ATLAS_XORSTR_STATIC( "Cloud request failed" )
                                     : ATLAS_XORSTR_STATIC( "Could not reach Cloud" ) };
            push_notification( sent ? notify_kind_t::success : notify_kind_t::error,
                               ICON_PLAYERS, ATLAS_XORSTR_STATIC( "Cloud report" ),
                               message );
        }
        this->m_response.clear( );
        this->m_history_response.clear( );
        this->m_tf2pl_response.clear( );
        this->m_surface_response.clear( );
        _InterlockedExchange( &this->m_state, 0 );
    }

    if ( !this->m_list_started && !this->m_state )
    {
        this->m_list_started = true;
        this->start( operation_t::list );
    }
    if ( now < this->m_next_scan )
    {
        return;
    }
    this->m_next_scan = now + 1000;
    if ( !g_tf2.m_engine->IsInGame( ) )
    {
        if ( this->m_in_game )
        {
            for ( u32 &account : this->m_notified )
            {
                account = 0;
            }
        }
        this->m_in_game = false;
    } else
    {
        if ( !this->m_in_game && !this->m_state )
        {
            {
                atl::scoped_lock lock{ this->m_publish };
                this->m_integrated.clear( );
            }
            for ( u32 &account : this->m_integration_seen ) account = 0;
            this->queue_integrations( );
            this->m_in_game = true;
        }
        this->scan_players( );
    }
}

#endif

void player_cloud::on_level_shutdown( )
{
    for ( u32 &account : this->m_notified )
    {
        account = 0;
    }
    for ( u32 &account : this->m_integration_seen )
    {
        account = 0;
    }
    {
        atl::scoped_lock lock{ this->m_publish };
        this->m_integrated.clear( );
    }
    this->m_in_game   = false;
    this->m_next_scan = 0;
}
