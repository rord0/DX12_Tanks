#ifndef TANKS_MATH_H 
#define TANKS_MATH_H

#include "../core.h"
#include <algorithm>

typedef enum {
	COLLIDER_RECTANGLE,
	COLLIDER_CIRCLE
} ColliderType;

typedef struct {
	vec2 position;
	vec2 size;
	f32 rotation;
	ColliderType type;
} Collider2D;

typedef struct {
	f32 penetration;
	vec2 normal;
	vec2 point;
} CollisionData;

f32 vec2Dist(vec2 a, vec2 b);
vec2 vec2Rotate(vec2 v, f32 angle);

mat4 ModelMatrix2D(vec2 pos, float rotation, vec2 scale);
// Physics
bool IsLineColliding(const Collider2D & collider, vec2 start, vec2 end, vec2 * outPoint);

#endif // TANKS_MATH_H
