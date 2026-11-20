#pragma once

class discord_rpc
{
public:
    void service( );

private:
    void disconnect( );
    bool connect( );
    bool send_activity( );
    bool service_incoming( );

    void    *m_pipe{ reinterpret_cast< void * >( -1 ) };
    unsigned long long m_next_connect{ };
    unsigned long long m_next_update{ };
    unsigned long long m_match_started{ };
    bool     m_was_in_game{ };
};

inline discord_rpc g_discord_rpc;
