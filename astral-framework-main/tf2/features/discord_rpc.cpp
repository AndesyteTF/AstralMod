#include "discord_rpc.hpp"

#include "../config.hpp"
#include "../global.hpp"
#include "movement_recorder.hpp"

namespace
{
constexpr const char *APPLICATION_ID{ "1569420474130374656" };
constexpr u32         OPCODE_HANDSHAKE{ 0 };
constexpr u32         OPCODE_FRAME{ 1 };
constexpr u32         OPCODE_CLOSE{ 2 };
constexpr u32         OPCODE_PING{ 3 };
constexpr u32         OPCODE_PONG{ 4 };
constexpr u32         MAX_PAYLOAD{ 16384 };

struct frame_header_t
{
    u32 m_opcode{ };
    u32 m_length{ };
};

bool write_all( HANDLE pipe, const void *data, u32 size )
{
    const u8 *cursor{ static_cast< const u8 * >( data ) };
    while ( size )
    {
        DWORD written{ };
        if ( !WriteFile( pipe, cursor, size, &written, nullptr ) || !written ) return false;
        cursor += written;
        size -= written;
    }
    return true;
}

bool send_frame( HANDLE pipe, u32 opcode, const char *payload )
{
    const u32 length{ static_cast< u32 >( str::length( payload ) ) };
    const frame_header_t header{ opcode, length };
    return write_all( pipe, &header, sizeof( header ) ) && write_all( pipe, payload, length );
}

const char *class_name( i32 value )
{
    switch ( value )
    {
        case TF_CLASS_SCOUT: return ASTRAL_XORSTR_STATIC( "Scout" );
        case TF_CLASS_SNIPER: return ASTRAL_XORSTR_STATIC( "Sniper" );
        case TF_CLASS_SOLDIER: return ASTRAL_XORSTR_STATIC( "Soldier" );
        case TF_CLASS_DEMOMAN: return ASTRAL_XORSTR_STATIC( "Demoman" );
        case TF_CLASS_MEDIC: return ASTRAL_XORSTR_STATIC( "Medic" );
        case TF_CLASS_HEAVYWEAPONS: return ASTRAL_XORSTR_STATIC( "Heavy" );
        case TF_CLASS_PYRO: return ASTRAL_XORSTR_STATIC( "Pyro" );
        case TF_CLASS_SPY: return ASTRAL_XORSTR_STATIC( "Spy" );
        case TF_CLASS_ENGINEER: return ASTRAL_XORSTR_STATIC( "Engineer" );
        default: return ASTRAL_XORSTR_STATIC( "Unknown class" );
    }
}
}

void discord_rpc::disconnect( )
{
    const HANDLE pipe{ static_cast< HANDLE >( m_pipe ) };
    if ( pipe != INVALID_HANDLE_VALUE ) CloseHandle( pipe );
    m_pipe = INVALID_HANDLE_VALUE;
}

bool discord_rpc::connect( )
{
    // Discord exposes its local RPC endpoint through the Win32 named-pipe namespace.
    // The extended-path prefix (\\?\) looks similar but does not address named pipes.
    wchar_t path[]{ L"\\\\.\\pipe\\discord-ipc-0" };
    for ( wchar_t index{ L'0' }; index <= L'9'; ++index )
    {
        path[ 21 ] = index;
        const HANDLE pipe{ CreateFileW( path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                       OPEN_EXISTING, 0, nullptr ) };
        if ( pipe == INVALID_HANDLE_VALUE ) continue;

        char handshake[ 96 ]{ };
        atl::format( handshake, sizeof( handshake ),
                     ASTRAL_XORSTR( "{\"v\":1,\"client_id\":\"%s\"}" ).get( ), APPLICATION_ID );
        if ( !send_frame( pipe, OPCODE_HANDSHAKE, handshake ) )
        {
            CloseHandle( pipe );
            continue;
        }

        m_pipe = pipe;
        return true;
    }
    return false;
}

