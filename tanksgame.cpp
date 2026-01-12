#include "core.h"
#include "render_entry.h"
#include "string.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdio>

#include "render_commands.cpp"

#define EXPORT extern "C" __declspec(dllexport)
#define TANK_MAX_HEALTH 100

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
} GameState;

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
    DebugGeoInstanceData debugHitbox = {vec3{tank.position.x, tank.position.y, 0.0f}, {0.8f, 0.8f}, {0.0f, 1.0f, 0.0f}, tankRot, 0.025f};
    RendererPushCircle(cmdBuffer, vec3{turretPos.x, turretPos.y, 0}, 0, {0.05f,0.05f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);
    RendererPushCircle(cmdBuffer, vec3{turretCenter.x, turretCenter.y, 0}, 0, {0.05f,0.05f}, {1.0f, 1.0f, 0.0f}, 1.0f, 30);
    RendererPushRectangle(cmdBuffer, debugHitbox, 30);
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

void UpdateTank(TankGFX * tank, double deltaTime)
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
		tank->healthLerp -= deltaTime ;
		if (tank->healthLerp < 0.0f) { tank->healthLerp = 0.0f; }
	}
	else if (tank->healthLerp < healthNormalized)
	{
		tank->healthLerp = healthNormalized;
	}
}

void TankPlayFireEffects(TankGFX * tank, GameState * state)
{
	if (tank->turretOffset >= 1.0f)
	{ 
		tank->turretOffset = 0.0f;
		vec2 particleDir = vec2{cosf(tank->turretRot), sinf(tank->turretRot)};
		vec2 tankDir = vec2{cosf(tank->rotation),  sinf(tank->rotation)};
		vec2 turretCenter = (-tankDir * -0.03f) + tank->position;
		vec2 newPos = turretCenter - particleDir * 0.5f;
		EmitParticles(&state->turretFireEmitter, newPos, particleDir);
		EmitParticles(&state->explosionEmitter, vec2{0,0}, particleDir);

		if (tank->health >= 10)
			tank->health -= 10;
		else
			tank->health = TANK_MAX_HEALTH;
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

	state->tanks[0].health = 100;
	state->tanks[1].health = 100;

	state->tanks[0].active = true;
	state->tanks[1].active = true;
	state->tanks[0].style.colorID = 1;
	state->tanks[1].style.colorID = 2;

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
    double time = state->time;


    vec4 color = GetHSVSpectrumColor(time);

    float angle = (float)fmod(time * 100, 360.0) * (PI/180.0f);
    float angle2 = (float)fmod(time * 10, 360.0) * (PI/180.0f);
    InstanceData2D gdEasy   = {{sinf(angle), cosf(angle), 0.0f}, {0.8f, 1.0f}, 0.0f};
    InstanceData2D gdNormal = {{0.75f, 0.75f}, {0.64f, 1.0f}, 1.57079633f};
    InstanceData2D gdHarder = {{0.0f, sinf(angle), 0.0f}, {0.8f, 1.0f}, angle};
    InstanceData2D gdHard   = {{gdHarder.position.x + sinf(angle) * -0.075f, gdHarder.position.y - cosf(angle) * -0.075f}, {0.57f, 1.0f}, angle};

    RendererPushCircle(renderCommands, gdNormal.position, gdEasy.rotation, {0.1f,0.1f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);

    RendererPushImage(renderCommands, 2, gdEasy, 4);
    RendererPushImage(renderCommands, state->extraTextureHandle, gdEasy, 0);

    RendererPushLine(renderCommands, gdEasy.position, gdHarder.position, {0.0f, 1.0f, 1.0f}, 0.02f, 0);

    state->tanks[0].position.x += input->tempInput.x * input->deltaTime;
    state->tanks[0].position.y += input->tempInput.y * input->deltaTime;
	state->tanks[0].rotation = (angle2);
	state->tanks[0].turretRot= angle;
    state->tanks[1].position.x += input->tempInput2.x * input->deltaTime;
    state->tanks[1].position.y += input->tempInput2.y * input->deltaTime;

	SimulateParticles(&state->turretFireEmitter, input->deltaTime);
	SimulateParticles(&state->explosionEmitter,  input->deltaTime);
	DrawParticles(renderCommands, state);

	if (input->isMousePressed) { TankPlayFireEffects(&state->tanks[0], state); }

	for (int i = 0; i < 8; i++)
	{
		UpdateTank(&state->tanks[i], input->deltaTime);
		DrawTank(state->tanks[i], renderCommands, state);
	}
}
