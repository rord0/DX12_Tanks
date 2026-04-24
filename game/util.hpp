#ifndef UTIL_HPP
#define UTIL_HPP

#include "../core.h"

size_t copy_c_str(char * dst, const char * src, size_t buffer_size);

typedef struct { u64 state;  u64 inc; } pcg32_random_t;

vec2 RandomDirection(pcg32_random_t * rng);

#endif // UTIL_HPP
