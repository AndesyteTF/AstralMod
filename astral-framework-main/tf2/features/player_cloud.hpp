#pragma once

#include "runtime/mutex.hpp"
#include "runtime/vector.hpp"

#include "link.hpp"

// Private noncontinuable exception used only for denied local accounts. The
// process-level handler consumes it before normal crash diagnostics run.
inline constexpr u32 ASTRAL_ACCESS_DENIED_EXCEPTION{ 0xE0421001u };

class player_cloud
{
public:
    enum mark_t : u32
    {
        mark_cheater = 1u << 0, mark_suspicious = 1u << 1, mark_exploiter = 1u << 2,
        mark_racist = 1u << 3, mark_hostile = 1u << 4, mark_suspected_cheater = 1u << 5,
        mark_blacklisted = 1u << 6, mark_vac_banned = 1u << 7, mark_game_banned = 1u << 8,
        mark_sourcebanned = 1u << 9, mark_pedophilia = 1u << 10, mark_ulistuser = 1u << 11,
        mark_zoophile_zoojokes = 1u << 12, mark_all = ( 1u << 13 ) - 1,
        mark_cheating = mark_cheater,
    };
    enum class report_result_t : u8
    {
        started,
        no_session,
        busy,
        invalid,
        unavailable,
    };

    void poll( );
    void on_level_shutdown( );
    void on_player_connected( i32 index );
    void on_player_disconnected( i32 index );
    bool known( u32 account, char ( &alias )[ 129 ] ) const;
    u32 marks( u32 account, char ( &alias )[ 129 ] ) const;
    bool remove_mark( u32 account, u32 mark );
    bool set_local_alias( u32 account, string_view alias );
    bool refresh_lists( );
    void set_integration_keys( string_view steam, string_view steam_history );
    bool load_tf2pl_file( string_view path );
    bool has_steam_key( ) const;
    bool has_steam_history_key( ) const;
    report_result_t submit_report( u32 account, string_view name, string_view reason,
                                   string_view alias );

private:
    enum class operation_t : u8
    {
        list,
        integrations,
        report,
    };

    struct known_t
    {
        u32  m_account{ };
        u32  m_marks{ mark_cheater };
        char m_alias[ 129 ]{ };
    };

    struct report_t
    {
        u32  m_account{ };
        char m_name[ 64 ]{ };
        char m_reason[ 256 ]{ };
        char m_alias[ 33 ]{ };
    };

    static void ASTRAL_STDCALL worker( void *context );
    bool start( operation_t operation );
    bool parse_list( atl::vector< known_t > *destination = nullptr );
    bool parse_synced_list( atl::vector< char > &response );
    bool parse_integrations( );
    void queue_integrations( );
    bool queue_integration( u32 account, string_view name );
    bool scan_player( i32 index );
    void scan_players( );
    void scan_pending_players( );
    bool local_save( ) const; // caller must hold m_publish
    bool access_allowed( );

    mutable atl::mutex   m_publish{ };
    atl::vector< known_t > m_known{ };
    atl::vector< known_t > m_integrated{ };
    atl::vector< char >    m_response{ };
    atl::vector< char >    m_history_response{ };
    atl::vector< char >    m_tf2pl_response{ };
    atl::vector< char >    m_surface_response{ };
    known_t                m_integration_players[ 100 ]{ };
    usize                  m_integration_count{ };
    report_t               m_report{ };
    volatile long          m_state{ };
    u32                    m_status{ };
    u32                    m_notified[ MAX_TRACKED_PLAYERS ]{ };
    u32                    m_integration_seen[ MAX_TRACKED_PLAYERS ]{ };
    bool                   m_pending_players[ MAX_TRACKED_PLAYERS ]{ };
    operation_t            m_operation{ operation_t::list };
    bool                   m_success{ };
    bool                   m_loaded{ };
    bool                   m_tf2pl_fetched{ };
    bool                   m_surface_fetched{ };
    bool                   m_list_started{ };
    bool                   m_in_game{ };
    bool                   m_access_checked{ };
    bool                   m_access_denied{ };
    char                   m_steam_key[ 65 ]{ };
    char                   m_steam_history_key[ 129 ]{ };
};

extern player_cloud g_player_cloud;
//FWork - kill me, why did i spend the time doing this