bool discord_rpc::service_incoming( )
{
    const HANDLE pipe{ static_cast< HANDLE >( m_pipe ) };
    for ( i32 frame{ }; frame < 4; ++frame )
    {
        frame_header_t header{ };
        DWORD          available{ };
        DWORD          peeked{ };
        if ( !PeekNamedPipe( pipe, &header, sizeof( header ), &peeked, &available, nullptr ) ) return false;
        if ( available < sizeof( header ) || peeked < sizeof( header ) ) return true;
        if ( header.m_length > MAX_PAYLOAD ) return false;
        if ( available < sizeof( header ) + header.m_length ) return true;

        DWORD read{ };
        if ( !ReadFile( pipe, &header, sizeof( header ), &read, nullptr ) || read != sizeof( header ) ) return false;
        char payload[ MAX_PAYLOAD + 1 ]{ };
        if ( header.m_length &&
             ( !ReadFile( pipe, payload, header.m_length, &read, nullptr ) || read != header.m_length ) ) return false;
        payload[ header.m_length ] = 0;

        if ( header.m_opcode == OPCODE_PING && !send_frame( pipe, OPCODE_PONG, payload ) ) return false;
        if ( header.m_opcode == OPCODE_CLOSE ) return false;
    }
    return true;
}

bool discord_rpc::send_activity( )
{
    const bool in_game{ g_tf2.m_engine && g_tf2.m_engine->IsInGame( ) };
    char       details[ 128 ]{ };
    char       state[ 128 ]{ };

    if ( !in_game )
    {
        str::copy( details, sizeof( details ), ASTRAL_XORSTR( "On the main menu" ).get( ) );
        str::copy( state, sizeof( state ), ASTRAL_XORSTR( "Waiting to play" ).get( ) );
    }
    else
    {
        const recorder_status_t recorder{ g_recorder.status( ) };
        if ( g_cfg.m_misc.m_discord_rpc_map && !recorder.m_map.empty( ) )
        {
            atl::format( details, sizeof( details ), ASTRAL_XORSTR( "In game - %s" ).get( ), recorder.m_map.c_str( ) );
        }
        else
        {
            str::copy( details, sizeof( details ), ASTRAL_XORSTR( "In game" ).get( ) );
        }

        const i32 kills{ g_cl.m_local ? g_cl.m_local->m_iKills( ) : 0 };
        const i32 deaths{ g_cl.m_local ? g_cl.m_local->m_iDeaths( ) : 0 };
        const u64 elapsed{ m_match_started ? ( GetTickCount64( ) - m_match_started ) / 1000 : 0 };
        if ( g_cfg.m_misc.m_discord_rpc_class && g_cl.m_local )
        {
            atl::format( state, sizeof( state ), ASTRAL_XORSTR( "K/D %d/%d | %02llu:%02llu | %s" ).get( ),
                         kills, deaths, elapsed / 60, elapsed % 60, class_name( g_cl.m_local->m_iClass( ) ) );
        }
        else
        {
            atl::format( state, sizeof( state ), ASTRAL_XORSTR( "K/D %d/%d | %02llu:%02llu" ).get( ),
                         kills, deaths, elapsed / 60, elapsed % 60 );
        }
    }

    char activity[ 768 ]{ };
    atl::format( activity, sizeof( activity ),
                 ASTRAL_XORSTR( "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":%lu,\"activity\":{\"type\":0,\"details\":\"%s\",\"state\":\"%s\",\"assets\":{\"large_image\":\"logo\",\"large_text\":\"AstralMod\"}}},\"nonce\":\"astral-presence\"}" ).get( ),
                 GetCurrentProcessId( ), details, state );
    return send_frame( static_cast< HANDLE >( m_pipe ), OPCODE_FRAME, activity );
}

void discord_rpc::service( )
{
    const u64 now{ GetTickCount64( ) };
    const bool in_game{ g_tf2.m_engine && g_tf2.m_engine->IsInGame( ) };
    if ( in_game && !m_was_in_game ) m_match_started = now;
    if ( !in_game ) m_match_started = 0;
    m_was_in_game = in_game;

    if ( !g_cfg.m_misc.m_discord_rpc )
    {
        this->disconnect( );
        return;
    }

    if ( static_cast< HANDLE >( m_pipe ) == INVALID_HANDLE_VALUE )
    {
        if ( now < m_next_connect ) return;
        m_next_connect = now + 5000;
        if ( !this->connect( ) ) return;
        m_next_update = 0;
    }

    if ( !this->service_incoming( ) )
    {
        this->disconnect( );
        return;
    }

    if ( now >= m_next_update )
    {
        m_next_update = now + 5000;
        if ( !this->send_activity( ) ) this->disconnect( );
    }
}
