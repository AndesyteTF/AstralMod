#include "base.hpp"
#include "util/base64.hpp"

#include "player_card_image.hpp"

ASTRAL_NO_DESTROY player_card_image g_player_card_image{ };

namespace
{
    constexpr usize PATH_CAPACITY{ 1024 };
    constexpr usize FILE_LIMIT{ 32u * 1024u * 1024u };

    usize image_folder( wchar_t ( &path )[ PATH_CAPACITY ] )
    {
        u32 length{ I( GetModuleFileNameW )( nullptr, path, PATH_CAPACITY - 300 ) };
        if ( !length || length >= PATH_CAPACITY - 300 ) return 0;
        while ( length && path[ length - 1 ] != L'\\' ) --length;
        path[ length ] = L'\0';
        str::append( path, PATH_CAPACITY, ASTRAL_XORWSTR( L"AstralMod\\PlayerCards" ).get( ) );
        I( CreateDirectoryW )( path, nullptr );
        length = str::length( path );
        path[ length++ ] = L'\\'; path[ length ] = L'\0';
        return length;
    }

    const char *mime_for( const wchar_t *name, usize length )
    {
        const wchar_t *dot{ };
        for ( usize index{ }; index < length; ++index ) if ( name[ index ] == L'.' ) dot = name + index + 1;
        if ( !dot ) return nullptr;
        if ( str::equals_ignore_case( dot, L"png" ) ) return "image/png";
        if ( str::equals_ignore_case( dot, L"jpg" ) || str::equals_ignore_case( dot, L"jpeg" ) ) return "image/jpeg";
        if ( str::equals_ignore_case( dot, L"gif" ) ) return "image/gif";
        if ( str::equals_ignore_case( dot, L"webp" ) ) return "image/webp";
        if ( str::equals_ignore_case( dot, L"bmp" ) ) return "image/bmp";
        return nullptr;
    }

    bool read_file( const wchar_t *path, atl::vector< u8 > &bytes )
    {
        void *file{ I( CreateFileW )( path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL, nullptr ) };
        if ( file == INVALID_HANDLE_VALUE ) return false;
        LARGE_INTEGER size{ }; DWORD read{ };
        const bool ok{ I( GetFileSizeEx )( file, &size ) && size.QuadPart > 0 &&
                       size.QuadPart <= static_cast< i64 >( FILE_LIMIT ) &&
                       bytes.resize( static_cast< usize >( size.QuadPart ) ) &&
                       I( ReadFile )( file, bytes.data( ), static_cast< DWORD >( size.QuadPart ), &read, nullptr ) &&
                       read == size.QuadPart };
        I( CloseHandle )( file );
        return ok;
    }
}

void player_card_image::refresh( )
{
    this->m_count = 0;
    wchar_t pattern[ PATH_CAPACITY ]{ };
    if ( !image_folder( pattern ) ) return;
    str::append( pattern, PATH_CAPACITY, L"*" );
    WIN32_FIND_DATAW found{ };
    void *search{ I( FindFirstFileW )( pattern, &found ) };
    if ( search == INVALID_HANDLE_VALUE ) return;
    do
    {
        const usize length{ str::length( found.cFileName ) };
        const char *mime{ mime_for( found.cFileName, length ) };
        const u64 size{ static_cast< u64 >( found.nFileSizeHigh ) << 32 | found.nFileSizeLow };
        if ( ( found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) || !mime || !size || size > FILE_LIMIT || length >= 260 ) continue;
        char name[ 64 ]{ }; atl::to_ansi( found.cFileName, name, sizeof( name ) );
        usize slot{ this->m_count };
        while ( slot && str::compare_ignore_case( this->m_images[ slot - 1 ].m_name.c_str( ), name ) > 0 )
        { this->m_images[ slot ] = this->m_images[ slot - 1 ]; --slot; }
        str::copy( this->m_images[ slot ].m_file, 260, found.cFileName );
        this->m_images[ slot ].m_name.assign( name ); this->m_images[ slot ].m_mime = mime;
        ++this->m_count;
    } while ( this->m_count < PLAYER_CARD_IMAGE_CAPACITY && I( FindNextFileW )( search, &found ) );
    I( FindClose )( search );
    for ( usize index{ }; index < this->m_count; ++index ) this->m_labels[ index ] = this->m_images[ index ].m_name.c_str( );
    this->m_loaded = this->m_failed = this->m_upload_index = PLAYER_CARD_IMAGE_CAPACITY;
    this->m_image_ready = false;
}

