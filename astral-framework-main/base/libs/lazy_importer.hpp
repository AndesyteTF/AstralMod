#pragma once

#include <intrin.h>

#include "runtime/obfuscation.hpp"
#include "util/typedefs.hpp"
#include "util/windefs.hpp"

extern "C"
{
    i32 ASTRAL_STDCALL RtlCreateUserThread( void *process, void *security, u8 suspended, u32 stack_bits, usize *stack_reserved, usize *stack_commit, void *start, void *parameter, void **thread, void *client_id );
    i32 ASTRAL_STDCALL NtWaitForAlertByThreadId( void *address, i64 *timeout );
    i32 ASTRAL_STDCALL NtAlertThreadByThreadId( void *thread_id );
    i32 ASTRAL_STDCALL NtWaitForSingleObject( void *handle, u8 alertable, i64 *timeout );
    i32 ASTRAL_STDCALL NtCreateThreadEx( void **thread, u32 desired_access, void *object_attributes, void *process, void *start_routine, void *argument, u32 create_flags, usize zero_bits, usize stack_commit, usize stack_reserve, void *attribute_list );
    i32 ASTRAL_STDCALL NtSetInformationThread( void *thread, u32 information_class, void *information, u32 information_length );
    i32 ASTRAL_STDCALL NtQueryInformationThread( void *thread, u32 information_class, void *information, u32 information_length, u32 *return_length );
    i32 ASTRAL_STDCALL NtAllocateVirtualMemory( void *process, void **base_address, usize zero_bits, usize *region_size, u32 allocation_type, u32 protect );
    i32 ASTRAL_STDCALL NtFreeVirtualMemory( void *process, void **base_address, usize *region_size, u32 free_type );
    i32 ASTRAL_STDCALL NtProtectVirtualMemory( void *process, void **base_address, usize *region_size, u32 new_protect, u32 *old_protect );
    i32 ASTRAL_STDCALL NtQueryVirtualMemory( void *process, void *base_address, i32 information_class, void *information, usize information_size, usize *return_size );
    i32 ASTRAL_STDCALL NtFlushInstructionCache( void *process, void *base_address, usize size );
    i32 ASTRAL_STDCALL NtQuerySystemTime( i64 *system_time );
    i32 ASTRAL_CDECL   vsnprintf( char *s, usize n, const char *fmt, char *arg );

#if defined( ASTRAL_PROVIDE_OWN_WINDEFS )
    u8 ASTRAL_STDCALL    RtlAddFunctionTable( void *function_table, u32 entry_count, u64 base_address );
    u8 ASTRAL_STDCALL    RtlDeleteFunctionTable( void *function_table );
    void *ASTRAL_STDCALL FindWindowA( const char *classname, const char *windowname );
    void ASTRAL_STDCALL  GetLocalTime( void *system_time );
    i32 ASTRAL_STDCALL   AllocConsole( );
    i32 ASTRAL_STDCALL   FreeConsole( );
    void *ASTRAL_STDCALL GetStdHandle( i32 standard_handle );
    i32 ASTRAL_STDCALL   SetConsoleTitleA( const char *title );
    i32 ASTRAL_STDCALL   WriteConsoleA( void *output, const void *buffer, u32 characters, u32 *written, void *reserved );
    i32 ASTRAL_STDCALL   CloseHandle( void *handle );
    i32 ASTRAL_STDCALL   DisableThreadLibraryCalls( void *module );
    void ASTRAL_STDCALL  Sleep( u32 milliseconds );
    u64 ASTRAL_STDCALL   GetTickCount64( );
    void ASTRAL_STDCALL  GetSystemInfo( void *system_info );
    void ASTRAL_STDCALL  OutputDebugStringA( const char *output_string );
    i32 ASTRAL_STDCALL   QueryPerformanceCounter( i64 *count );
    i32 ASTRAL_STDCALL   QueryPerformanceFrequency( i64 *frequency );
    i16 ASTRAL_STDCALL   GetAsyncKeyState( i32 key );
    void *ASTRAL_STDCALL GetForegroundWindow( );
    i64 ASTRAL_STDCALL   SetWindowLongPtrA( void *window, i32 index, i64 value );
    i64 ASTRAL_STDCALL   CallWindowProcA( void *previous, void *window, u32 message, usize wparam, i64 lparam );
    i64 ASTRAL_STDCALL   DefWindowProcA( void *window, u32 message, usize wparam, i64 lparam );
    u16 ASTRAL_STDCALL   RegisterClassExA( const void *window_class );
    void *ASTRAL_STDCALL CreateWindowExA( u32 extended_style, const char *class_name, const char *window_name, u32 style, i32 x, i32 y, i32 width, i32 height, void *parent, void *menu, void *instance, void *parameter );
    i32 ASTRAL_STDCALL   DestroyWindow( void *window );
    i32 ASTRAL_STDCALL   UnregisterClassA( const char *class_name, void *instance );
    void ASTRAL_STDCALL  FreeLibraryAndExitThread( void *module, u32 exit_code );
    void ASTRAL_STDCALL  GetCurrentProcess( );
    void *ASTRAL_STDCALL GetDC( void *window );
    i32 ASTRAL_STDCALL   ReleaseDC( void *window, void *context );
    void *ASTRAL_STDCALL CreatePen( i32 style, i32 width, u32 color );
    void *ASTRAL_STDCALL CreateFontA( i32 height, i32 width, i32 escapement, i32 orientation, i32 weight, u32 italic, u32 underline, u32 strike_out, u32 charset, u32 output_precision, u32 clip_precision, u32 quality, u32 pitch_and_family, const char *face );
    void *ASTRAL_STDCALL CreateSolidBrush( u32 color );
    i32 ASTRAL_STDCALL   DeleteObject( void *value );
    i32 ASTRAL_STDCALL   LineTo( void *context, i32 x, i32 y );
    i32 ASTRAL_STDCALL   MoveToEx( void *context, i32 x, i32 y, void *previous );
    i32 ASTRAL_STDCALL   PatBlt( void *context, i32 x, i32 y, i32 width, i32 height, u32 raster );
    void *ASTRAL_STDCALL SelectObject( void *context, void *value );
    i32 ASTRAL_STDCALL   SetBkMode( void *context, i32 mode );
    u32 ASTRAL_STDCALL   SetTextColor( void *context, u32 color );
    i32 ASTRAL_STDCALL   TextOutA( void *context, i32 x, i32 y, const char *value, i32 size );
    i32 ASTRAL_STDCALL   GetTextExtentPoint32A( void *context, const char *value, i32 size, void *extent );
    u32 ASTRAL_STDCALL   GetGlyphOutlineA( void *context, u32 code, u32 format, void *metrics, u32 buffer_size, void *buffer, const void *matrix );
    void *ASTRAL_STDCALL AddFontMemResourceEx( void *data, u32 size, void *reserved, u32 *font_count );
    i32 ASTRAL_STDCALL   RemoveFontMemResourceEx( void *font );
    i32 ASTRAL_STDCALL   MessageBoxA( HWND window, const char *msg_title, const char *msg_body, u32 msg_type );
    void *ASTRAL_STDCALL AddVectoredExceptionHandler( unsigned long first_handler, void *exception_handler );
    void *ASTRAL_STDCALL LoadLibraryExA( const char *file_name, void *file, u32 flags );
    u32 ASTRAL_STDCALL   GetCurrentThreadId( );
    u32 ASTRAL_STDCALL   GetLastError( );
#endif
}

