#include "core.h"

u32 PushRenderEntryStruct(RendererPushBuffer * pb, void * entry, size_t entrySize)
{
    u32 entryOffset = 0;
    if (pb->index + entrySize < pb->size)
    {
        entryOffset = pb->index;
        memcpy(pb->memory + pb->index, entry, entrySize);
        pb->index += entrySize;
        pb->entryCount++;
    }
    return entryOffset;
}

void PushRenderSortEntry(RendererPushBuffer * pb, RenderSortEntry sortEntry)
{
    if (pb->sortEntryCount < pb->maxSortEntries)
    {
        pb->sortEntries[pb->sortEntryCount++] = sortEntry;
    }
    // TODO: warn if sort entry buffer full.
}

#define PushRenderEntry(pushBuffer, entry) PushRenderEntryStruct(pushBuffer, &entry, sizeof(entry));

void RendererPushImage(RendererPushBuffer * pb, u32 textureID, InstanceData2D instanceData, u16 layer)
{
    RenderEntryTexturedQuad entry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, textureID, instanceData};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushRectangle(RendererPushBuffer * pb, DebugGeoInstanceData instanceData, u16 layer)
{
    RenderEntryDebugRectangle entry = {RENDER_ENTRY_TYPE_DEBUG_RECTANGLE, instanceData};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_DEBUG_RECTANGLE, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushLine(RendererPushBuffer * pb, vec3 startPos, vec3 endPos, vec3 color, float width, u16 layer)
{
    LineInstanceData instanceData = {startPos, endPos, color, width};
    RenderEntryLine entry = {RENDER_ENTRY_TYPE_LINE, instanceData};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_LINE, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushCircle(RendererPushBuffer * pb, vec3 position, vec2 size, vec3 color, float fill, u16 layer)
{
    // TODO: Circle rendering
}

void RendererPushSubTexture(RendererPushBuffer * pb, u32 textureID, vec2 position, vec2 size, vec4 uvCoords, u16 layer)
{
    // TODO: SubTexture rendering
}