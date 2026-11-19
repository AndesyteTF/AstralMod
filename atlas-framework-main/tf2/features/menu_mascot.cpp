#include "base.hpp"
#include "util/base64.hpp"

#include "menu_mascot.hpp"

ATLAS_NO_DESTROY menu_mascot g_menu_mascot{ };

namespace
{
constexpr usize MASCOT_PATH_CAPACITY{ 1024 };
// This is a corruption/OOM guard, not a normal content limit. Only the selected file is loaded.
constexpr usize MASCOT_FILE_LIMIT{ 32u * 1024u * 1024u };

usize mascot_folder( wchar_t ( &path )[ MASCOT_PATH_CAPACITY ] )
{
    u32 length{ I( GetModuleFileNameW )( nullptr, path, MASCOT_PATH_CAPACITY - MASCOT_FILE_LENGTH - 20 ) };
    if ( !length || length >= MASCOT_PATH_CAPACITY - MASCOT_FILE_LENGTH - 20 )
    {
        return 0;
    }
    while ( length && path[ length - 1 ] != L'\\' )
    {
        --length;
    }
    path[ length ] = L'\0';
    str::append( path, MASCOT_PATH_CAPACITY, ATLAS_XORWSTR( L"AstralMod\\Mascots" ).get( ) );
    I( CreateDirectoryW )( path, nullptr );
    length = str::length( path );
    path[ length++ ] = L'\\';
    path[ length ] = L'\0';
    return length;
}

const char *image_mime( const wchar_t *name, usize length )
{
    const wchar_t *dot{ };
    for ( usize index{ }; index < length; ++index )
    {
        if ( name[ index ] == L'.' ) dot = name + index + 1;
    }
    if ( !dot ) return nullptr;
    if ( str::equals_ignore_case( dot, L"png" ) ) return "image/png";
    if ( str::equals_ignore_case( dot, L"jpg" ) || str::equals_ignore_case( dot, L"jpeg" ) ) return "image/jpeg";
    if ( str::equals_ignore_case( dot, L"gif" ) ) return "image/gif";
    if ( str::equals_ignore_case( dot, L"webp" ) ) return "image/webp";
    if ( str::equals_ignore_case( dot, L"bmp" ) ) return "image/bmp";
    return nullptr;
}

bool read_image( const wchar_t *path, atl::vector< u8 > &bytes )
{
    void *file{ I( CreateFileW )( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr ) };
    if ( file == INVALID_HANDLE_VALUE ) return false;
    LARGE_INTEGER size{ };
    DWORD read{ };
    const bool ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart > 0 &&
                   size.QuadPart <= static_cast< i64 >( MASCOT_FILE_LIMIT ) &&
                   bytes.resize( static_cast< usize >( size.QuadPart ) ) &&
                   I( ReadFile )( file, bytes.data( ), static_cast< DWORD >( size.QuadPart ), &read, nullptr ) &&
                   read == size.QuadPart };
    I( CloseHandle )( file );
    return ok;
}
}

