#include "core.h"
#include "render_entry.h"

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

#define PushRenderEntry2(entryType, pushBuffer, entry)							\
	u32 entryOffset = PushRenderEntryStruct(pushBuffer, &entry, sizeof(entry)); \
	RenderSortEntry sortEntry = {entryType, layer, entryOffset};				\
	PushRenderSortEntry(pushBuffer, sortEntry)

void RendererPushImage(RendererPushBuffer * pb, u32 textureID, InstanceData2D instanceData, u16 layer)
{
    RenderEntryTexturedQuad entry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, textureID, instanceData};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushSetProjection(RendererPushBuffer * pb, mat4 projectionMatrix)
{
	RenderEntrySetProj entry = {RENDER_ENTRY_TYPE_SET_PROJ, projectionMatrix};
	PushRenderEntry(pb, entry);
}

void RendererPushSetClear(RendererPushBuffer * pb, vec4 clearColor)
{
	RenderEntryClear entry = {RENDER_ENTRY_TYPE_CLEAR, clearColor};
	PushRenderEntry(pb, entry);
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

void RendererPushCircle(RendererPushBuffer * pb, vec3 position, f32 rotation, vec2 scale, vec3 color, float fill, u16 layer)
{
    RenderEntryDebugCircle entry = {RENDER_ENTRY_TYPE_DEBUG_CIRCLE, position, rotation, scale, color, fill};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_DEBUG_CIRCLE, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushSubTexture(RendererPushBuffer * pb, u32 textureID, vec3 position, f32 rotation, vec2 scale, vec4 uvTransform, u16 layer)
{
    // TODO: SubTexture rendering
    RenderEntrySubTexture entry = {RENDER_ENTRY_TYPE_SUB_TEXTURE, textureID, position, rotation, scale, uvTransform};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_SUB_TEXTURE, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}
