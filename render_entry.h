#ifndef RENDER_ENTRY_H
#define RENDER_ENTRY_H

#include "core.h"
typedef enum 
{
    RENDER_ENTRY_TYPE_TEXTURED_QUAD = 0,
    RENDER_ENTRY_TYPE_DEBUG_RECTANGLE,
    RENDER_ENTRY_TYPE_LINE,
    RENDER_ENTRY_TYPE_DEBUG_CIRCLE,
    RENDER_ENTRY_TYPE_SUB_TEXTURE,
    RENDER_ENTRY_TYPE_TEXT,
    RENDER_ENTRY_TYPE_SDF_RECT,
    RENDER_ENTRY_TYPE_SDF_TRIANGLE,
    RENDER_ENTRY_TYPE_SCROLL_TEXTURE,
    RENDER_ENTRY_TYPE_WORLD_BORDER,
    RENDER_ENTRY_TYPE_CLEAR,
	RENDER_ENTRY_TYPE_SET_PROJ,
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
	mat4 VP; // View * Projection
	mat4 projection;
} RenderEntrySetProj;

typedef struct {
    RenderEntryHeader header;
    DebugGeoInstanceData instanceData;
} RenderEntryDebugRectangle;

typedef struct {
    RenderEntryHeader header;
    vec3 position;
    f32 rotation;
    vec2 scale;
    vec3 color;
    f32 fill;
} RenderEntryDebugCircle;

typedef struct {
    RenderEntryHeader header;
    LineInstanceData instanceData;
} RenderEntryLine;

typedef struct {
    RenderEntryHeader header;
	TextureInstanceData instanceData;
} RenderEntryTexturedQuad;

typedef struct {
    RenderEntryHeader header;
    u32 textureAtlasID;
    vec3 position;
    float rotation;
    vec2 scale;
    vec4 uvTransform;
} RenderEntrySubTexture;

typedef struct {
    RenderEntryHeader header;
    i32 fontID;
	f32 fontSize;
    vec2 position;
	TextStyle style;
	u32 len;
	b32 isWorldSpace;
} RenderEntryText;

typedef struct {
    RenderEntryHeader header;
    vec2 position;
    vec2 size;
	vec4 fillColor;
	vec4 strokeColor;
	f32 cornerRadius;
} RenderEntrySDFRect;

typedef struct {
    RenderEntryHeader header;
	SDFRectInstanceData instanceData;
} RenderEntrySDFTriangle;

typedef struct {
    RenderEntryHeader header;
	ScrollTextureInstanceData instanceData;
} RenderEntryScrollTexture;

typedef struct {
    RenderEntryHeader header;
	WorldBorderInstanceData instanceData;
} RenderEntryWorldBorder;

typedef struct {
    RenderEntryType type;
    u16 layer;
    u32 pushBufferOffset;
} RenderSortEntry;

typedef struct {
    u32 entryCount;
    u8 * memory;
    size_t size;
    size_t index;
    RenderSortEntry * sortEntries;
    size_t maxSortEntries;
    size_t sortEntryCount;
} RendererPushBuffer;

#endif // RENDER_ENTRY_H