bool player_card_image::load( usize index )
{
    if ( index >= this->m_count ) return false;
    wchar_t path[ PATH_CAPACITY ]{ };
    if ( !image_folder( path ) ) return false;
    str::append( path, PATH_CAPACITY, this->m_images[ index ].m_file );
    atl::vector< u8 > bytes{ };
    if ( !read_file( path, bytes ) ) { this->m_failed = index; return false; }
    this->m_upload_data.clear( );
    if ( !base64::append( bytes.data( ), bytes.size( ), this->m_upload_data ) ) return false;
    if ( this->m_panel.m_slot >= HTML_PANEL_CAPACITY )
    {
        const char *document{
            "<html><style>html,body{margin:0;width:100%;height:100%;overflow:hidden;background:transparent}"
            "img{width:100%;height:100%;object-fit:cover;border-radius:50%}</style><img id='i' "
            "onload=\"astral.send('loaded')\" onerror=\"astral.send('error')\"><script>let p=[];"
            "astral.receive=function(m){if(m==='reset'){p=[];return;}if(m.indexOf('done:')===0){"
            "document.getElementById('i').src='data:'+m.slice(5)+';base64,'+p.join('');p=[];return;}p.push(m);};"
            "astral.send('ready');</script></html>"
        };
        this->m_panel = g_html.open( 128, 128, document, false );
        if ( this->m_panel.m_slot >= HTML_PANEL_CAPACITY ) { this->m_failed = index; return false; }
        this->m_panel_ready = false;
    }
    this->m_upload_index = index; this->m_upload_cursor = 0; this->m_upload_phase = 0;
    this->m_upload_mime = this->m_images[ index ].m_mime; this->m_image_ready = false;
    return true;
}

void player_card_image::paint( rect_t bounds, u8 opacity, usize selected )
{
    if ( selected >= this->m_count || this->m_failed == selected ) return;
    if ( this->m_loaded != selected && this->m_upload_index != selected && !this->load( selected ) ) return;
    fixed_string< HTML_MESSAGE_LENGTH > message{ };
    if ( g_html.receive( this->m_panel, message ) )
    {
        if ( str::equals( message.c_str( ), ASTRAL_XORSTR_STATIC( "ready" ) ) ) this->m_panel_ready = true;
        else if ( str::equals( message.c_str( ), ASTRAL_XORSTR_STATIC( "loaded" ) ) ) this->m_image_ready = true;
        else if ( str::equals( message.c_str( ), ASTRAL_XORSTR_STATIC( "error" ) ) ) { this->m_failed = selected; this->m_image_ready = false; }
    }
    if ( this->m_upload_index == selected && this->m_panel_ready )
    {
        constexpr usize CHUNK{ 24u * 1024u };
        if ( !this->m_upload_phase ) { if ( !g_html.send( this->m_panel, "reset" ) ) return; this->m_upload_phase = 1; }
        for ( i32 sent{ }; sent < 4 && this->m_upload_cursor < this->m_upload_data.size( ); ++sent )
        {
            const usize amount{ math::smaller( CHUNK, this->m_upload_data.size( ) - this->m_upload_cursor ) };
            if ( !g_html.send( this->m_panel, string_view{ this->m_upload_data.data( ) + this->m_upload_cursor, amount } ) ) return;
            this->m_upload_cursor += amount;
        }
        if ( this->m_upload_cursor == this->m_upload_data.size( ) )
        {
            char done[ 48 ]{ }; str::format_to( done, sizeof( done ), ASTRAL_XORSTR( "done:{}" ).get( ), this->m_upload_mime );
            if ( !g_html.send( this->m_panel, done ) ) return;
            this->m_loaded = selected; this->m_upload_index = PLAYER_CARD_IMAGE_CAPACITY;
            this->m_upload_data.reset( );
        }
    }
    if ( this->ready( selected ) ) g_render.html( this->m_panel.m_slot, bounds, opacity );
}
