#include "core.h"
#include "render_commands.cpp"
#include "string.h"

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
} TankGFX;

void DrawTank(vec2 position, TankStyle style)
{
    u32 trackIndex  =  0 + (3 * style.trackType) + style.colorID;
    u32 bodyIndex   = 12 + (3 * style.bodyType ) + style.colorID;
    u32 turretIndex = 24 + (3 * style.trackType) + style.colorID;

    //TODO(rordon): assert that indexes are less than 36.
    //TODO(rordon): get uv coord of image from array...
    //TODO(rordon): issue draw calls for tank atlas.
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

typedef struct
{
    void * memory;
    size_t size;
    size_t index;
} Arena; 

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
    state->extraTextureHandle = gameMemory->platformLoadTexture(RESOURCES_PATH"world_eater.jpg");
    // TODO(rordon): tank_parts.csv into array of uv atlas data. 
    // TODO(rordon): get image handle for tank texture atlas.
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
    //color = {0.2f, 0.3f, 0.3f, 1.0f};
    color = {0.765f, 0.714f, 0.486f, 1.0f};
    state->clearColor = color;

    DebugGeoInstanceData debugRectangle = {{0.5, 0.25, 0.0}, {0.8f, 1.0f}, {0.0f, 1.0f, 0.0f}, 0, 0.1f};
    RendererPushRectangle(&state->renderPB, debugRectangle, 0);

    InstanceData2D gdEasy = {{state->tempPlayerPos.x, state->tempPlayerPos.y, 0.0f}, {1.0f, 1.0f}, 0.0f};
    InstanceData2D gdNormal = {{0.5f, 0.0f}, {1.0f + sinf(time) * 0.5f, 1.0f + sinf(time) * 0.5f}, 1.57079633f};
    InstanceData2D gdHard = {{-0.0f, -0.5f}, {1.0f, 1.0f}, (float)fmod(time, 360.0)};
    InstanceData2D gdHarder = {{-sinf(time) * 0.5f, cosf(time) * 0.5f, 0.0f}, {2.0f, 2.0f}, (float)fmod(time/2,360.0)};

    RendererPushImage(&state->renderPB, 1, gdEasy, 20);
    RendererPushImage(&state->renderPB, 2, gdNormal, 19);
    RendererPushImage(&state->renderPB, 3, gdHard, 1);
    RendererPushImage(&state->renderPB, 4, gdHarder, 2);

    RendererPushLine(&state->renderPB, gdEasy.position, gdHard.position, {0.0f, 0.0f, 1.0f}, 0.01f, 0);
}