namespace li
{
    namespace detail
    {
        inline constexpr u32 fnv_basis{ 0x811C9DC5u };
        inline constexpr u32 fnv_prime{ 0x01000193u };
        inline constexpr u32 lazy_hash_seed{ 0x734A2C91u };

        constexpr u32 hash_step( u32 value, char current )
        {
            return ( value ^ static_cast< u8 >( current ) ) * fnv_prime;
        }

        constexpr u32 khash( const char *value )
        {
            u32 hash{ fnv_basis };

            while ( *value )
            {
                hash = hash_step( hash, *value );
                ++value;
            }

            return hash;
        }

        ASTRAL_FORCE_INLINE u32 runtime_hash_basis( )
        {
            return ASTRAL_XOR32( fnv_basis );
        }

        ASTRAL_FORCE_INLINE u32 runtime_hash_prime( )
        {
            return ASTRAL_XOR32( fnv_prime );
        }

        inline u32 hash( const char *value )
        {
            if ( !value )
            {
                return 0;
            }

            u32 result{ runtime_hash_basis( ) };
            u32 prime{ runtime_hash_prime( ) };

            while ( *value )
            {
                result = ( result ^ static_cast< u8 >( *value ) ) * prime;
                ++value;
            }

            return result;
        }

        ASTRAL_FORCE_INLINE usize mix_cache_value( usize value )
        {
            if constexpr ( sizeof( usize ) >= sizeof( u64 ) )
            {
                value ^= value >> 33u;
                value *= static_cast< usize >( 0xC2B2AE3D27D4EB4Full );
                value ^= value >> 29u;
                value *= static_cast< usize >( 0x165667B19E3779F9ull );
                value ^= value >> 32u;
                return value;
            } else
            {
                value ^= value >> 16u;
                value *= static_cast< usize >( 0x7FEB352Du );
                value ^= value >> 15u;
                value *= static_cast< usize >( 0x846CA68Bu );
                value ^= value >> 16u;
                return value;
            }
        }

