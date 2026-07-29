#include "util.hpp"
#include <algorithm>

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

u32 fn1va_32(const char * s)
{
 	u32 hash = 2166136261u;
    while (*s) { hash = (hash ^ (uint8_t)*s++) * 16777619u; }
    return hash;
}

vec4 ColorHexToRBGANormalized(u32 color)
{
	float r = ((color >> 24) & 0xFF) / 255.0f;
    float g = ((color >> 16) & 0xFF) / 255.0f;
    float b = ((color >>  8) & 0xFF) / 255.0f;
    float a = ((color)       & 0xFF) / 255.0f;

    return vec4{r, g, b, a};
}

vec3 ColorHexToRBGNormalized(u32 color)
{
	float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8)  & 0xFF) / 255.0f;
    float b = ((color)       & 0xFF) / 255.0f;

    return vec3{r, g, b};
}

vec2 vec2SmoothDamp(vec2 currentPos, vec2 targetPos, vec2 * currentVelocity, float smoothTime, float maxSpeed, float deltaTime)
{
	// Converted from:
	// https://github.com/Unity-Technologies/UnityCsReference/blob/master/Runtime/Export/Math/Vector2.cs

	smoothTime = std::max(0.0001f, smoothTime);
	float omega = 2.0f / smoothTime;

	float x = omega * deltaTime;
	float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

	float change_x = currentPos.x - targetPos.x;
	float change_y = currentPos.y - targetPos.y;

	// Clamp maximum speed
	float maxChange = maxSpeed * smoothTime;

	float maxChangeSq = maxChange * maxChange;
	float sqDist = change_x * change_x + change_y * change_y;
	if (sqDist > maxChangeSq)
	{
		f32 mag = sqrtf(sqDist);
		change_x = change_x / mag * maxChange;
		change_y = change_y / mag * maxChange;
	}

	float target_x = currentPos.x - change_x;
	float target_y = currentPos.y - change_y;

	float temp_x = (currentVelocity->x + omega * change_x) * deltaTime;
	float temp_y = (currentVelocity->y + omega * change_y) * deltaTime;

	currentVelocity->x = (currentVelocity->x - omega * temp_x) * exp;
	currentVelocity->y = (currentVelocity->y - omega * temp_y) * exp;

	float output_x = target_x + (change_x + temp_x) * exp;
	float output_y = target_y + (change_y + temp_y) * exp;

	// Prevent overshooting
	float origMinusCurrent_x = targetPos.x - currentPos.x;
	float origMinusCurrent_y = targetPos.y - currentPos.y;
	float outMinusOrig_x = output_x - targetPos.x;
	float outMinusOrig_y = output_y - targetPos.y;

	if (origMinusCurrent_x * outMinusOrig_x + origMinusCurrent_y * outMinusOrig_y > 0)
	{
		output_x = targetPos.x;
		output_y = targetPos.y;

		currentVelocity->x = (output_x - targetPos.x) / deltaTime;
		currentVelocity->y = (output_y - targetPos.y) / deltaTime;
	}

	vec2 v;
	v.x = output_x;
	v.y = output_y;
	return v;
}

