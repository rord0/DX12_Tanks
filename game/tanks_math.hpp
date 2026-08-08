#ifndef TANKS_MATH_H 
#define TANKS_MATH_H

#include "../core.h"
#include <algorithm>

vec2 mat4GetScale2D(const mat4 * m);
vec3 mat4GetPosition(mat4 * m);
f32 vec2Dist(vec2 a, vec2 b);
f32 vec2Dot(vec2 a, vec2 b);
vec2 vec2Rotate(vec2 v, f32 angle);

void GetAxes2D(const mat4 * m, vec2 * outX, vec2 * outY);

vec2 mat4GetPositionVec2(const mat4 * m);

mat4 ModelMatrix2D(vec2 pos, float rotation, vec2 scale);

mat2 GetRotationMat2(const mat4 & m);
//bool IsLineColliding(const Collider2D & collider, vec2 start, vec2 end, vec2 * outPoint);

#endif // TANKS_MATH_H
