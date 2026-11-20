#include <stdint.h>

// No-CRT implementation used when clang lowers __builtin_truncf to a library
// call in the SSE2 build. Preserve NaN/Inf and the sign of zero.
extern "C" float truncf(float x)
{
    union
    {
        float f;
        uint32_t i;
    } value = { x };

    const uint32_t exponent = (value.i >> 23) & 0xffu;
    if (exponent < 127u)
    {
        value.i &= 0x80000000u;
        return value.f;
    }
    if (exponent >= 150u)
    {
        return x;
    }

    const uint32_t fractionalBits = 150u - exponent;
    value.i &= ~((1u << fractionalBits) - 1u);
    return value.f;
}
