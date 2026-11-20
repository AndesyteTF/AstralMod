#include "base.hpp"

#include "external_media.hpp"

#include <MusicOverlay/Core/OverlayState.h>
#include "Platform/Windows/MediaSessionManager.h"

bool external_media::startup( )
{
    if ( this->m_manager ) return true;

    auto *state{ new mo::OverlayState{ } };
    auto *manager{ new mo::win::MediaSessionManager{ *state } };
    if ( !state || !manager )
    {
        delete manager;
        delete state;
        return false;
    }

    manager->start( );
    this->m_state   = state;
    this->m_manager = manager;
    return true;
}

void external_media::shutdown( )
{
    auto *manager{ static_cast< mo::win::MediaSessionManager * >( this->m_manager ) };
    auto *state{ static_cast< mo::OverlayState * >( this->m_state ) };
    if ( manager ) manager->stop( );
    delete manager;
    delete state;
    this->m_manager = nullptr;
    this->m_state   = nullptr;
}

bool external_media::snapshot( external_media_snapshot_t &out ) const
{
    out = external_media_snapshot_t{ };
    const auto *state{ static_cast< const mo::OverlayState * >( this->m_state ) };
    if ( !state ) return false;

    const mo::TrackInfo track{ state->snapshot( ) };
    out.available      = !track.title.empty( ) || !track.artist.empty( ) || !track.sourceAppId.empty( );
    out.playing        = track.playing;
    out.can_previous   = track.canPrevious;
    out.can_play_pause = track.canPlayPause;
    out.can_next       = track.canNext;

    if ( !track.title.empty( ) ) atl::to_ansi( track.title.c_str( ), out.title, sizeof( out.title ) );
    if ( !track.artist.empty( ) ) atl::to_ansi( track.artist.c_str( ), out.artist, sizeof( out.artist ) );
    if ( !track.sourceAppId.empty( ) ) atl::to_ansi( track.sourceAppId.c_str( ), out.source, sizeof( out.source ) );
    return true;
}

void external_media::previous( )
{
    if ( auto *manager{ static_cast< mo::win::MediaSessionManager * >( this->m_manager ) } ) manager->previous( );
}

void external_media::toggle_play_pause( )
{
    if ( auto *manager{ static_cast< mo::win::MediaSessionManager * >( this->m_manager ) } ) manager->togglePlayPause( );
}

void external_media::next( )
{
    if ( auto *manager{ static_cast< mo::win::MediaSessionManager * >( this->m_manager ) } ) manager->next( );
}

