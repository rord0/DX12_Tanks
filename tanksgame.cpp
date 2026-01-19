#include "core.h"
#include "render_entry.h"
#include "string.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <limits>

#include "render_commands.cpp"

#define EXPORT extern "C" __declspec(dllexport)
#define MAX_PLAYERS 8
#define TANK_MAX_HEALTH 100
#define TANK_ROTATION_SPEED 1.8f
#define TANK_MOVEMENT_SPEED 0.5f
#define TANK_FIRE_RATE 0.8f

RendererPushBuffer * DEBUG_RENDER_CMDS;

typedef struct {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
} AtlasEntry;

typedef struct {
	u32 sheetWidth;
	u32 sheetHeight;
	u32 numRows;
	u32 numCols;
	u32 numFrames;
    u32 textureHandle;
} SpriteSheet;

typedef struct {
	b32 active;
	vec2 position;
	vec2 direction;
	vec2 scale;
	float timeAlive;
} Particle;

typedef struct {
	f32 startLifetime;
	u32 maxParticles;
	Particle * particles;
} ParticleEmitter;

typedef struct {
    u8 trackType;
    u8 bodyType;
    u8 turretType;
    u8 colorID;
} TankStyle;

typedef struct
{
	u16 playerID;
	u16 health;
	vec2 position;
	b32 active;
	f32 rotation;
	f32 turretRot;
	f32 turretOffset;
	f32 healthLerp;
	TankStyle style;
} TankGFX;

typedef struct
{
    void * memory;
    size_t size;
    size_t index;
} Arena; 

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

typedef struct {
	bool active;
	u16 playerID;
	u16 health;
	vec2 position;
	f32 turretRot;
	f32 rotation;
	f32 lastFireTime;
	Collider2D collider;
	u8 input;
} Tank;

typedef struct {
    double time;
    vec3 cameraPos;
	Arena permArena;
    u32 extraTextureHandle;
    u32 tankAtlasHandle;
	AtlasEntry * tankAtlasEntries;
	SpriteSheet fireEffectSheet;
	SpriteSheet explosionVFXSheet;
	ParticleEmitter turretFireEmitter;
	ParticleEmitter explosionEmitter;
	TankGFX tanks[8]; 
	Tank tanks2[8];
} GameState;

f32 vec2Dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }
f32 vec2Dist(vec2 a, vec2 b) { return sqrtf((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y)); }
vec2 vec2Norm(vec2 v) { f32 mag = sqrtf(v.x * v.x + v.y * v.y); return {v.x / mag, v.y / mag }; }
vec2 vec2Rotate(vec2 v, f32 angle)
{
	float cosA = cosf(angle);
    float sinA = sinf(angle);
	return {v.x * cosA - v.y * sinA, v.x * sinA + v.y * cosA};
}

void RectGetMinMaxPointsOnAxis(const Collider2D & collider, const vec2 axis, vec2 * min, vec2 * max)
{
	if (collider.type == COLLIDER_RECTANGLE)
	{
		vec2 xAxis = vec2{ cosf(collider.rotation), sinf(collider.rotation)};
		vec2 yAxis = vec2{-sinf(collider.rotation), cosf(collider.rotation)};

		f32 halfExtentX = collider.size.x / 4.0f;
		f32 halfExtentY = collider.size.y / 4.0f;

		vec2 vertices[4];
		vertices[0] = collider.position + (xAxis * halfExtentX) + (yAxis * halfExtentY);
		vertices[1] = collider.position + (xAxis * halfExtentX) - (yAxis * halfExtentY);
		vertices[2] = collider.position - (xAxis * halfExtentX) + (yAxis * halfExtentY);
		vertices[3] = collider.position - (xAxis * halfExtentX) - (yAxis * halfExtentY);

		f32 minProj = vec2Dot(vertices[0], axis);
		f32 maxProj = minProj;
		*min = vertices[0];
		*max = vertices[0];

		for (vec2 v : vertices)
		{
			f32 p = vec2Dot(v, axis);
			if (p < minProj)
			{
				minProj = p;
				*min = v;
			}
			if (p > maxProj)
			{
				maxProj = p;
				*max = v;
			}
		}
	}
}

b32 IsCollidingOnAxis(vec2 axis, const Collider2D & a, const Collider2D & b, CollisionData & collisionData)
{
	vec2 minVertA, minVertB, maxVertA, maxVertB;
	RectGetMinMaxPointsOnAxis(a, axis, &minVertA, &maxVertA);
	RectGetMinMaxPointsOnAxis(b, axis, &minVertB, &maxVertB);

	float minA = vec2Dot(minVertA, axis);
	float maxA = vec2Dot(maxVertA, axis);
	float minB = vec2Dot(minVertB, axis);
	float maxB = vec2Dot(maxVertB, axis);

	if (maxA < minB || maxB < minA) { return false; }

	float overlapA = maxA - minB;
	float overlapB = maxB - minA;

	if (overlapA < overlapB)
	{
		collisionData.normal = -axis;
        collisionData.penetration = abs(overlapA);
	}
	else
	{
		collisionData.normal = axis;
		collisionData.penetration = abs(overlapB);
	}
	collisionData.point = collisionData.normal;


	return true;

}