void menu_mascot::refresh( )
{
    this->close( );
    this->m_count = 0;
    usize oversized{ };
    wchar_t pattern[ MASCOT_PATH_CAPACITY ]{ };
    if ( !mascot_folder( pattern ) ) return;
    str::append( pattern, MASCOT_PATH_CAPACITY, L"*" );
    WIN32_FIND_DATAW found{ };
    void *search{ I( FindFirstFileW )( pattern, &found ) };
    if ( search == INVALID_HANDLE_VALUE ) return;
    do
    {
        const usize length{ str::length( found.cFileName ) };
        const char *mime{ image_mime( found.cFileName, length ) };
        if ( ( found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) || !mime || length >= MASCOT_FILE_LENGTH ) continue;
        const u64 file_size{ static_cast< u64 >( found.nFileSizeHigh ) << 32 | found.nFileSizeLow };
        if ( !file_size || file_size > MASCOT_FILE_LIMIT )
        {
            ++oversized;
            continue;
        }
        char name[ MASCOT_NAME_LENGTH + 1 ]{ };
        atl::to_ansi( found.cFileName, name, sizeof( name ) );
        usize slot{ this->m_count };
        while ( slot && str::compare_ignore_case( this->m_images[ slot - 1 ].m_name.c_str( ), name ) > 0 )
        {
            this->m_images[ slot ] = this->m_images[ slot - 1 ];
            --slot;
        }
        str::copy( this->m_images[ slot ].m_file, MASCOT_FILE_LENGTH, found.cFileName );
        this->m_images[ slot ].m_name.assign( name );
        this->m_images[ slot ].m_mime = mime;
        ++this->m_count;
    } while ( this->m_count < MASCOT_CAPACITY && I( FindNextFileW )( search, &found ) );
    I( FindClose )( search );
    for ( usize index{ }; index < this->m_count; ++index ) this->m_labels[ index ] = this->m_images[ index ].m_name.c_str( );
    if ( this->m_count ) g_cfg.m_misc.m_menu_mascot_file = math::smaller( g_cfg.m_misc.m_menu_mascot_file, this->m_count - 1 );

    char message[ 128 ]{ };
    str::format_to( message, sizeof( message ),
                    oversized ? ATLAS_XORSTR( "Found {} image(s); skipped {} empty/over 32 MB" ).get( )
                              : ATLAS_XORSTR( "Found {} image(s) in the TF2 AstralMod/Mascots folder" ).get( ),
                    this->m_count, oversized );
    push_notification( this->m_count ? notify_kind_t::success : notify_kind_t::warning,
                       ICON_SPARKLES, ATLAS_XORSTR_STATIC( "Mascot refresh" ), message );
}

bool menu_mascot::load( usize index )
{
    if ( index >= this->m_count ) return false;
    wchar_t path[ MASCOT_PATH_CAPACITY ]{ };
    const usize folder{ mascot_folder( path ) };
    if ( !folder )
    {
        this->m_failed = index;
        push_notification( notify_kind_t::error, ICON_SPARKLES, ATLAS_XORSTR_STATIC( "Mascot" ),
                           ATLAS_XORSTR_STATIC( "Could not resolve the TF2 AstralMod/Mascots folder" ) );
        return false;
    }
    str::append( path, MASCOT_PATH_CAPACITY, this->m_images[ index ].m_file );
    atl::vector< u8 > bytes{ };
    if ( !read_image( path, bytes ) )
    {
        this->m_failed = index;
        push_notification( notify_kind_t::error, ICON_SPARKLES, ATLAS_XORSTR_STATIC( "Mascot" ),
                           ATLAS_XORSTR_STATIC( "Could not read the selected image (empty, unavailable, or over 32 MB)" ) );
        return false;
    }

    this->m_upload_data.clear( );
    if ( !base64::append( bytes.data( ), bytes.size( ), this->m_upload_data ) ) return false;

    if ( this->m_panel.m_slot >= HTML_PANEL_CAPACITY )
    {
        const char *document{
            "<html><style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent}"
            "img{width:100%;height:100%;object-fit:contain;object-position:center bottom}</style>"
            "<img id='mascot' onload=\"atlas.send('image-loaded:'+this.naturalWidth+':'+this.naturalHeight)\" onerror=\"atlas.send('image-error')\">"
            "<script>let mascotParts=[];atlas.receive=function(m){if(m==='__reset__'){mascotParts=[];return;}"
            "if(m.indexOf('__done__:')===0){document.getElementById('mascot').src='data:'+m.slice(9)+';base64,'+mascotParts.join('');mascotParts=[];return;}mascotParts.push(m);};atlas.send('mascot-ready');</script></html>"
        };
        this->m_panel = g_html.open( 512, 768, document, false );
        if ( this->m_panel.m_slot >= HTML_PANEL_CAPACITY )
        {
            this->m_failed = index;
            push_notification( notify_kind_t::error, ICON_SPARKLES, ATLAS_XORSTR_STATIC( "Mascot" ),
                               ATLAS_XORSTR_STATIC( "No HTML panel slot is available; reload the module once to clear old panels" ) );
            return false;
        }
        this->m_panel_ready = false;
    }

    this->m_upload_index = index;
    this->m_upload_cursor = 0;
    this->m_upload_phase = 0;
    this->m_upload_mime = this->m_images[ index ].m_mime;
    this->m_failed = MASCOT_CAPACITY;
    return true;
}

