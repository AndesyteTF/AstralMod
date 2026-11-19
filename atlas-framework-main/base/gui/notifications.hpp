#pragma once

#include "util/typedefs.hpp"

enum class notify_kind_t : u8
{
    success,
    information,
    warning,
    error,
};

enum class notify_channel_t : u8
{
    system,
    players,
    configs,
    scripts,
    convars,
    audio,
    gameplay,
    visuals,
    count,
};

struct notification_settings_t
{
    bool  m_enabled{ true };
    bool  m_log_enabled{ true };
    bool  m_kinds[ 4 ]{ true, true, true, true };
    bool  m_channels[ static_cast< usize >( notify_channel_t::count ) ]{
        true, true, true, true, true, true, true, true
    };
    i32   m_lifetime_ms{ 4000 };
    usize m_position{ 0 }; // top-right, top-left, bottom-right, bottom-left
};

void push_notification( notify_kind_t kind, const char *title, const char *message );
void push_notification( notify_kind_t kind, notify_channel_t channel, const char *title,
                        const char *message );
void push_notification( notify_kind_t kind, const char *icon, const char *title,
                        const char *message );
void push_notification( notify_kind_t kind, notify_channel_t channel, const char *icon,
                        const char *title, const char *message );
void push_notification_notice( notify_kind_t kind, const char *icon, const char *title,
                               const char *message );

void draw_notifications( i32 screen_width, i32 screen_height );
void configure_notifications( const notification_settings_t &settings );
usize notification_log_count( );
u32 notification_log_revision( );
const char *notification_log_title( usize newest_index );
const char *notification_log_message( usize newest_index );
notify_kind_t notification_log_kind( usize newest_index );
notify_channel_t notification_log_channel( usize newest_index );
void clear_notification_log( );