vec2 ClosestPointOnRect(Collider2D rect, vec2 point)
{
	vec2 yAxis = vec2{-sinf(rect.rotation), cosf(rect.rotation)};
	vec2 xAxis = vec2{ cosf(rect.rotation), sinf(rect.rotation)};

    float hx = rect.size.x * 0.25f;
    float hy = rect.size.y * 0.25f;

	vec2 dir = point - rect.position;
    float px = vec2Dot(dir, xAxis);
    float py = vec2Dot(dir, yAxis);

	px = std::clamp(px, -hx, hx);
    py = std::clamp(py, -hy, hy);

	vec2 p = rect.position + xAxis * px + yAxis * py;
    //RendererPushCircle(DEBUG_RENDER_CMDS, vec3{p.x, p.y, 0.0f}, 0.0f, {0.1f,0.1f}, {1.0f, 0.0f, 0.0f}, 1.0f, 30);
    return p;
}

b32 IsColliding(const Collider2D & a, const Collider2D & b, CollisionData * outData)
{
	if (a.type == COLLIDER_RECTANGLE && b.type == COLLIDER_RECTANGLE)
	{
		vec2 axes[4];
		axes[0] = vec2{-sinf(a.rotation), cosf(a.rotation)};
		axes[1] = vec2{ cosf(a.rotation), sinf(a.rotation)};
		axes[2] = vec2{-sinf(b.rotation), cosf(b.rotation)};
		axes[3] = vec2{ cosf(b.rotation), sinf(b.rotation)};

		CollisionData bestCD;
		CollisionData currentCD;

		bestCD.penetration = 10000.0f;
		for (const vec2 & axis : axes)
		{
			if (!IsCollidingOnAxis(axis, a, b, currentCD))
				return false;

			if (currentCD.penetration <= bestCD.penetration)
				bestCD = currentCD;
		}

		RendererPushCircle(DEBUG_RENDER_CMDS, vec3{bestCD.normal.x, bestCD.normal.y, 0.0f}, 0.0f, {0.1f,0.1f}, {1.0f, 0.0f, 0.0f}, 1.0f, 30);
		if (outData != NULL) *outData = bestCD;
		return true;
	}
	if (a.type == COLLIDER_RECTANGLE && b.type == COLLIDER_CIRCLE)
	{
		vec2 closestPoint = ClosestPointOnRect(a, b.position);
		f32 dist = vec2Dist(closestPoint, b.position);

		if (dist <= 0.5f / 4.0f)
		{
			return true;
		}

		return false;
	}

	return true;
}

bool IsLinesColliding(vec2 startA, vec2 endA, vec2 startB, vec2 endB, vec2 * outIntersection)
{
	float uA = ((endB.x-startB.x)*(startA.y-startB.y) - (endB.y-startB.y)*(startA.x-startB.x)) / ((endB.y-startB.y)*(endA.x-startA.x) - (endB.x-startB.x)*(endA.y-startA.y));
	float uB = ((endA.x-startA.x)*(startA.y-startB.y) - (endA.y-startA.y)*(startA.x-startB.x)) / ((endB.y-startB.y)*(endA.x-startA.x) - (endB.x-startB.x)*(endA.y-startA.y));

	if (uA >= 0 && uA <= 1 && uB >= 0 && uB <= 1)
	{
		float intersectionX = startA.x + (uA * (endA.x-startA.x));
		float intersectionY = startA.y + (uA * (endA.y-startA.y));
		if (outIntersection != NULL) *outIntersection = vec2{intersectionX, intersectionY};
		return true;
	}
	return false;
}

bool IsLineColliding(const Collider2D & collider, vec2 start, vec2 end, vec2 * outPoint)
{
	vec2 xAxis = vec2{ cosf(collider.rotation), sinf(collider.rotation)};
	vec2 yAxis = vec2{-sinf(collider.rotation), cosf(collider.rotation)};

	f32 halfExtentX = collider.size.x / 4.0f;
	f32 halfExtentY = collider.size.y / 4.0f;

	vec2 vertices[4];
	vertices[0] = collider.position + (xAxis * halfExtentX) + (yAxis * halfExtentY);
	vertices[1] = collider.position - (xAxis * halfExtentX) + (yAxis * halfExtentY);
	vertices[2] = collider.position - (xAxis * halfExtentX) - (yAxis * halfExtentY);
	vertices[3] = collider.position + (xAxis * halfExtentX) - (yAxis * halfExtentY);

	bool colliding = false;
	f32 closestDist = std::numeric_limits<float>::max();;
	vec2 closestPoint= {0,0};
	for (int i = 0; i < 4; i++)
	{
		vec2 intersection;
		if (IsLinesColliding(start, end, vertices[i], vertices[(i + 1) % 4], &intersection))
		{
			colliding = true;
			f32 dist= vec2Dist(start, intersection);
			if (dist <= closestDist)
			{
				closestPoint = intersection;
				closestDist = dist;
			}
			RendererPushCircle(DEBUG_RENDER_CMDS, {intersection.x, intersection.y, 0.0f}, 0.0f, {0.1f,0.1f}, {1.0f, 0.0f, 0.0f}, 1.0f, 30);
		}
	}

	if (colliding && outPoint != NULL) { *outPoint = closestPoint; }

	return colliding;
}