void menu_mascot::paint( )
{
    if ( !g_cfg.m_misc.m_menu_mascot || !this->m_count )
    {
        this->m_reveal = 0.0f;
        this->m_closing = false;
        return;
    }
    const usize selected{ math::smaller( g_cfg.m_misc.m_menu_mascot_file, this->m_count - 1 ) };
    const double now{ g_tf2.m_engine->Plat_FloatTime( ) };
    const float elapsed{ this->m_animation_time > 0.0
                             ? math::clamp( static_cast< float >( now - this->m_animation_time ), 0.0f, 0.05f )
                             : 0.0f };
    this->m_animation_time = now;
    if ( this->m_pending != selected )
    {
        this->m_pending = selected;
        this->m_switch_time = now;
        this->m_failed = MASCOT_CAPACITY;
        this->m_image_ready = false;
        this->m_reveal = 0.0f;
        return;
    }
    if ( this->m_failed == selected ) return;
    if ( this->m_loaded != selected && this->m_upload_index != selected )
    {
        if ( now - this->m_switch_time < 0.2 || !this->load( selected ) ) return;
    }

    fixed_string< HTML_MESSAGE_LENGTH > browser_message{ };
    if ( g_html.receive( this->m_panel, browser_message ) )
    {
        if ( str::equals( browser_message.c_str( ), ATLAS_XORSTR_STATIC( "mascot-ready" ) ) )
        {
            this->m_panel_ready = true;
        } else if ( str::scan( browser_message.c_str( ), ATLAS_XORSTR_STATIC( "image-loaded:{}:{}" ),
                               this->m_image_width, this->m_image_height ) )
        {
            this->m_image_ready = this->m_image_width > 0 && this->m_image_height > 0;
        } else if ( str::equals( browser_message.c_str( ), ATLAS_XORSTR_STATIC( "image-error" ) ) )
        {
            this->m_image_ready = false;
            this->m_failed = selected;
            push_notification( notify_kind_t::error, ICON_SPARKLES, ATLAS_XORSTR_STATIC( "Mascot" ),
                               ATLAS_XORSTR_STATIC( "Steam's image decoder could not decode the selected file" ) );
            return;
        }
    }

    if ( this->m_upload_index == selected && this->m_panel_ready )
    {
        constexpr usize CHUNK_SIZE{ 24u * 1024u };
        if ( this->m_upload_phase == 0 )
        {
            if ( !g_html.send( this->m_panel, ATLAS_XORSTR_STATIC( "__reset__" ) ) ) return;
            this->m_upload_phase = 1;
        }
        for ( i32 sent{ }; this->m_upload_phase == 1 && sent < 4 &&
                           this->m_upload_cursor < this->m_upload_data.size( ); ++sent )
        {
            const usize amount{ math::smaller( CHUNK_SIZE,
                                               this->m_upload_data.size( ) - this->m_upload_cursor ) };
            if ( !g_html.send( this->m_panel,
                               string_view{ this->m_upload_data.data( ) + this->m_upload_cursor, amount } ) ) return;
            this->m_upload_cursor += amount;
        }
        if ( this->m_upload_phase == 1 && this->m_upload_cursor == this->m_upload_data.size( ) )
        {
            this->m_upload_phase = 2;
        }
        if ( this->m_upload_phase == 2 )
        {
            char done[ 48 ]{ };
            str::format_to( done, sizeof( done ), ATLAS_XORSTR( "__done__:{}" ).get( ), this->m_upload_mime );
            if ( !g_html.send( this->m_panel, done ) ) return;
            this->m_loaded = selected;
            this->m_upload_index = MASCOT_CAPACITY;
            this->m_upload_data.reset( );
        }
    }

    const bool reveal{ g_gui.can_interact( ) && this->m_image_ready && !this->m_closing };
    const float rate{ reveal ? 3.6f : 10.0f };
    if ( reveal )
    {
        this->m_reveal = math::smaller( 1.0f, this->m_reveal + elapsed * rate );
    } else
    {
        this->m_reveal = math::clamp( this->m_reveal - elapsed * rate, 0.0f, 1.0f );
    }
    if ( this->m_reveal <= 0.0f || !this->m_image_ready ) return;

    const float travel{ math::smooth_step( this->m_reveal ) };
    const i32 width{ g_gui.scaled( math::clamp( g_cfg.m_misc.m_menu_mascot_size, 100, 400 ) ) };
    const i32 height{ width * 3 / 2 };
    const rect_t menu{ g_gui.bounds( ) };
    const i32 offset{ g_gui.scaled( math::clamp( g_cfg.m_misc.m_menu_mascot_offset, -500, 500 ) ) };
    const i32 spacing{ g_gui.scaled( math::clamp( g_cfg.m_misc.m_menu_mascot_spacing, -200, 200 ) ) };
    i32 content_width{ width };
    i32 content_height{ height };
    i32 content_left{ };
    i32 content_top{ };
    if ( static_cast< i64 >( this->m_image_width ) * height >
         static_cast< i64 >( this->m_image_height ) * width )
    {
        content_height = static_cast< i32 >( static_cast< i64 >( width ) * this->m_image_height /
                                             this->m_image_width );
        content_top = height - content_height;
    } else
    {
        content_width = static_cast< i32 >( static_cast< i64 >( height ) * this->m_image_width /
                                            this->m_image_height );
        content_left = ( width - content_width ) / 2;
    }

    const i32 hidden_left{ ( menu.left + menu.right ) / 2 - content_left - content_width / 2 };
    const i32 hidden_top{ ( menu.top + menu.bottom ) / 2 - content_top - content_height / 2 };
    i32 final_left{ hidden_left };
    i32 final_top{ hidden_top };
    switch ( math::clamp< usize >( g_cfg.m_misc.m_menu_mascot_side, 0, 3 ) )
    {
        case 0:
            final_left = menu.left - spacing - content_left - content_width;
            final_top = ( menu.top + menu.bottom ) / 2 + offset - content_top - content_height / 2;
            break;
        case 1:
            final_left = menu.right + spacing - content_left;
            final_top = ( menu.top + menu.bottom ) / 2 + offset - content_top - content_height / 2;
            break;
        case 2:
            final_left = ( menu.left + menu.right ) / 2 + offset - content_left - content_width / 2;
            final_top = menu.top - spacing - content_top - content_height;
            break;
        case 3:
            final_left = ( menu.left + menu.right ) / 2 + offset - content_left - content_width / 2;
            final_top = menu.bottom + spacing - content_top;
            break;
        default: break;
    }
    const i32 left{ hidden_left + static_cast< i32 >( ( final_left - hidden_left ) * travel ) };
    const i32 top{ hidden_top + static_cast< i32 >( ( final_top - hidden_top ) * travel ) };
    g_render.html( this->m_panel.m_slot,
                   rect_t{ left, top, left + width, top + height }, g_gui.opacity( ) );
}

