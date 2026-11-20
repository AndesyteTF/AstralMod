#include "base.hpp"

#include "skin_share.hpp"

extern api g_api;

ASTRAL_NO_DESTROY skin_share g_skin_share{ };

static constexpr usize SHARE_RESPONSE_LIMIT{ 512 * 1024 };
static constexpr u64   SHARE_INTERVAL{ 10 * 1000 };
static constexpr u64   SHARE_RETRY{ 30 * 1000 };
static constexpr u64   SHARE_MISSING_RETRY{ 5 * 60 * 1000 };

void ASTRAL_STDCALL skin_share::worker( void *context )
{
    auto *self{ static_cast< skin_share * >( context ) };

    self->m_success = g_api.request( ASTRAL_XORWSTR( L"POST" ).get( ), ASTRAL_XORWSTR( L"/v1/skins/sync" ).get( ),
                                     string_view{ self->m_request.data( ), self->m_request.size( ) },
                                     self->m_response, SHARE_RESPONSE_LIMIT, self->m_status );
    _InterlockedExchange( &self->m_state, 2 );
}

bool skin_share::start( )
{
    const i32     local{ g_tf2.m_engine->GetLocalPlayer( ) };
    const i32     maximum{ math::smaller( g_tf2.m_engine->GetMaxClients( ), MAX_TRACKED_PLAYERS - 1 ) };
    player_info_t info{ };
    u32           players[ MAX_TRACKED_PLAYERS ]{ };
    usize         count{ };

    if ( !g_tf2.m_engine->GetPlayerInfo( local, &info ) || !info.friendsID )
    {
        return false;
    }

    for ( i32 index{ 1 }; index <= maximum; ++index )
    {
        player_info_t other{ };
        if ( index != local && g_tf2.m_engine->GetPlayerInfo( index, &other ) && other.friendsID &&
             !other.fakeplayer && !other.ishltv && !other.isreplay )
        {
            players[ count++ ] = other.friendsID;
        }
    }

    atl::sort( players, players + count, []( u32 left, u32 right )
    { return left < right; } );

    const shared_loadout_t &own{ g_skins.own_loadout( ) };
    const bool              enabled{ g_cfg.m_skins.m_enabled && g_cfg.m_skins.m_shared };
    json::writer            writer{ };

    writer.begin_object( );
    writer.member( ASTRAL_XORSTR( "account_id" ).get( ), info.friendsID );
    writer.key( ASTRAL_XORSTR( "players" ).get( ) );
    writer.begin_array( );
    for ( usize index{ }; index < count; ++index )
    {
        if ( !index || players[ index ] != players[ index - 1 ] )
        {
            writer.write( players[ index ] );
        }
    }
    writer.end_array( );

    writer.key( ASTRAL_XORSTR( "loadout" ).get( ) );
    writer.begin_object( );
    writer.key( ASTRAL_XORSTR( "weapons" ).get( ) );
    writer.begin_array( );
    for ( usize index{ }; enabled && index < own.m_weapon_count; ++index )
    {
        const shared_weapon_t &weapon{ own.m_weapons[ index ] };

        writer.begin_object( );
        writer.member( ASTRAL_XORSTR( "key" ).get( ), weapon.m_key );
        writer.member( ASTRAL_XORSTR( "model" ).get( ), weapon.m_model );
        writer.member( ASTRAL_XORSTR( "paintkit" ).get( ), weapon.m_paintkit );
        writer.member( ASTRAL_XORSTR( "wear" ).get( ), weapon.m_wear );
        writer.member( ASTRAL_XORSTR( "australium" ).get( ), weapon.m_australium );
        writer.member( ASTRAL_XORSTR( "festivized" ).get( ), weapon.m_festivized );
        writer.member( ASTRAL_XORSTR( "unusual" ).get( ), weapon.m_unusual );
        writer.member( ASTRAL_XORSTR( "killstreak" ).get( ), weapon.m_killstreak );
        writer.member( ASTRAL_XORSTR( "sheen" ).get( ), weapon.m_sheen );
        writer.member( ASTRAL_XORSTR( "killstreaker" ).get( ), weapon.m_killstreaker );
        writer.end_object( );
    }
    writer.end_array( );

    writer.key( ASTRAL_XORSTR( "wearables" ).get( ) );
    writer.begin_array( );
    for ( usize index{ }; enabled && index < own.m_wearable_count; ++index )
    {
        const shared_wearable_t &wearable{ own.m_wearables[ index ] };

        writer.begin_object( );
        writer.member( ASTRAL_XORSTR( "definition" ).get( ), wearable.m_definition );
        writer.member( ASTRAL_XORSTR( "unusual" ).get( ), wearable.m_unusual );
        writer.end_object( );
    }
    writer.end_array( );
    writer.end_object( );
    writer.end_object( );

    if ( !writer.ok( ) || !this->m_request.resize( writer.text( ).size( ) ) )
    {
        return false;
    }

    for ( usize index{ }; index < this->m_request.size( ); ++index )
    {
        this->m_request[ index ] = writer.text( ).data( )[ index ];
    }

    this->m_success = false;
    this->m_status  = 0;
    _InterlockedExchange( &this->m_state, 1 );

    if ( atl::create_hidden_thread( &skin_share::worker, this ) )
    {
        return true;
    }

    _InterlockedExchange( &this->m_state, 0 );
    return false;
}