mat4 orthographicProjection(float right, float left, float top, float bottom, float n, float f)
{
    mat4 m = {};
    m.m[0][0] = 2.0f / (right - left);
    m.m[1][1] = 2.0f / (top - bottom);
    m.m[2][2] = 2.0f / (f - n);

    m.m[0][3] = -((right + left)/(right - left));
    m.m[1][3] = -((top + bottom)/(top - bottom));
    m.m[2][3] = -((f + n)/(f - n));

    m.m[3][3] = 1.0f;
    return m;
}                      

mat4 inverseOrthographicProjection(float right, float left, float top, float bottom, float n, float f)
{
    mat4 m = {};

    m.m[0][0] = (right - left) * 0.5f;
    m.m[1][1] = (top - bottom) * 0.5f;
    m.m[2][2] = (f - n) * 0.5f;

    m.m[0][3] = (right + left) * 0.5f;
    m.m[1][3] = (top + bottom) * 0.5f;
    m.m[2][3] = (f + n) * 0.5f;

    m.m[3][3] = 1.0f;

    return m;
}

vec4 CalculateUVTransform(AtlasEntry entry, u32 atlasHeight, u32 atlasWidth)
{
	vec4 uv = {(f32)entry.width / (f32)atlasWidth, (f32)entry.height / (f32)atlasHeight, (f32)entry.x / (f32)atlasWidth, (f32)entry.y / (f32)atlasHeight};
	return uv;
}

vec3 ColorHexToRBGNormalized(u32 color)
{
	float r = ((color >> 16) & 0xFF) / 255.0f;
    float g = ((color >> 8)  & 0xFF) / 255.0f;
    float b = ((color)       & 0xFF) / 255.0f;

    return vec3{r, g, b};
}

f32 easeOutExpo(f32 x) { return (x == 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * x); }
f32 easeInQuad(f32 x) { return (x * x); }

float Vec2AngleToRad(vec2 A, vec2 B)
{
	vec2 d = A - B;
	return atan2f(d.y, d.x);
}

float TurretRecoilOffset(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const float peakTime = 0.3f;
    const float maxOffset = 0.033f;

    float y;

    if (t < peakTime)
    {
        // Going up: 0 → 0.3
        float u = t / peakTime;          // normalize to 0 → 1
        y = easeOutExpo(u) * maxOffset;
    }
    else
    {
        // Going down: 0.3 → 1.0
        float d = (t - peakTime) / (1.0f - peakTime); // normalize to 0 → 1
        y = (1.0f - easeInQuad(d)) * maxOffset;
    }

    return y;
}

void DrawHealthbar(RendererPushBuffer * cmdBuffer, vec2 center, f32 remaining, f32 flashAmt)
{
	vec3 healthBarBGColor = {0.129411765, 0.141176471, 0.145098039};
	vec3 hbRemainingColor = ColorHexToRBGNormalized(0x98C24D);
	vec3 hbLostColor = ColorHexToRBGNormalized(0x8D2E24);
	f32 healthBarWidth = 0.8f;
	f32 healthBarHeight = 0.06f;
	f32 lost = 1.0f - remaining;
	f32 hbLeftside = center.x - (healthBarWidth/4.0f);
	f32 hbRightside = center.x + (healthBarWidth/4.0f);

    DebugGeoInstanceData hbBackground= {vec3{center.x, center.y, 0.0f}, {healthBarWidth + 0.026f, healthBarHeight + 0.026f}, healthBarBGColor, 0, 1.0f};
    DebugGeoInstanceData hbRemaining = {vec3{hbLeftside + ((healthBarWidth*remaining)/4.0f),center.y, 0.0f}, {healthBarWidth*remaining, healthBarHeight}, hbRemainingColor, 0, 1.0f};
    DebugGeoInstanceData hbFlash = {vec3{hbLeftside + ((healthBarWidth*flashAmt)/4.0f),center.y, 0.0f}, {healthBarWidth*flashAmt, healthBarHeight}, vec3{1.0f,1.0f,1.0f}, 0, 1.0f};
    DebugGeoInstanceData hbLost = {vec3{hbRightside - ((healthBarWidth*lost)/4.0f), center.y, 0.0f}, {healthBarWidth*lost, healthBarHeight}, hbLostColor, 0, 1.0f};

	RendererPushRectangle(cmdBuffer, hbBackground, 3);
	RendererPushRectangle(cmdBuffer, hbLost, 4);
	RendererPushRectangle(cmdBuffer, hbFlash, 4);
	RendererPushRectangle(cmdBuffer, hbRemaining, 5);
}

