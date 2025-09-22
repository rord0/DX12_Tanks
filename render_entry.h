#ifndef RENDER_ENTRY_H
#define RENDER_ENTRY_H

#include "core.h"

typedef enum 
{
    RENDER_ENTRY_TYPE_CLEAR,
    RENDER_ENTRY_TYPE_DEBUG_RECTANGLE,
    RENDER_ENTRY_TYPE_DEBUG_CIRCLE,
    RENDER_ENTRY_TYPE_TEXTURED_QUAD
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

#endif RENDER_ENTRY_H