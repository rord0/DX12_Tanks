#ifndef UTIL_HPP
#define UTIL_HPP

#include "../core.h"


size_t copy_c_str(char * dst, const char * src, size_t buffer_size);

typedef struct { u64 state;  u64 inc; } pcg32_random_t;

vec2 RandomDirection(pcg32_random_t * rng);
u32 RandomU32(pcg32_random_t * rng);

u32 fn1va_32(const char * s);

vec3 ColorHexToRBGNormalized(u32 color);
vec4 ColorHexToRBGANormalized(u32 color);

vec2 vec2SmoothDamp(vec2 currentPos, vec2 targetPos, vec2 * currentVelocity, float smoothTime, float maxSpeed, float deltaTime);

u32 CountNewLines(const char * data, size_t size);

bool CSVParseU32Field(const char *& ptr, const char *& end, u32 & out);
bool CSVParseF32Field(const char *& ptr, const char *& end, f32 & out);

vec4 lerp(const vec4 & a, const vec4 & b, float t);
vec2 lerp(const vec2 & a, const vec2 & b, float t);
f32 lerp(const f32 & a, const f32 & b, float t);

#endif // UTIL_HPP
