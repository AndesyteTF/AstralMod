#pragma once

struct _EXCEPTION_POINTERS;

namespace crash_reporter
{
    void capture( _EXCEPTION_POINTERS *info );
    void poll( );
    void signature_failure( u32 id, u32 module, u32 reason, const char *name );
}
