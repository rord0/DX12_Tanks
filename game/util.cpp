#include "util.hpp"

size_t copy_c_str(char * dst, const char * src, size_t buffer_size)
{
    if (dst == NULL || src == NULL || buffer_size == 0)
        return 0;

    size_t i;
    for (i = 0; i < buffer_size - 1 && src[i] != '\0'; i++)
        dst[i] = src[i];

    dst[i] = '\0';
    return i;
}

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
u32 pcg32_random_r(pcg32_random_t * rng)
{
    u64 oldstate = rng->state;
    // Advance internal state
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc|1);
    // Calculate output function (XSH RR), uses old state for max ILP
    u32 xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
    u32 rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

vec2 RandomDirection(pcg32_random_t* rng)
{
    float angle = (float)pcg32_random_r(rng) / (float)0x100000000ULL * 6.28318530717958647692f;
    return { cosf(angle), sinf(angle) };
}