        inline PEB *peb( )
        {
#if defined( __x86_64__ )
            return reinterpret_cast< PEB * >( __readgsqword( 0x60 ) );
#else
            return reinterpret_cast< PEB * >( __readfsdword( 0x30 ) );
#endif
        }

        ASTRAL_FORCE_INLINE usize cache_pointer_key( const void *storage, usize value )
        {
            usize stack{ reinterpret_cast< usize >( &stack ) };
            usize peb_value{ reinterpret_cast< usize >( peb( ) ) };
            usize cookie{ static_cast< usize >( *reinterpret_cast< volatile u32 * >( static_cast< usize >( 0x7FFE0330u ) ) ) };
            usize ticks{ static_cast< usize >( __rdtsc( ) ) };
            usize key{ mix_cache_value( stack ^ peb_value ^ cookie ^ ticks ^ reinterpret_cast< usize >( storage ) ) };
            key ^= mix_cache_value( value + static_cast< usize >( 0x9E3779B97F4A7C15ull ) );
            return key ? key : static_cast< usize >( 0xD6E8FEB86659FD93ull );
        }

        inline char lower( char value )
        {
            return ( value >= 'A' && value <= 'Z' ) ? static_cast< char >( value | 0x20 ) : value;
        }

        inline wchar_t lower( wchar_t value )
        {
            return ( value >= L'A' && value <= L'Z' ) ? static_cast< wchar_t >( value | 0x20 ) : value;
        }

        inline bool equal_module_name( const wchar_t *left, usize left_length, const char *right,
                                       usize right_length )
        {
            if ( !left || !right || !left_length || !right_length )
            {
                return false;
            }

            if ( left_length != right_length && left_length != right_length + 4 )
            {
                return false;
            }

            for ( usize index{ }; index < right_length; ++index )
            {
                if ( lower( left[ index ] ) != static_cast< wchar_t >( lower( right[ index ] ) ) )
                {
                    return false;
                }
            }

            if ( left_length == right_length )
            {
                return true;
            }

            return left[ right_length ] == L'.' && lower( left[ right_length + 1 ] ) == L'd' &&
                   lower( left[ right_length + 2 ] ) == L'l' && lower( left[ right_length + 3 ] ) == L'l';
        }

        inline void *find_module( const char *name, usize length )
        {
            PEB *process_environment_block{ peb( ) };

            if ( !process_environment_block || !process_environment_block->Ldr || !name || !length )
            {
                return nullptr;
            }

            LIST_ENTRY *head{ &process_environment_block->Ldr->InMemoryOrderModuleList };

            for ( LIST_ENTRY *entry{ head->Flink }; entry != head; entry = entry->Flink )
            {
                auto *module{ reinterpret_cast< LDR_DATA_TABLE_ENTRY * >(
                    reinterpret_cast< u8 * >( entry ) - sizeof( LIST_ENTRY ) ) };

                if ( !module->DllBase || !module->BaseDllName.Buffer )
                {
                    continue;
                }

                usize module_name_length{ module->BaseDllName.Length / sizeof( wchar_t ) };

                if ( equal_module_name( module->BaseDllName.Buffer, module_name_length, name, length ) )
                {
                    return module->DllBase;
                }
            }

            return nullptr;
        }