void DrawTank(TankGFX & tank, RendererPushBuffer * cmdBuffer, GameState * state)
{
	if (!tank.active) { return; }

    u32 trackIndex  =  0 + (3 * tank.style.trackType) + tank.style.colorID;
    u32 bodyIndex   = 12 + (3 * tank.style.bodyType ) + tank.style.colorID;
    u32 turretIndex = 24 + (3 * tank.style.trackType) + tank.style.colorID;

	//TODO(rordon): assert that indexes are less than 36.
	u32 atlasWidth  = 4096;
	u32 atlasHeight = 4096;

	vec4 trackUV  = CalculateUVTransform(state->tankAtlasEntries[trackIndex], atlasHeight, atlasWidth);
	vec4 bodyUV   = CalculateUVTransform(state->tankAtlasEntries[bodyIndex], atlasHeight, atlasWidth);
	vec4 turretUV = CalculateUVTransform(state->tankAtlasEntries[turretIndex], atlasHeight, atlasWidth);

	vec2 turretDir = vec2{cosf(tank.turretRot), sinf(tank.turretRot)};
	vec2 tankDir   = vec2{cosf(tank.rotation),  sinf(tank.rotation)};
	vec2 turretCenter = (-tankDir * -0.03f) + tank.position;
	vec2 turretPos = (-turretDir * 0.075f); // Offset backwards
	turretPos += (turretDir *  TurretRecoilOffset(tank.turretOffset)); // Add recoil offset.
	turretPos += turretCenter;

	f32 tankRot = tank.rotation + (PI/2.0f);

	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tankRot, {0.8f, 1.0f}, trackUV, 0);
	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tankRot, {0.64f, 1.0f}, bodyUV, 1);
	RendererPushSubTexture(cmdBuffer, state->tankAtlasHandle, {turretPos.x, turretPos.y, 0.0f}, tank.turretRot + (PI/2.0f), {0.57f, 1.0f}, turretUV, 2);

	DrawHealthbar(cmdBuffer, vec2{tank.position.x, tank.position.y + 0.3f}, ((float)tank.health / (float)TANK_MAX_HEALTH), tank.healthLerp);

	// DEBUG VISUALS
    RendererPushCircle(cmdBuffer, vec3{turretPos.x, turretPos.y, 0}, 0, {0.05f,0.05f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);
    RendererPushCircle(cmdBuffer, vec3{turretCenter.x, turretCenter.y, 0}, 0, {0.05f,0.05f}, {1.0f, 1.0f, 0.0f}, 1.0f, 30);
}

void DrawSpriteFrame(RendererPushBuffer * cmdBuffer, vec2 position, vec2 scale, f32 rotation, SpriteSheet * sheet, u32 spriteIndex)
{
	spriteIndex = spriteIndex % sheet->numFrames;

	const u32 animFrameWidth  = sheet->sheetWidth  / sheet->numCols;
	const u32 animFrameHeight = sheet->sheetHeight / sheet->numRows;

	u32 colIndex = spriteIndex % sheet->numCols;
	u32 rowIndex = spriteIndex / sheet->numCols;

	u32 sheetPosX = colIndex * animFrameWidth;
	u32 sheetPosY = rowIndex * animFrameHeight;

	vec4 uv = {(f32)animFrameWidth / (f32)sheet->sheetWidth, (f32)animFrameHeight / (f32)sheet->sheetHeight, (f32)sheetPosX / (f32)sheet->sheetWidth, (f32)sheetPosY / (f32)sheet->sheetHeight};

	RendererPushSubTexture(cmdBuffer, sheet->textureHandle, {position.x, position.y, 0.0f}, rotation, scale, uv, 3);
}

vec4 HSVtoRGBA(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1 - fabs(fmod(h / 60.0f, 2) - 1));
    float m = v - c;

    float r, g, b;
    if (h < 60)      { r = c; g = x; b = 0; }
    else if (h < 120){ r = x; g = c; b = 0; }
    else if (h < 180){ r = 0; g = c; b = x; }
    else if (h < 240){ r = 0; g = x; b = c; }
    else if (h < 300){ r = x; g = 0; b = c; }
    else             { r = c; g = 0; b = x; }

    return { r + m, g + m, b + m, 1.0f};
}

