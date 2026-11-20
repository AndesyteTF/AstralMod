#pragma once

#include "link.hpp"

class behavior_scanner
{
public:
    void on_network_update( );
    void on_fire_bullets( i32 shooter );
    void clear( );

private:
    struct record_t
    {
        u32    m_handle{ INVALID_EHANDLE_INDEX };
        u32    m_account{ };
        float  m_score{ };
        double m_score_time{ };
        vec3   m_eye{ };
        vec3   m_previous_eye{ };
        i32    m_sim_tick{ };
        i32    m_changed_tick{ };
        i32    m_seen_tick{ };
        i32    m_choke_streak{ };
        bool   m_eligible{ };
        bool   m_tracked{ };
        bool   m_flagged{ };
    };

    void adopt( record_t &record, i32 index, u32 handle );
    void add_score( record_t &record, i32 index, float amount, const char *reason );
    void flag( record_t &record, i32 index, const char *reason );
    bool excluded( const record_t &record ) const;

    record_t m_records[ MAX_TRACKED_PLAYERS ]{ };
};

extern behavior_scanner g_behavior_scanner;