bool menu_mascot::allow_menu_close( )
{
    if ( !g_cfg.m_misc.m_menu_mascot || !this->m_count || !this->m_image_ready || this->m_reveal <= 0.0f )
    {
        this->m_closing = false;
        this->m_close_deadline = 0;
        return true;
    }

    const u64 now{ I( GetTickCount64 )( ) };
    if ( !this->m_closing )
    {
        this->m_closing = true;
        this->m_close_deadline = now + 125u;
    }
    if ( now >= this->m_close_deadline )
    {
        this->m_reveal = 0.0f;
        this->m_closing = false;
        this->m_close_deadline = 0;
        return true;
    }
    return false;
}

void menu_mascot::close( )
{
    if ( this->m_panel.m_slot < HTML_PANEL_CAPACITY ) g_html.close( this->m_panel );
    this->m_panel = { };
    this->m_loaded = MASCOT_CAPACITY;
    this->m_failed = MASCOT_CAPACITY;
    this->m_pending = MASCOT_CAPACITY;
    this->m_switch_time = 0.0;
    this->m_upload_index = MASCOT_CAPACITY;
    this->m_upload_cursor = 0;
    this->m_upload_phase = 0;
    this->m_upload_mime = nullptr;
    this->m_upload_data.reset( );
    this->m_panel_ready = false;
    this->m_image_ready = false;
    this->m_image_width = 0;
    this->m_image_height = 0;
    this->m_closing = false;
    this->m_close_deadline = 0;
    this->m_reveal = 0.0f;
    this->m_animation_time = 0.0;
}
