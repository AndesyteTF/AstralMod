#include "base.hpp"

#include "../config.hpp"
#include "crash_reporter.hpp"

namespace
{
    constexpr u32 CRASH_MAGIC{ 0x48535243u };
    constexpr u32 CRASH_VERSION{ 1 };
    constexpr u32 SIGNATURE_MAGIC{ 0x46474953u };
    constexpr u32 SIGNATURE_VERSION{ 1 };

    struct crash_record_t
    {
        u32 m_magic, m_version, m_code, m_reserved;
        uptr m_address, m_instruction, m_stack;
    };

    struct signature_record_t
    {
        u32 m_magic, m_version, m_id, m_module, m_reason;
        char m_name[ 128 ];
    };

    bool report_path( wchar_t ( &path )[ 1024 ], const wchar_t *file )
    {
        u32 length{ I( GetModuleFileNameW )( nullptr, path, 900 ) };
        if ( !length || length >= 900 ) return false;
        while ( length && path[ length - 1 ] != L'\\' ) --length;

        const wchar_t folder[]{ L"AstralMod" };
        for ( usize index{ }; index + 1 < sizeof( folder ) / sizeof( folder[ 0 ] ); ++index )
            path[ length++ ] = folder[ index ];
        path[ length ] = 0;
        I( CreateDirectoryW )( path, nullptr );

        while ( *file ) path[ length++ ] = *file++;
        path[ length ] = 0;
        return true;
    }

    bool crash_path( wchar_t ( &path )[ 1024 ] )
    {
        return report_path( path, L"\\pending-crash.bin" );
    }

    bool signature_path( wchar_t ( &path )[ 1024 ] )
    {
        return report_path( path, L"\\pending-signature.bin" );
    }

    bool opt_in_path( wchar_t ( &path )[ 1024 ] )
    {
        return report_path( path, L"\\crash-reports.optin" );
    }

    bool diagnostics_enabled( )
    {
        if ( g_cfg.m_misc.m_crash_reports ) return true;
        wchar_t path[ 1024 ]{ };
        if ( !opt_in_path( path ) ) return false;
        void *file{ I( CreateFileW )( path, 0x80000000u, 1, nullptr, 3, 0x80, nullptr ) };
        if ( file == reinterpret_cast< void * >( -1 ) ) return false;
        I( CloseHandle )( file );
        return true;
    }

    bool fatal_exception( u32 code )
    {
        return code == STATUS_ACCESS_VIOLATION || code == STATUS_STACK_OVERFLOW ||
               code == STATUS_HEAP_CORRUPTION || code == STATUS_ARRAY_BOUNDS_EXCEEDED ||
               code == STATUS_ILLEGAL_INSTRUCTION;
    }
}

void crash_reporter::capture( _EXCEPTION_POINTERS *info )
{
    static volatile long capturing{ };
    if ( !g_cfg.m_misc.m_crash_reports || !info || !info->ExceptionRecord || !info->ContextRecord ||
         !fatal_exception( info->ExceptionRecord->ExceptionCode ) ||
         _InterlockedCompareExchange( &capturing, 1, 0 ) != 0 )
        return;

    crash_record_t record{ CRASH_MAGIC, CRASH_VERSION, info->ExceptionRecord->ExceptionCode, 0,
                           reinterpret_cast< uptr >( info->ExceptionRecord->ExceptionAddress ) };
#if defined( __x86_64__ )
    record.m_instruction = info->ContextRecord->Rip;
    record.m_stack       = info->ContextRecord->Rsp;
#else
    record.m_instruction = info->ContextRecord->Eip;
    record.m_stack       = info->ContextRecord->Esp;
#endif

    wchar_t path[ 1024 ]{ };
    if ( crash_path( path ) )
    {
        void *file{ I( CreateFileW )( path, 0x40000000u, 0, nullptr, 2, 0x80, nullptr ) };
        if ( file != reinterpret_cast< void * >( -1 ) )
        {
            DWORD written{ };
            I( WriteFile )( file, &record, static_cast< DWORD >( sizeof( record ) ), &written, nullptr );
            I( CloseHandle )( file );
        }
    }
}

void crash_reporter::poll( )
{
    static i32 opt_in_state{ -1 };
    const i32 wanted{ g_cfg.m_misc.m_crash_reports ? 1 : 0 };
    if ( wanted == opt_in_state ) return;

    wchar_t marker[ 1024 ]{ };
    if ( opt_in_path( marker ) )
    {
        if ( wanted )
        {
            void *file{ I( CreateFileW )( marker, 0x40000000u, 0, nullptr, 4, 0x80, nullptr ) };
            if ( file != reinterpret_cast< void * >( -1 ) ) I( CloseHandle )( file );
        }
        else
        {
            I( DeleteFileW )( marker );
            wchar_t path[ 1024 ]{ };
            if ( crash_path( path ) ) I( DeleteFileW )( path );
            if ( signature_path( path ) ) I( DeleteFileW )( path );
        }
    }
    opt_in_state = wanted;
}

void crash_reporter::signature_failure( u32 id, u32 module, u32 reason, const char *name )
{
    static u32 last_id{ };
    static u32 last_reason{ };
    if ( !diagnostics_enabled( ) || ( last_id == id && last_reason == reason ) ) return;
    last_id     = id;
    last_reason = reason;

    signature_record_t record{ SIGNATURE_MAGIC, SIGNATURE_VERSION, id, module, reason, { } };
    if ( name ) str::copy( record.m_name, sizeof( record.m_name ), name );

    wchar_t path[ 1024 ]{ };
    if ( !signature_path( path ) ) return;
    void *file{ I( CreateFileW )( path, 0x40000000u, 0, nullptr, 2, 0x80, nullptr ) };
    if ( file != reinterpret_cast< void * >( -1 ) )
    {
        DWORD written{ };
        I( WriteFile )( file, &record, sizeof( record ), &written, nullptr );
        I( CloseHandle )( file );
    }
}