vec4 GetHSVSpectrumColor(float time, float speed = 1.0f)
{
    float hue = fmod(time * speed * 60.0f, 360.0f);
    return HSVtoRGBA(hue, 1.0f, 1.0f);
}

vec2 MousePosToWorld(vec2i mousePos, vec2i viewportSize, mat4 proj)
{
	float aspect = (float)viewportSize.x / (float)viewportSize.y;
	vec2 mouseNDC;
	mouseNDC.x = (2.0f * (mousePos.x / (float)viewportSize.x)) - 1.0f;
	mouseNDC.y = 1.0f - ((2.0f * mousePos.y) / (float)viewportSize.y);

	vec4 mouseClip = {mouseNDC.x, mouseNDC.y, 0.0f, 1.0f};
	vec4 worldPos = inverseOrthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f) * mouseClip;

	return {worldPos.x, worldPos.y};
}

Arena ArenaInit(void * memory, size_t size)
{
    Arena out = {memory, size, 0};
    return out;
}

void * ArenaPush(Arena * arena, size_t size)
{
    void * memory = nullptr;
    if (arena->index + size <= arena->size)
    {
        memory = (u8*)arena->memory + arena->index;
        arena->index += size;
    }
    return memory;
}

size_t ArenaGetRemainingSize(Arena * Arena)
{
    return Arena->size - Arena->index;
}

bool CSVParseU32Field(const char *& ptr, const char *& end, u32 & out)
{
    const char * start = ptr;
    while (ptr < end && *ptr != ',' && *ptr != '\n') { ptr++; }

    int value = 0;
    auto result = std::from_chars(start, ptr, value);
    if (result.ec != std::errc()) { return false; }

    // Skip comma
    if (*ptr == ',') { ptr++; }

    out = value;
    return true;
}

void ParseTextureAtlasCSV(GameMemory * memory, GameState * state, const char * filepath)
{
    // read csv file into buffer.
    DEBUG_FileResult fileResult = memory->platformLoadFile(filepath);
    if (fileResult.size == 0) { return; }

    // Count the number of lines 
    u32 numLines = 0;

    for (int i = 0; i < fileResult.size; i++)
    {
        if (((char*)fileResult.data)[i] == '\n') { numLines++; };
    }
    if (((char*)fileResult.data)[fileResult.size] != '\n') { numLines++; };

	state->tankAtlasEntries = (AtlasEntry*)ArenaPush(&state->permArena, sizeof(AtlasEntry) * (numLines - 1));

    // Parse records
    const char* ptr = (char*)fileResult.data;
    const char* end = (char*)fileResult.data + fileResult.size;

    // Skip first line
    while (ptr < end && *ptr != '\n') { ptr++; }
    ptr++;

	int entryCount = 0;
    // Add x, y, frame_width, frame_height fields to AtlasEntry struct
    while (ptr < end)
    {
        // Skip name field
        while (ptr < end && *ptr != ',') { ptr++; }
        ptr++;

        // Read fields
        AtlasEntry entry = {0};
        if (!CSVParseU32Field(ptr, end, entry.x)) { break; }
        if (!CSVParseU32Field(ptr, end, entry.y)) { break; }
        if (!CSVParseU32Field(ptr, end, entry.width)) { break; }
        if (!CSVParseU32Field(ptr, end, entry.height)) { break; }

        // Add entry to array
		state->tankAtlasEntries[entryCount++] = entry;
    }

    memory->platformFreeFile(&fileResult.data);
}

void EmitParticles(ParticleEmitter * emitter, vec2 position, vec2 direction)
{
	for (int i = 0; i < emitter->maxParticles; i++)
	{
		Particle * particle = &emitter->particles[i];
		if (!particle->active)
		{
			particle->active = true;
			particle->timeAlive = 0.0f;
			particle->position = position;
			particle->direction = direction;
			break;
		}
	}
}

void SimulateParticles(ParticleEmitter * emitter, double deltaTime)
{
	for (int i = 0; i < emitter->maxParticles; i++)
	{
		Particle * effect = &emitter->particles[i];
		if (effect->active)
		{
			effect->timeAlive += (float)deltaTime;

			if (effect->timeAlive > emitter->startLifetime)
			{
				effect->timeAlive = emitter->startLifetime;
				effect->active = false;
			}
		}
	}
}

int GetTankIndex(u16 playerID, GameState * state)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (state->tanks[i].playerID == playerID) { return i; }
	}
	return -1;
}

void TankDamage(Tank * tank, u16 amount)
{
	if (tank->health > amount)
	{
		tank->health -= amount;
	}
	else
	{
		tank->health = TANK_MAX_HEALTH;
	}
}