        inline void *find_export( void *module, u32 export_hash, u32 depth );

        inline void *resolve_forwarded_export( const char *forwarded, u32 depth )
        {
            if ( !forwarded || depth > 8 )
            {
                return nullptr;
            }

            const char *separator{ forwarded };

            while ( *separator && *separator != '.' )
            {
                ++separator;
            }

            if ( *separator != '.' || separator == forwarded )
            {
                return nullptr;
            }

            void *module{ find_module( forwarded, static_cast< usize >( separator - forwarded ) ) };

            if ( !module )
            {
                return nullptr;
            }

            const char *function{ separator + 1 };

            if ( !*function || *function == '#' )
            {
                return nullptr;
            }

            return find_export( module, hash( function ), depth + 1 );
        }

        inline IMAGE_DATA_DIRECTORY *image_data_directory( void *module )
        {
            auto *dos{ static_cast< IMAGE_DOS_HEADER * >( module ) };
            if ( dos->e_magic != IMAGE_DOS_SIGNATURE )
            {
                return nullptr;
            }

            auto *nt{ reinterpret_cast< IMAGE_NT_HEADERS * >( reinterpret_cast< uptr >( dos ) + dos->e_lfanew ) };
            if ( nt->Signature != IMAGE_NT_SIGNATURE )
            {
                return nullptr;
            }

            return nt->OptionalHeader.DataDirectory;
        }

        inline void *find_export( void *module, u32 export_hash, u32 depth )
        {
            if ( !module || !export_hash || depth > 8 )
            {
                return nullptr;
            }

            IMAGE_DATA_DIRECTORY *directories{ image_data_directory( module ) };

            if ( !directories || !directories[ 0 ].VirtualAddress || !directories[ 0 ].Size )
            {
                return nullptr;
            }

            const auto *exports{ reinterpret_cast< IMAGE_EXPORT_DIRECTORY * >( static_cast< u8 * >( module ) + directories[ 0 ].VirtualAddress ) };
            const auto *names{ reinterpret_cast< u32 * >( static_cast< u8 * >( module ) + exports->AddressOfNames ) };
            const auto *ordinals{ reinterpret_cast< u16 * >( static_cast< u8 * >( module ) + exports->AddressOfNameOrdinals ) };
            const auto *functions{ reinterpret_cast< u32 * >( static_cast< u8 * >( module ) + exports->AddressOfFunctions ) };

            for ( u32 index{ }; index < exports->NumberOfNames; ++index )
            {
                const char *export_name{ reinterpret_cast< char * >( static_cast< u8 * >( module ) + names[ index ] ) };

                if ( hash( export_name ) != export_hash )
                {
                    continue;
                }

                u16 ordinal{ ordinals[ index ] };

                if ( ordinal >= exports->NumberOfFunctions )
                {
                    return nullptr;
                }

                u32 function_rva{ functions[ ordinal ] };

                if ( function_rva >= directories[ 0 ].VirtualAddress &&
                     function_rva < directories[ 0 ].VirtualAddress + directories[ 0 ].Size )
                {
                    const char *forwarded = reinterpret_cast< const char * >( reinterpret_cast< u8 * >( module ) + function_rva );
                    return resolve_forwarded_export( forwarded, depth + 1 );
                }

                return reinterpret_cast< void * >( reinterpret_cast< u8 * >( module ) + function_rva );
            }

            return nullptr;
        }

