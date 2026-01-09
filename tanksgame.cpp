#include "core.h"
#include "string.h"
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdio>

#include "render_commands.cpp"

#define EXPORT extern "C" __declspec(dllexport)

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
    u8 trackType;
    u8 bodyType;
    u8 turretType;
    u8 colorID;
} TankStyle;

typedef struct
{
	u16 playerID;
	vec2 position;
	f32 rotation;
	f32 turretRot;
	TankStyle style;
	f32 turretOffset;
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
    vec2 tempPlayerPos;
    u32 extraTextureHandle;
	Arena permArena;
    u32 tankAtlasHandle;
	AtlasEntry * tankAtlasEntries;
	SpriteSheet fireEffectSheet;
	TankGFX testTank;
} GameState;

vec4 CalculateUVTransform(AtlasEntry entry, u32 atlasHeight, u32 atlasWidth)
{
	vec4 uv = {(f32)entry.width / (f32)atlasWidth, (f32)entry.height / (f32)atlasHeight, (f32)entry.x / (f32)atlasWidth, (f32)entry.y / (f32)atlasHeight};
	return uv;
}

f32 easeOutExpo(f32 x) { return (x == 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * x); }
f32 easeInQuad(f32 x) { return (x * x); }

float TurretRecoilOffset(float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

	float y;

    if (t < 0.5f)
    {
        // Going up: 0 → 0.5
        float u = t / 0.5f;              // normalize to 0 → 1
        y = easeOutExpo(u) * 0.033f;
    }
    else
    {
        // Going down: 0.5 → 1
        float d = (t - 0.5f) / 0.5f;     // normalize to 0 → 1
        y = (1.0f - easeInQuad(d)) * 0.033f;
    }

    return y;
}

void DrawTank(TankGFX tank, RendererPushBuffer * cmdBuffer, GameState * state)
{
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
    RendererPushCircle(cmdBuffer, vec3{turretPos.x, turretPos.y, 0}, 0, {0.05f,0.05f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);

    RendererPushCircle(cmdBuffer, vec3{turretCenter.x, turretCenter.y, 0}, 0, {0.05f,0.05f}, {1.0f, 1.0f, 0.0f}, 1.0f, 30);

    DebugGeoInstanceData debugHitbox = {vec3{tank.position.x, tank.position.y, 0.0f}, {0.8f, 0.8f}, {0.0f, 1.0f, 0.0f}, tankRot, 0.025f};
    RendererPushRectangle(cmdBuffer, debugHitbox, 30);
}

void DrawSpriteFrame(RendererPushBuffer * cmdBuffer, SpriteSheet * sheet, u32 spriteIndex)
{
	const f32 animFPS = 20;
	const f32 animTimePerFrame = 1.0 / animFPS;
	const f32 animDuration = animTimePerFrame * sheet->numFrames;

	// const f32 animElapsed = time - startTime;
	// const float32 animProgress = animElapsed / animDuration;

	spriteIndex = spriteIndex % sheet->numFrames;

	const u32 animFrameWidth  = sheet->sheetWidth  / sheet->numCols;
	const u32 animFrameHeight = sheet->sheetHeight / sheet->numRows;

	u32 colIndex = spriteIndex % sheet->numCols;
	u32 rowIndex = spriteIndex / sheet->numCols;

	u32 sheetPosX = colIndex * animFrameWidth;
	u32 sheetPosY = rowIndex * animFrameHeight;

	vec4 uv = {(f32)animFrameWidth / (f32)sheet->sheetWidth, (f32)animFrameHeight / (f32)sheet->sheetHeight, (f32)sheetPosX / (f32)sheet->sheetWidth, (f32)sheetPosY / (f32)sheet->sheetHeight};

	RendererPushSubTexture(cmdBuffer, sheet->textureHandle, {0.0f, 0.0f, 0.0f}, 0.0f, {1.0f, 1.0f}, uv, 3);
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

EXPORT GAME_START_FUNCTION(start)
{
    GameState * state = (GameState*)gameMemory->permStorage;
	state->permArena = ArenaInit((u8*)gameMemory->permStorage + sizeof(GameState), gameMemory->permStorageSize - sizeof(GameState));

    state->tankAtlasHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"tank_parts.png");
    state->extraTextureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"images/platformer/Props_AirDrop.png");
    ParseTextureAtlasCSV(gameMemory, state, RESOURCES_PATH"tank_parts.csv");
	state->fireEffectSheet.textureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"tank_fire_spritesheet.png");
	state->fireEffectSheet.sheetHeight = 480;
	state->fireEffectSheet.sheetWidth = 1080;
	state->fireEffectSheet.numCols = 3;
	state->fireEffectSheet.numRows = 2;
	state->fireEffectSheet.numFrames = 6;
	state->testTank = {0};
}

EXPORT GAME_UPDATE_FUNCTION(update)
{
    // Update code here
    GameState * state = (GameState*)gameMemory->permStorage;
    state->time += input->deltaTime;
    double time = state->time;

    state->tempPlayerPos.y += input->tempInput.y * input->deltaTime;
    state->tempPlayerPos.x += input->tempInput.x * input->deltaTime;

    vec4 color = GetHSVSpectrumColor(time);

    float angle = (float)fmod(time * 100, 360.0) * (PI/180.0f);
    float angle2 = (float)fmod(time * 20, 360.0) * (PI/180.0f);
    InstanceData2D gdEasy   = {{state->tempPlayerPos.x, state->tempPlayerPos.y, 0.0f}, {0.8f, 1.0f}, 0.0f};
    InstanceData2D gdNormal = {{state->tempPlayerPos.x, state->tempPlayerPos.y}, {0.64f, 1.0f}, 1.57079633f};
    InstanceData2D gdHarder = {{0.0f, sinf(angle), 0.0f}, {0.8f, 1.0f}, angle};
    InstanceData2D gdHard   = {{gdHarder.position.x + sinf(angle) * -0.075f, gdHarder.position.y - cosf(angle) * -0.075f}, {0.57f, 1.0f}, angle};

    RendererPushCircle(renderCommands, gdNormal.position, gdEasy.rotation, {0.1f,0.1f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);

    RendererPushImage(renderCommands, 2, gdEasy, 4);
    RendererPushImage(renderCommands, state->extraTextureHandle, gdEasy, 0);

    RendererPushLine(renderCommands, gdEasy.position, gdHarder.position, {0.0f, 1.0f, 1.0f}, 0.02f, 0);

	TankGFX tankA = {0};
	tankA.playerID = 0;
	tankA.position = vec2{gdHard.position.x, gdHard.position.y};
	tankA.rotation = gdHard.rotation;
	tankA.style.colorID = 1;

	state->testTank.playerID = 0;
	state->testTank.position = {cosf(angle2), sinf(angle2)};
	state->testTank.rotation = (angle2);
	state->testTank.turretRot= angle;
	state->testTank.style.colorID = 3;

	if (state->testTank.turretOffset >= 1.0f && input->isMousePressed) { 
		state->testTank.turretOffset = 0.0f; }

	if (state->testTank.turretOffset < 1.0f)
	{
		state->testTank.turretOffset += input->deltaTime * 2.0f;
		if (state->testTank.turretOffset > 1.0f) state->testTank.turretOffset = 1.0f;
	}

	// DrawTank(tankA, state);
	DrawTank(state->testTank, renderCommands, state);
	DrawSpriteFrame(renderCommands, &state->fireEffectSheet, 6);
}
