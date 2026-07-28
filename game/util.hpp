#ifndef UTIL_HPP
#define UTIL_HPP

#include "../core.h"

size_t copy_c_str(char * dst, const char * src, size_t buffer_size);

typedef struct { u64 state;  u64 inc; } pcg32_random_t;

vec2 RandomDirection(pcg32_random_t * rng);

u32 fn1va_32(const char * s);

vec3 ColorHexToRBGNormalized(u32 color);
vec4 ColorHexToRBGANormalized(u32 color);

#endif // UTIL_HPP
