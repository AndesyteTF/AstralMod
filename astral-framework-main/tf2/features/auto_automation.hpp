#pragma once

#include "link.hpp"

class auto_automation
{
public:
    void think( );
    void on_vote_start( bf_read &msg );

private:
    bool protected_player( u32 account ) const;
    const char *class_name( i32 player_class ) const;

    double m_next_join{ };
};

extern auto_automation g_auto_automation;
