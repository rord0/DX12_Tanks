#ifndef RENDER_ENTRY_H
#define RENDER_ENTRY_H

typedef enum 
{
    RENDER_ENTRY_TYPE_CLEAR,
	RENDER_ENTRY_TYPE_SET_PROJ,
    RENDER_ENTRY_TYPE_DEBUG_RECTANGLE,
    RENDER_ENTRY_TYPE_DEBUG_CIRCLE,
    RENDER_ENTRY_TYPE_LINE,
    RENDER_ENTRY_TYPE_TEXTURED_QUAD,
    RENDER_ENTRY_TYPE_SUB_TEXTURE,
    RENDER_ENTRY_TYPE_TEXT,
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
    u32 textureID;
    InstanceData2D instanceData;
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
	vec4 color;
	u32 len;
} RenderEntryText;

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
