#include "core.h"
#include "string.h"
#include <charconv>

#include "render_commands.cpp"

#define EXPORT extern "C" __declspec(dllexport)

typedef struct {
    u32 x;
    u32 y;
    u32 width;
    u32 height;
} AtlasEntry;

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
	TankStyle style;
	f32 turretOffset;
} TankGFX;

typedef struct
{
    void * memory;
    size_t size;
    size_t index;
} Arena; 

AtlasEntry TANK_PART_ATLAS_ENTRIES[36];

vec4 CalculateUVTransform(AtlasEntry entry, u32 atlasHeight, u32 atlasWidth)
{
	vec4 uv = {(f32)entry.width / (f32)atlasWidth, (f32)entry.height / (f32)atlasHeight, (f32)entry.x / (f32)atlasWidth, (f32)entry.y / (f32)atlasHeight};
	return uv;
}

void DrawTank(TankGFX tank, GameState * state)
{
    u32 trackIndex  =  0 + (3 * tank.style.trackType) + tank.style.colorID;
    u32 bodyIndex   = 12 + (3 * tank.style.bodyType ) + tank.style.colorID;
    u32 turretIndex = 24 + (3 * tank.style.trackType) + tank.style.colorID;

    //TODO(rordon): assert that indexes are less than 36.
	u32 atlasWidth  = 4096;
	u32 atlasHeight = 4096;

	vec4 trackUV  = CalculateUVTransform(TANK_PART_ATLAS_ENTRIES[trackIndex], atlasHeight, atlasWidth);
	vec4 bodyUV   = CalculateUVTransform(TANK_PART_ATLAS_ENTRIES[bodyIndex], atlasHeight, atlasWidth);
	vec4 turretUV = CalculateUVTransform(TANK_PART_ATLAS_ENTRIES[turretIndex], atlasHeight, atlasWidth);

	RendererPushSubTexture(&state->renderPB, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tank.rotation, {0.8f, 1.0f}, trackUV, 0);
	RendererPushSubTexture(&state->renderPB, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tank.rotation, {0.64f, 1.0f}, bodyUV, 1);
	RendererPushSubTexture(&state->renderPB, state->tankAtlasHandle, {tank.position.x, tank.position.y, 0.0f}, tank.rotation, {0.57f, 1.0f}, turretUV, 2);
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

void ParseTextureAtlasCSV(GameMemory * memory, const char * filepath)
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
		TANK_PART_ATLAS_ENTRIES[entryCount++] = entry;
    }

    memory->platformFreeFile(&fileResult.data);
}

EXPORT GAME_START_FUNCTION(start)
{
    GameState * state = (GameState*)gameMemory->permStorage;

    Arena tempMemoryArena = ArenaInit(gameMemory->transientStorage, gameMemory->transStorageSize);
    state->renderPB.memory = (u8*)ArenaPush(&tempMemoryArena, MB(1));
    state->renderPB.size = MB(1);
    size_t maxSortEntryCount = ArenaGetRemainingSize(&tempMemoryArena) / sizeof(RenderSortEntry);
    state->renderPB.sortEntries = (RenderSortEntry*)ArenaPush(&tempMemoryArena, maxSortEntryCount * sizeof(RenderSortEntry));
    state->renderPB.maxSortEntries = maxSortEntryCount;

    state->tankAtlasHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"tank_parts.png");
    state->extraTextureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"images/platformer/Props_AirDrop.png");
    ParseTextureAtlasCSV(gameMemory, RESOURCES_PATH"tank_parts.csv");
}

EXPORT GAME_UPDATE_FUNCTION(update)
{
    // Update code here
    GameState * state = (GameState*)gameMemory->permStorage;
    state->time += deltaTime;
    double time = state->time;

    state->tempPlayerPos.y += state->tempInput.y * deltaTime;
    state->tempPlayerPos.x += state->tempInput.x * deltaTime;

    vec4 color = GetHSVSpectrumColor(time);

    color = {0.5f, 0.714f, 0.486f, 1.0f};
    state->clearColor = color;

    DebugGeoInstanceData debugRectangle = {{0.5, 0.25, 0.0}, {0.8f, 1.0f}, {0.0f, 1.0f, 0.0f}, 0, 1.0f};
    RendererPushRectangle(&state->renderPB, debugRectangle, 3);

    float angle = (float)fmod(time, 360.0);
    InstanceData2D gdEasy   = {{state->tempPlayerPos.x, state->tempPlayerPos.y, 0.0f}, {0.8f, 1.0f}, 0.0f};
    InstanceData2D gdNormal = {{state->tempPlayerPos.x, state->tempPlayerPos.y}, {0.64f, 1.0f}, 1.57079633f};
    InstanceData2D gdHarder = {{0.0f, sinf(angle), 0.0f}, {0.8f, 1.0f}, angle};
    InstanceData2D gdHard   = {{gdHarder.position.x + sinf(angle) * -0.075f, gdHarder.position.y - cosf(angle) * -0.075f}, {0.57f, 1.0f}, angle};

    //RendererPushCircle(&state->renderPB, gdHarder.position, gdEasy.rotation, {0.5f,0.5f}, {1.0f,1.0f,0.0f}, 1.0f, 30);
    RendererPushCircle(&state->renderPB, gdNormal.position, gdEasy.rotation, {0.1f,0.1f}, {0.0f, 1.0f, 0.0f}, 1.0f, 30);

    //RendererPushImage(&state->renderPB, 1, gdEasy, 2);
    RendererPushImage(&state->renderPB, 2, gdEasy, 4);
    RendererPushImage(&state->renderPB, state->extraTextureHandle, gdEasy, 0);

    RendererPushLine(&state->renderPB, gdEasy.position, gdHarder.position, {0.0f, 1.0f, 1.0f}, 0.02f, 0);

	TankGFX tankA = {0};
	tankA.playerID = 0;
	tankA.position = vec2{gdHard.position.x, gdHard.position.y};
	tankA.rotation = gdHard.rotation;
	tankA.style.colorID = 1;

	TankGFX tankB = {0};
	tankB.playerID = 0;
	tankB.position = vec2{gdHard.position.x + 0.5f, gdHard.position.y};
	tankB.rotation = gdHard.rotation;
	tankB.style.colorID = 2;

	DrawTank(tankA, state);
	DrawTank(tankB, state);
}