void TankPlayFireEffects(u16 playerID, vec2 hitPosition, GameState * state)
{
	int tankIndex = GetTankIndex(playerID, state);
	if (tankIndex == -1) { return; }

	TankGFX * tank = &state->tanks[tankIndex];
	tank->turretOffset = 0.0f;
	vec2 particleDir = vec2{cosf(tank->turretRot), sinf(tank->turretRot)};
	vec2 tankDir = vec2{cosf(tank->rotation),  sinf(tank->rotation)};
	vec2 turretCenter = (-tankDir * -0.03f) + tank->position;
	vec2 newPos = turretCenter - particleDir * 0.5f;
	EmitParticles(&state->turretFireEmitter, newPos, particleDir);
	EmitParticles(&state->explosionEmitter, hitPosition, particleDir);
}

void TankShoot(Tank * tank, GameState * state)
{
	vec2 turretDir = vec2{cosf(tank->turretRot), sinf(tank->turretRot)};
	vec2 tankDir   = vec2{cosf(tank->rotation),  sinf(tank->rotation)};
	vec2 turretCenter = tank->position + (tankDir * -0.03f);
	vec2 turretPos = turretCenter - (turretDir * 0.075f); // Offset backwards
											//
	vec2 lineStart = turretCenter;
	vec2 lineEnd =  turretCenter + (-turretDir*1.5f);

	Tank * hitTank = NULL;
	vec2 hitPos = lineEnd;
	f32 closestDist = std::numeric_limits<float>::max();
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		Tank * otherTank = &state->tanks2[i];
		if (otherTank->active && otherTank->playerID != tank->playerID)
		{
			vec2 collisionPoint;
			if (IsLineColliding(otherTank->collider, lineStart, lineEnd, &collisionPoint))
			{
				f32 dist = vec2Dist(collisionPoint, lineStart);
				if (dist < closestDist)
				{
					closestDist = dist;
					hitPos = collisionPoint;
					hitTank = otherTank;
				}
			}
		}
	}
	if (hitTank != NULL)
	{
		TankDamage(hitTank, 10);
	}

	TankPlayFireEffects(tank->playerID, hitPos, state);
	tank->lastFireTime = state->time;
}

void UpdateTank(Tank * tank, double deltaTime, GameState * state)
{
	if (!tank->active) return;

	vec2i inputAxis = {0, 0};
	bool shootPressed = ((tank->input >> 4) & 1);
	if ((tank->input >> 0) & 1) { inputAxis.y += 1; } // UP
	if ((tank->input >> 1) & 1) { inputAxis.x -= 1; } // LEFT
	if ((tank->input >> 2) & 1) { inputAxis.y -= 1; } // DOWN
	if ((tank->input >> 3) & 1) { inputAxis.x += 1; } // RIGHT
	
	tank->rotation -= inputAxis.x * TANK_ROTATION_SPEED * deltaTime;
	tank->turretRot = tank->rotation;
	vec2 tankForward = vec2Rotate({-1.0f, 0.0f}, tank->rotation);
	tank->position += tankForward * TANK_MOVEMENT_SPEED * inputAxis.y * deltaTime;

	tank->collider.position = tank->position;
	tank->collider.rotation = tank->rotation;

	if (shootPressed && state->time > tank->lastFireTime + TANK_FIRE_RATE)
	{
		TankShoot(tank, state);
	}
}

void DEBUG_SyncTanks(Tank * tanks, TankGFX * tankGFX)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		tankGFX[i].position  = tanks[i].position;
		tankGFX[i].rotation  = tanks[i].rotation;
		tankGFX[i].turretRot = tanks[i].turretRot;
		tankGFX[i].health	 = tanks[i].health;
		tankGFX[i].active	 = tanks[i].active;
	}
}

void UpdateTankGFX(TankGFX * tank, double deltaTime)
{
	if (!(tank->active)) { return; }

	if (tank->turretOffset < 1.0f)
	{
		tank->turretOffset += deltaTime;
		if (tank->turretOffset > 1.0f) tank->turretOffset = 1.0f;
	}

	f32 healthNormalized = ((f32)tank->health / (f32)TANK_MAX_HEALTH);
	if (tank->healthLerp > healthNormalized)
	{
		tank->healthLerp -= deltaTime * 0.5f;
		if (tank->healthLerp < 0.0f) { tank->healthLerp = 0.0f; }
	}
	else if (tank->healthLerp < healthNormalized)
	{
		tank->healthLerp = healthNormalized;
	}
}


void DrawTurretFireEffects(RendererPushBuffer * renderCmds, GameState * state)
{
	for (int i = 0; i < state->turretFireEmitter.maxParticles; i++)
	{
		const Particle * effect = &state->turretFireEmitter.particles[i];
		if (!effect->active) { continue; }

		const f32 animProgress = effect->timeAlive / state->turretFireEmitter.startLifetime;
		u32 currentFrame = (u32)std::max(0.0f, animProgress * state->fireEffectSheet.numFrames);
		f32 aspect = 480.0f / 240.0f;
		vec2 scale = {aspect, 1.0f};
		scale *= 0.6f;
		f32 angle = atan2f(effect->direction.y, effect->direction.x);
		if (angle < 0) angle += 2 * PI;
		DrawSpriteFrame(renderCmds, effect->position, scale, angle - PI, &state->fireEffectSheet, currentFrame);
	}
}