        inline void *find_export( u32 export_hash )
        {
            PEB *process_environment_block{ peb( ) };

            if ( !process_environment_block || !process_environment_block->Ldr || !export_hash )
            {
                return nullptr;
            }

            LIST_ENTRY *head{ &process_environment_block->Ldr->InMemoryOrderModuleList };

            for ( LIST_ENTRY *entry{ head->Flink }; entry != head; entry = entry->Flink )
            {
                auto *module{ reinterpret_cast< LDR_DATA_TABLE_ENTRY * >(
                    reinterpret_cast< u8 * >( entry ) - sizeof( LIST_ENTRY ) ) };

                if ( !module->DllBase )
                {
                    continue;
                }

                void *address{ find_export( module->DllBase, export_hash, 0 ) };

                if ( address )
                {
                    return address;
                }
            }

            return nullptr;
        }
    } // namespace detail

    template< u32 encoded_hash, u32 hash_key, u32 hash_shift, u32 hash_mask, u32 hash_addend,
              typename function_type >
    class lazy_function
    {
    public:
        ASTRAL_FORCE_INLINE static function_type get( )
        {
            for ( ;; )
            {
                const u32 state{ __atomic_load_n( &m_state, __ATOMIC_ACQUIRE ) };
                if ( state == cache_ready )
                {
                    return reinterpret_cast< function_type >( cache_get( ) );
                }

                u32 expected{ cache_empty };
                if ( state == cache_empty &&
                     __atomic_compare_exchange_n( &m_state, &expected, cache_resolving, false,
                                                  __ATOMIC_ACQUIRE, __ATOMIC_RELAXED ) )
                {
                    void *cached{ detail::find_export( hash( ) ) };
                    if ( cached )
                    {
                        cache_set( cached );
                    }

                    __atomic_store_n( &m_state, cached ? cache_ready : cache_empty, __ATOMIC_RELEASE );
                    return reinterpret_cast< function_type >( cached );
                }

                _mm_pause( );
            }
        }

        ASTRAL_FORCE_INLINE operator function_type( ) const
        {
            return get( );
        }

        template< typename... arguments_t >
        ASTRAL_FORCE_INLINE auto operator( )( arguments_t... arguments ) const -> decltype( get( )( arguments... ) )
        {
            return get( )( arguments... );
        }

    private:
        ASTRAL_FORCE_INLINE static u32 hash( )
        {
            return astral_obfuscation::decode_u32( encoded_hash, hash_key, hash_shift, hash_mask, hash_addend );
        }

        ASTRAL_FORCE_INLINE static void *cache_get( )
        {
            return reinterpret_cast< void * >( m_value ^ m_key );
        }

        ASTRAL_FORCE_INLINE static void cache_set( void *value )
        {
            const usize raw{ reinterpret_cast< usize >( value ) };
            const usize key{ detail::cache_pointer_key( &m_value, raw ) };
            m_key   = key;
            m_value = raw ^ key;
        }

        enum : u32
        {
            cache_empty,
            cache_resolving,
            cache_ready,
        };

        static_assert( __atomic_always_lock_free( sizeof( u32 ), nullptr ) );

        inline static usize m_value{ };
        inline static usize m_key{ };
        inline static u32   m_state{ cache_empty };
    };

#if !defined( I )
#define I( name ) \
    [] {                                                                                                               \
        constexpr u32 counter{ __COUNTER__ };                                                                          \
        return ::li::lazy_function<                                                                                    \
            ::astral_obfuscation::encode_u32( ::li::detail::khash( #name ), static_cast< u32 >( __LINE__ ), counter,    \
                                             sizeof( #name ), ::li::detail::lazy_hash_seed ),                          \
            ::astral_obfuscation::constant_key32( static_cast< u32 >( __LINE__ ), counter, sizeof( #name ),             \
                                                 ::li::detail::lazy_hash_seed ),                                       \
            ::astral_obfuscation::shift32( static_cast< u32 >( __LINE__ ), counter, sizeof( #name ) ),                  \
            ::astral_obfuscation::xor_mask32( static_cast< u32 >( __LINE__ ), counter, sizeof( #name ),                 \
                                             ::li::detail::lazy_hash_seed ),                                           \
            ::astral_obfuscation::addend32( static_cast< u32 >( __LINE__ ), counter, sizeof( #name ),                   \
                                           ::li::detail::lazy_hash_seed ),                                             \
            decltype( &::name ) >{ }; }( )
#endif

} // namespace li
