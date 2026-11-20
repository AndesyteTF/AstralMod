#pragma once

class IGameEvent;

void draw_astral_sense( );
void astral_sense_player_death( IGameEvent *event );
void astral_sense_game_event( IGameEvent *event );
void astral_sense_level_init( const char *map_name );
