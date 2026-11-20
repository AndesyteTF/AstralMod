#pragma once

#include "link.hpp"

// Fires the current weapon the moment the crosshair covers a valid enemy.
// Runs after prediction, before crits, so the crit system can still act on our
// attack bits. With no key bound it stays active the whole time, like airblast.
class triggerbot
{
public:
    void run( );

private:
    bool player_allowed( C_TFPlayer *player ) const;
    bool hitbox_allowed( i32 hitbox ) const;
    bool over_player( const vec3 &from, const vec3 &end ) const;
    bool over_building( const vec3 &from, const vec3 &end ) const;
};

extern triggerbot g_triggerbot;