bool skin_share::parse( atl::vector< peer_t > &peers ) const
{
    json::document document{ };
    if ( !document.parse( string_view{ this->m_response.data( ), this->m_response.size( ) } ) )
    {
        return false;
    }

    const json::value items{ document.root( )[ ASTRAL_XORSTR( "players" ).get( ) ] };
    if ( items.type( ) != json::type_t::array || items.size( ) >= MAX_TRACKED_PLAYERS )
    {
        return false;
    }

    u32 previous{ };
    for ( usize index{ }; index < items.size( ); ++index )
    {
        const json::value item{ items[ index ] };
        const json::value loadout{ item[ ASTRAL_XORSTR( "loadout" ).get( ) ] };
        const json::value weapons{ loadout[ ASTRAL_XORSTR( "weapons" ).get( ) ] };
        const json::value wearables{ loadout[ ASTRAL_XORSTR( "wearables" ).get( ) ] };
        const i64         account{ item[ ASTRAL_XORSTR( "account_id" ).get( ) ].as< i64 >( ) };
        const usize       weapon_count{ weapons.type( ) == json::type_t::array ? weapons.size( ) : 0 };
        const usize       wearable_count{ wearables.type( ) == json::type_t::array ? wearables.size( ) : 0 };

        if ( account <= previous || account > 0xFFFFFFFFll || weapon_count > config::SKIN_WEAPON_COUNT ||
             wearable_count > config::SKIN_WEARABLE_COUNT )
        {
            return false;
        }

        peer_t *peer{ peers.emplace_back( ) };
        if ( !peer )
        {
            return false;
        }

        peer->m_account = static_cast< u32 >( account );
        previous        = peer->m_account;

        for ( usize slot{ }; slot < weapon_count; ++slot )
        {
            const json::value field{ weapons[ slot ] };
            shared_weapon_t  &weapon{ peer->m_loadout.m_weapons[ peer->m_loadout.m_weapon_count++ ] };

            weapon.m_key          = field[ ASTRAL_XORSTR( "key" ).get( ) ].as< i32 >( -1 );
            weapon.m_model        = field[ ASTRAL_XORSTR( "model" ).get( ) ].as< i32 >( -1 );
            weapon.m_paintkit     = field[ ASTRAL_XORSTR( "paintkit" ).get( ) ].as< u32 >( );
            weapon.m_wear         = field[ ASTRAL_XORSTR( "wear" ).get( ) ].as< u32 >( );
            weapon.m_australium   = field[ ASTRAL_XORSTR( "australium" ).get( ) ].as< bool >( );
            weapon.m_festivized   = field[ ASTRAL_XORSTR( "festivized" ).get( ) ].as< bool >( );
            weapon.m_unusual      = field[ ASTRAL_XORSTR( "unusual" ).get( ) ].as< u32 >( );
            weapon.m_killstreak   = field[ ASTRAL_XORSTR( "killstreak" ).get( ) ].as< u32 >( );
            weapon.m_sheen        = field[ ASTRAL_XORSTR( "sheen" ).get( ) ].as< u32 >( );
            weapon.m_killstreaker = field[ ASTRAL_XORSTR( "killstreaker" ).get( ) ].as< u32 >( );

            g_skins.sanitize( weapon );
        }

        for ( usize slot{ }; slot < wearable_count; ++slot )
        {
            const json::value  field{ wearables[ slot ] };
            shared_wearable_t &wearable{ peer->m_loadout.m_wearables[ peer->m_loadout.m_wearable_count++ ] };

            wearable.m_definition = field[ ASTRAL_XORSTR( "definition" ).get( ) ].as< i32 >( -1 );
            wearable.m_unusual    = field[ ASTRAL_XORSTR( "unusual" ).get( ) ].as< u32 >( );

            g_skins.sanitize( wearable );
        }
    }

    return true;
}

void skin_share::update( )
{
    const u64  now{ I( GetTickCount64 )( ) };
    const bool syncing{ g_api.has_config_token( ) && g_tf2.m_engine->IsInGame( ) };

    if ( _InterlockedCompareExchange( &this->m_state, 2, 2 ) == 2 )
    {
        atl::vector< peer_t > peers{ };

        // a failed sync keeps the last loadouts, they still describe who is on the server
        if ( !this->m_success || this->m_status != 200 || !this->parse( peers ) )
        {
            this->m_next_sync = now + ( this->m_status == 404 ? SHARE_MISSING_RETRY : SHARE_RETRY );
        } else if ( syncing )
        {
            this->m_peers.swap( peers );
        }

        this->m_response.clear( );
        _InterlockedExchange( &this->m_state, 0 );
    }

    if ( !syncing )
    {
        this->m_next_sync = 0;
        this->m_peers.clear( );
        return;
    }

    if ( !this->m_state && now >= this->m_next_sync )
    {
        this->m_next_sync = now + ( this->start( ) ? SHARE_INTERVAL : SHARE_RETRY );
    }
}

const shared_loadout_t *skin_share::loadout( u32 account ) const
{
    for ( const peer_t &peer : this->m_peers )
    {
        if ( peer.m_account == account )
        {
            return &peer.m_loadout;
        }
    }

    return nullptr;
}

bool skin_share::is_peer( u32 account ) const
{
    for ( const peer_t &peer : this->m_peers )
    {
        if ( peer.m_account == account )
        {
            return true;
        }
    }

    return false;
}
