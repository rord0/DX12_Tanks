#include "core.h"

typedef enum 
{
    RENDER_ENTRY_CLEAR,
    RENDER_ENTRY_DEBUG_RECTANGLE,
    RENDER_ENTRY_DEBUG_CIRCLE,
    RENDER_ENTRY_TEXTURED_QUAD
} RenderEntryType;

typedef struct {
    RenderEntryType type;
} RenderEntryHeader;

typedef struct {
    RenderEntryHeader header;
    vec4 clearColor;
} RenderEntryClear;

typedef struct {
    RenderEntryHeader header;
    DebugGeoInstanceData instanceData;
} RenderEntryDebugRectangle;

typedef struct {
    RenderEntryHeader header;
    DebugGeoInstanceData instanceData;
} RenderEntryDebugCircle;

typedef struct {
    RenderEntryHeader header;
    u32 textureID;
    InstanceData2D instanceData;
} RenderEntryTexturedQuad;

typedef struct {
    u32 entryCount;
    u8 * memory;
    size_t size;
    size_t index;
} RendererPushBuffer;