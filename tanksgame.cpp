#include "core.h"
#include "render_entry.h"
#include "string.h"

#define EXPORT extern "C" __declspec(dllexport)

const char * tankPartNames [36] = {
    "Tank_Track_Type01_Dark_Blue", "Tank_Track_Type01_Green", "Tank_Track_Type01_Grey", "Tank_Track_Type01_Tan",
    "Tank_Track_Type02_Dark_Blue", "Tank_Track_Type02_Green", "Tank_Track_Type02_Grey", "Tank_Track_Type02_Tan",
    "Tank_Track_Type03_Dark_Blue", "Tank_Track_Type03_Green", "Tank_Track_Type03_Grey", "Tank_Track_Type03_Tan",
    "Tank_Body_Type01_Dark_Blue", "Tank_Body_Type01_Green", "Tank_Body_Type01_Grey", "Tank_Body_Type01_Tan",
    "Tank_Body_Type02_Dark_Blue", "Tank_Body_Type02_Green", "Tank_Body_Type02_Grey", "Tank_Body_Type02_Tan",
    "Tank_Body_Type03_Dark_Blue", "Tank_Body_Type03_Green", "Tank_Body_Type03_Grey", "Tank_Body_Type03_Tan",
    "Tank_Turret_Type01_Dark_Blue", "Tank_Turret_Type01_Green", "Tank_Turret_Type01_Grey", "Tank_Turret_Type01_Tan",
    "Tank_Turret_Type02_Dark_Blue", "Tank_Turret_Type02_Green", "Tank_Turret_Type02_Grey", "Tank_Turret_Type02_Tan",
    "Tank_Turret_Type03_Dark_Blue", "Tank_Turret_Type03_Green", "Tank_Turret_Type03_Grey", "Tank_Turret_Type03_Tan",
};

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
    u32 trackIndex  =  0 + style.trackType * style.colorID;
    u32 bodyIndex   = 12 + style.bodyType  * style.colorID;
    u32 turretIndex = 24 + style.trackType * style.colorID;

    //TODO(rordon): get uv coord of image from hashtable...
    //TODO(rordon): instance draw tank.
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

EXPORT GAME_START_FUNCTION(start)
{
    GameState * state = (GameState*)gameMemory->permStorage;
    state->renderPB.size = gameMemory->transStorageSize;
    state->renderPB.memory = (u8*)gameMemory->transientStorage;
    // TODO(rordon): load asset files
    // TODO(rordon): create tank part uv coordinate hashmap.
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
    color = {0.0f, 0.5f, 0.5f, 1.0f};
    state->clearColor = color;

    DebugGeoInstanceData debugRectangle = {{state->tempPlayerPos.x, state->tempPlayerPos.y, 0.0}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, (float)fmod(time, 360.0), 0.25f};
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
}