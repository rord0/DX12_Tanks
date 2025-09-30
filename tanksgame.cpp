#include "core.h"
#include "render_entry.h"
#include "string.h"

#define EXPORT extern "C" __declspec(dllexport)

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


bool PushRenderEntryStruct(RendererPushBuffer * pb, void * entry, size_t entrySize)
{
    if (pb->index + entrySize < pb->size)
    {
        memcpy(pb->memory + pb->index, entry, entrySize);
        pb->index += entrySize;
        pb->entryCount++;
        return true;
    }
    else
    {
        return false;
    }
}

#define PushRenderEntry(pushBuffer, entry) PushRenderEntryStruct(pushBuffer, &entry, sizeof(entry));

inline bool RendererPushImage(RendererPushBuffer * pb, u32 textureID, InstanceData2D instanceData)
{
    RenderEntryTexturedQuad entry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, textureID, instanceData};
    return PushRenderEntry(pb, entry);
}

inline bool RendererPushRectangle(RendererPushBuffer * pb, DebugGeoInstanceData instanceData)
{
    RenderEntryDebugRectangle entry = {RENDER_ENTRY_TYPE_DEBUG_RECTANGLE, instanceData};
    return PushRenderEntry(pb, entry);
}

inline bool RendererPushLine(RendererPushBuffer * pb, vec3 startPos, vec3 endPos, vec3 color, float width)
{
    LineInstanceData instanceData = {startPos, endPos, color, width};
    RenderEntryLine entry = {RENDER_ENTRY_TYPE_LINE, instanceData};
    return PushRenderEntry(pb, entry);
}

EXPORT GAME_START_FUNCTION(start)
{
    GameState * state = (GameState*)gameMemory->permStorage;
    state->renderPB.size = gameMemory->transStorageSize;
    state->renderPB.memory = (u8*)gameMemory->transientStorage;
    u32 tankAtlasHandle = gameMemory->platformLoadTexture("tank_parts.png");
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

    DebugGeoInstanceData debugRectangle = {{0.5, 0.25, 0.0}, {0.8f, 1.0f}, {0.0f, 1.0f, 0.0f}, 0, 0.05f};
    DebugGeoInstanceData debugRectangle2 = {{state->tempPlayerPos.x, state->tempPlayerPos.y - 0.33f, 0.0}, {0.5f, 0.5f}, {0.0f, 1.0f, 1.0f}, (float)fmod(time, 360.0), 0.5f};
    DebugGeoInstanceData debugRectangle3 = {{state->tempPlayerPos.x + 0.25f, state->tempPlayerPos.y, 0.0}, {0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, (float)fmod(time, 360.0), 0.75f};
    RendererPushRectangle(&state->renderPB, debugRectangle);
    RendererPushRectangle(&state->renderPB, debugRectangle2);
    RendererPushRectangle(&state->renderPB, debugRectangle3);

    InstanceData2D gdEasy = {{state->tempPlayerPos.x, state->tempPlayerPos.y, 0.0f}, {1.0f, 1.0f}, 0.0f};
    InstanceData2D gdNormal = {{0.5f, 0.0f}, {1.0f + sinf(time) * 0.5f, 1.0f + sinf(time) * 0.5f}, 1.57079633f};
    InstanceData2D gdHard = {{-0.0f, -0.5f}, {1.0f, 1.0f}, (float)fmod(time, 360.0)};
    InstanceData2D gdHarder = {{-sinf(time) * 0.5f, cosf(time) * 0.5f, 0.0f}, {1.0f, 1.0f}, 0.0f};

    RendererPushImage(&state->renderPB, 0, gdEasy);
    RendererPushImage(&state->renderPB, 1, gdNormal);
    RendererPushImage(&state->renderPB, 2, gdHard);
    RendererPushImage(&state->renderPB, 3, gdHarder);

    RendererPushLine(&state->renderPB, debugRectangle3.position,debugRectangle2.position, {0.196f, 0.04f, 0.6f}, 0.01f);
    RendererPushLine(&state->renderPB, debugRectangle3.position,gdHard.position, {1.0f, 0.8f, 0.8f}, 0.01f);
}