#ifndef RENDER_ENTRY_H
#define RENDER_ENTRY_H

typedef enum 
{
    RENDER_ENTRY_TYPE_CLEAR,
    RENDER_ENTRY_TYPE_DEBUG_RECTANGLE,
    RENDER_ENTRY_TYPE_DEBUG_CIRCLE,
    RENDER_ENTRY_TYPE_LINE,
    RENDER_ENTRY_TYPE_TEXTURED_QUAD,
    RENDER_ENTRY_TYPE_SUB_TEXTURE,
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
    SubTextureInstanceData instanceData;
} RenderEntrySubTexture;

#endif // RENDER_ENTRY_H