void DrawExplosionEffects(RendererPushBuffer * renderCmds, GameState * state)
{
	for (int i = 0; i < state->explosionEmitter.maxParticles; i++)
	{
		const Particle * effect = &state->explosionEmitter.particles[i];
		if (!effect->active) { continue; }

		const f32 animProgress = effect->timeAlive / state->explosionEmitter.startLifetime;
		u32 currentFrame = (u32)std::max(0.0f, animProgress * state->explosionVFXSheet.numFrames);
		vec2 scale = {1.0f, 1.0f};
		f32 angle = atan2f(effect->direction.y, effect->direction.x);
		if (angle < 0) angle += 2 * PI;
		DrawSpriteFrame(renderCmds, effect->position, scale, angle, &state->explosionVFXSheet, currentFrame);
	}
}

void DrawParticles(RendererPushBuffer * renderCmds, GameState * state)
{
	DrawTurretFireEffects(renderCmds, state);
	DrawExplosionEffects(renderCmds, state);
}

void DEBUG_DrawCollider(RendererPushBuffer * renderCmds, Collider2D * collider, b32 colliding)
{
	vec3 color = colliding ? vec3{1.0f, 0.0f, 0.0f} : vec3{0.0f, 1.0f, 0.0f};
	if (collider->type == COLLIDER_RECTANGLE)
	{
		DebugGeoInstanceData rect = {vec3{collider->position.x, collider->position.y, 0.0f},
			{collider->size.x, collider->size.y}, color, collider->rotation, 0.05f};
		RendererPushRectangle(renderCmds, rect, 33);
	}
}

EXPORT GAME_START_FUNCTION(start)
{
    GameState * state = (GameState*)gameMemory->permStorage;
	state->permArena = ArenaInit((u8*)gameMemory->permStorage + sizeof(GameState), gameMemory->permStorageSize - sizeof(GameState));

    state->tankAtlasHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"tank_parts.png");
    state->extraTextureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"images/platformer/Props_AirDrop.png");
    ParseTextureAtlasCSV(gameMemory, state, RESOURCES_PATH"tank_parts.csv");

	// SPRITESHEETS INITIALIZATION
	state->fireEffectSheet.textureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"tank_fire_spritesheet.png");
	state->fireEffectSheet.sheetHeight = 480;
	state->fireEffectSheet.sheetWidth  = 1080;
	state->fireEffectSheet.numFrames = 6;
	state->fireEffectSheet.numCols	 = 3;
	state->fireEffectSheet.numRows   = 2;

	state->explosionVFXSheet.textureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"explosion_spritesheet.png");
	state->explosionVFXSheet.sheetHeight = 1080;
	state->explosionVFXSheet.sheetWidth  = 2048;
	state->explosionVFXSheet.numFrames = 8;
	state->explosionVFXSheet.numCols   = 4;
	state->explosionVFXSheet.numRows   = 2;

	state->tanks[0]= {0};
	state->tanks[1]= {0};
	state->tanks[0].playerID = 66;
	state->tanks[1].playerID = 67;

	state->tanks2[0].active= true;
	state->tanks2[1].active = true;
	state->tanks2[0].playerID = 66;
	state->tanks2[1].playerID = 67;
	state->tanks[0].style.colorID = 1;
	state->tanks[1].style.colorID = 2;

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		state->tanks2[i].collider.type = COLLIDER_RECTANGLE;
		state->tanks2[i].collider.size = {0.75f, 0.65f};
		state->tanks2[i].health = TANK_MAX_HEALTH;
	}

	const f32 spritesheetAnimFPS = 20;
	const f32 fireAnimDuration = (1.0 / spritesheetAnimFPS) * state->fireEffectSheet.numFrames;
	state->turretFireEmitter.startLifetime = fireAnimDuration;
	state->turretFireEmitter.maxParticles = 16;
	state->turretFireEmitter.particles = (Particle*)ArenaPush(&state->permArena, sizeof(Particle) * state->turretFireEmitter.maxParticles);

	const f32 explosionAnimDuration = (1.0 / spritesheetAnimFPS) * state->explosionVFXSheet.numFrames;
	state->explosionEmitter.startLifetime = explosionAnimDuration;
	state->explosionEmitter.maxParticles = 16;
	state->explosionEmitter.particles = (Particle*)ArenaPush(&state->permArena, sizeof(Particle) * state->explosionEmitter.maxParticles);
}

