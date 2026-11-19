#pragma once

inline constexpr usize EXTERNAL_MEDIA_TEXT_CAPACITY{ 256 };

struct external_media_snapshot_t
{
    bool available{ };
    bool playing{ };
    bool can_previous{ };
    bool can_play_pause{ };
    bool can_next{ };
    char title[ EXTERNAL_MEDIA_TEXT_CAPACITY ]{ };
    char artist[ EXTERNAL_MEDIA_TEXT_CAPACITY ]{ };
    char source[ EXTERNAL_MEDIA_TEXT_CAPACITY ]{ };
};

class external_media
{
public:
    bool startup( );
    void shutdown( );
    bool snapshot( external_media_snapshot_t &out ) const;
    void previous( );
    void toggle_play_pause( );
    void next( );

private:
    void *m_state{ };
    void *m_manager{ };
};

