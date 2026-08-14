#include "tanks_math.hpp"

f32 rad2Deg(f32 r) { return r * (180.0f / PI); }
f32 deg2Rad(f32 d) { return d * (PI / 180.0f); }
f32 vec2Dot(vec2 a, vec2 b)  { return a.x * b.x + a.y * b.y; }
f32 vec2Dist(vec2 a, vec2 b) { return sqrtf((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y)); }
vec2 vec2Norm(vec2 v) { f32 mag = sqrtf(v.x * v.x + v.y * v.y); return {v.x / mag, v.y / mag }; }
vec2 vec2Direction(vec2 a, vec2 b) { return vec2Norm(a - b); }
vec2 vec2Rotate(vec2 v, f32 angle)
{
	float cosA = cosf(angle);
    float sinA = sinf(angle);
	return {v.x * cosA - v.y * sinA, v.x * sinA + v.y * cosA};
}

mat4 ModelMatrix2D(vec2 pos, float rotation, vec2 scale)
{
    float c = cosf(rotation);
    float s = sinf(rotation);

    mat4 m = {0};

    m.m[0][0] = c * scale.x;   m.m[0][1] = s * scale.x;   m.m[0][2] = 0.0f; m.m[0][3] = 0.0f;
    m.m[1][0] = -s * scale.y;  m.m[1][1] = c * scale.y;   m.m[1][2] = 0.0f; m.m[1][3] = 0.0f;
    m.m[2][0] = 0.0f;          m.m[2][1] = 0.0f;          m.m[2][2] = 1.0f; m.m[2][3] = 0.0f;
    m.m[3][0] = pos.x;         m.m[3][1] = pos.y;         m.m[3][2] = 0.0f; m.m[3][3] = 1.0f;

    return m;
}

mat2 GetRotationMat2(const mat4 & m)
{
    float sx = sqrtf(m.m[0][0] * m.m[0][0] + m.m[0][1] * m.m[0][1]);
    float sy = sqrtf(m.m[1][0] * m.m[1][0] + m.m[1][1] * m.m[1][1]);

    mat2 r;
    r.m[0][0] = m.m[0][0] / sx;  r.m[0][1] = m.m[0][1] / sx;
    r.m[1][0] = m.m[1][0] / sy;  r.m[1][1] = m.m[1][1] / sy;
    return r;
}

mat2 mat2::transpose()
{
	mat2 t = *this;
	t.m[0][1] = m[1][0];
	t.m[1][0] = m[0][1];
	return t;
}

void GetAxes2D(const mat4 * m, vec2 * outX, vec2 * outY)
{
    *outX = vec2Norm({m->m[0][0], m->m[0][1]});  // row 0
    *outY = vec2Norm({m->m[1][0], m->m[1][1]});  // row 1
}

vec3 mat4GetPosition(mat4 * m)
{
	return {m->m[3][0], m->m[3][1], m->m[3][2]};
}

vec2 mat4GetPositionVec2(const mat4 * m)
{
	return {m->m[3][0], m->m[3][1]};
}

vec2 mat4GetScale2D(const mat4 * m)
{
	return {sqrtf(m->m[0][0]*m->m[0][0] + m->m[0][1]*m->m[0][1]), sqrtf(m->m[1][0]*m->m[1][0] + m->m[1][1]*m->m[1][1])};
}