EXPORT GAME_UPDATE_FUNCTION(update)
{
    // Update code here
    GameState * state = (GameState*)gameMemory->permStorage;
    state->time += input->deltaTime;

	float aspect = (float)input->viewportSize.x / (float)input->viewportSize.y;
	mat4 projection = orthographicProjection(aspect, -aspect, 1.0f, -1.0f, -0.01f, 100.0f);
    double time = state->time;
	DEBUG_RENDER_CMDS = renderCommands;

	vec2 mouseWorld = MousePosToWorld(input->mousePosVP, input->viewportSize, projection);
    vec4 color = GetHSVSpectrumColor(time);

    float angle = (float)fmod(time * 100, 360.0) * (PI/180.0f);
    float angle2 = (float)fmod(time * 10, 360.0) * (PI/180.0f);
    InstanceData2D gdEasy   = {{mouseWorld.x, mouseWorld.y, 0.0f}, {0.5f, 0.5f}, 0.0f};
    InstanceData2D gdNormal = {{0, 0, 0.0f}, {0.64f, 1.0f}, 1.57079633f};
    InstanceData2D gdHarder = {{0.0f, sinf(angle), 0.0f}, {0.8f, 1.0f}, angle};
    InstanceData2D gdHard   = {{gdHarder.position.x + sinf(angle) * -0.075f, gdHarder.position.y - cosf(angle) * -0.075f}, {0.57f, 1.0f}, angle};

	u8 inputA = 0;
	if (input->WASD[0].isDown) {inputA |= 1 << 0; };
	if (input->WASD[1].isDown) {inputA |= 1 << 1; };
	if (input->WASD[2].isDown) {inputA |= 1 << 2; };
	if (input->WASD[3].isDown) {inputA |= 1 << 3; };
	if (input->isSpacePressed) {inputA |= 1 << 4; };
	u8 inputB = 0;
	if (input->ARROWS[0].isDown) {inputB |= 1 << 0; };
	if (input->ARROWS[1].isDown) {inputB |= 1 << 1; };
	if (input->ARROWS[2].isDown) {inputB |= 1 << 2; };
	if (input->ARROWS[3].isDown) {inputB |= 1 << 3; };
	if (input->isEnterPressed) {inputB |= 1 << 4; };

	state->tanks2[0].input = inputA;
	state->tanks2[1].input = inputB;
	//state->tanks2[0].turretRot = Vec2AngleToRad(state->tanks2[0].position, mouseWorld); 
	
	Collider2D c = {.position = mouseWorld,
					.size = vec2{0.5f, 0.5f},
					.rotation = 0,
					.type = COLLIDER_CIRCLE};

	vec4 clearColor = {0.5f, 0.714f, 0.486f, 1.0f};

	vec2 intersection;

	vec2 lineAStart = {0,0};
	vec2 lineAEnd = mouseWorld;
	
	bool lineColliding  = IsLineColliding(state->tanks2[0].collider, lineAStart, lineAEnd, &intersection);
	vec3 lineColor = lineColliding ? vec3{1.0,0,0} : vec3{0,1,0};
	RendererPushSetClear(renderCommands, clearColor);
	RendererPushSetProjection(renderCommands, projection);

    RendererPushCircle(renderCommands, gdNormal.position, gdEasy.rotation, {0.1f,0.1f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);
    RendererPushCircle(renderCommands, vec3{intersection.x, intersection.y, 0}, gdEasy.rotation, {0.1f,0.1f}, {0.0f, 1.0f, 0.0f}, 0.25f, 30);

    RendererPushImage(renderCommands, 2, gdEasy, 4);
    RendererPushImage(renderCommands, state->extraTextureHandle, gdEasy, 0);

    RendererPushLine(renderCommands, lineAStart, lineAEnd, lineColor, 0.02f, 0);

	SimulateParticles(&state->turretFireEmitter, input->deltaTime);
	SimulateParticles(&state->explosionEmitter,  input->deltaTime);
	DrawParticles(renderCommands, state);

	CollisionData cd = {0};
	bool tanksColliding = IsColliding(state->tanks2[0].collider, state->tanks2[1].collider, &cd);
	//DEBUG_DrawCollider(renderCommands, &state->tanks2->collider, IsColliding(state->tanks2[0].collider, c, NULL));
	DEBUG_DrawCollider(renderCommands, &state->tanks2[1].collider, tanksColliding);
	if (tanksColliding)
	{
		vec2 correction = cd.normal * (cd.penetration * 0.5f);
		state->tanks2[0].position += correction;
		state->tanks2[1].position -= correction;
	}

	DEBUG_SyncTanks(state->tanks2, state->tanks);

	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		UpdateTank(&state->tanks2[i], input->deltaTime, state);
		UpdateTankGFX(&state->tanks[i], input->deltaTime);
		DrawTank(state->tanks[i], renderCommands, state);
	}
}
