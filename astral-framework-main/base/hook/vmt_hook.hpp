#pragma once

#include "allocator.hpp"

namespace astralhook
{
    class vmt_hook;

    class vmt_method_hook
    {
    public:
        vmt_method_hook( )                                    = default;
        vmt_method_hook( const vmt_method_hook & )            = delete;
        vmt_method_hook &operator=( const vmt_method_hook & ) = delete;
        vmt_method_hook( vmt_method_hook &&other );
        vmt_method_hook &operator=( vmt_method_hook &&other );
        ~vmt_method_hook( ) = default;

        static vmt_method_hook create( void *object, u32 index, void *destination );

        bool enable( );
        bool disable( );
        void reset( );

        template< typename type >
        ASTRAL_FORCE_INLINE type original( ) const
        {
            return reinterpret_cast< type >( this->m_original.get( ) );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret call( args... arguments ) const
        {
            using function_t = ret ( * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret stdcall( args... arguments ) const
        {
            using function_t = ret( ASTRAL_STDCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        ASTRAL_FORCE_INLINE error last_error( ) const
        {
            return this->m_error;
        }

        explicit ASTRAL_FORCE_INLINE operator bool( ) const
        {
            return this->m_entry && this->m_original.get( ) && this->m_destination && this->m_error == error::none;
        }

    private:
        void                            **m_entry{ };
        detail::encoded_pointer< void * > m_original{ };
        void                             *m_destination{ };
        bool                              m_enabled{ };
        error                             m_error{ error::none };
    };

    class vm_hook
    {
    public:
        vm_hook( )                            = default;
        vm_hook( const vm_hook & )            = delete;
        vm_hook &operator=( const vm_hook & ) = delete;
        vm_hook( vm_hook &&other );
        vm_hook &operator=( vm_hook &&other );
        ~vm_hook( ) = default;

        void reset( );

        template< typename type >
        ASTRAL_FORCE_INLINE type original( ) const
        {
            return reinterpret_cast< type >( this->m_original.get( ) );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret call( args... arguments ) const
        {
            using function_t = ret ( * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret ccall( args... arguments ) const
        {
            using function_t = ret( ASTRAL_CDECL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret thiscall( args... arguments ) const
        {
            using function_t = ret( ASTRAL_THISCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret stdcall( args... arguments ) const
        {
            using function_t = ret( ASTRAL_STDCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        template< typename ret = void, typename... args >
        ASTRAL_FORCE_INLINE ret fastcall( args... arguments ) const
        {
            using function_t = ret( ASTRAL_FASTCALL * )( args... );
            function_t function{ this->original< function_t >( ) };
            return detail::call_or_default< ret >( function, arguments... );
        }

        ASTRAL_FORCE_INLINE error last_error( ) const
        {
            return this->m_error;
        }

        explicit ASTRAL_FORCE_INLINE operator bool( ) const
        {
            return this->m_entry && this->m_original.get( ) && this->m_error == error::none;
        }

    private:
        friend class vmt_hook;

        vmt_hook                         *m_owner{ };
        void                            **m_entry{ };
        detail::encoded_pointer< void * > m_original{ };
        void                             *m_destination{ };
        error                             m_error{ error::none };
    };

    class vmt_hook
    {
    public:
        struct applied_object
        {
            void  *object{ };
            void **original_vmt{ };
        };

        vmt_hook( )                             = default;
        vmt_hook( const vmt_hook & )            = delete;
        vmt_hook &operator=( const vmt_hook & ) = delete;
        vmt_hook( vmt_hook &&other );
        vmt_hook &operator=( vmt_hook &&other );
        ~vmt_hook( ) = default;

        static vmt_hook create( void *object, u32 method_count );

        void    reset( );
        bool    apply( void *object );
        void    remove( void *object );
        vm_hook hook_method( u32 index, void *destination );

        template< typename type >
        ASTRAL_FORCE_INLINE type original( u32 index ) const
        {
            void **original_vmt{ this->m_original_vmt.get( ) };

            if ( index >= this->m_method_count || !original_vmt )
            {
                return type{ };
            }

            return reinterpret_cast< type >( original_vmt[ index ] );
        }

        ASTRAL_FORCE_INLINE void **shadow_vmt( ) const
        {
            return this->m_shadow_vmt;
        }

        ASTRAL_FORCE_INLINE void **new_vmt( ) const
        {
            return this->shadow_vmt( );
        }

        ASTRAL_FORCE_INLINE u32 method_count( ) const
        {
            return this->m_method_count;
        }

        ASTRAL_FORCE_INLINE error last_error( ) const
        {
            return this->m_error;
        }

        explicit ASTRAL_FORCE_INLINE operator bool( ) const
        {
            return this->m_shadow_vmt && this->m_original_vmt.get( ) && this->m_error == error::none;
        }

    private:
        friend class vm_hook;

        allocation                         m_allocation{ };
        detail::encoded_pointer< void ** > m_original_vmt{ };
        void                             **m_shadow_vmt{ };
        u32                                m_method_count{ };
        atl::vector< applied_object >      m_objects{ };
        error                              m_error{ error::none };
    };
} // namespace astralhook
