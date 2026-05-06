#include "render_commands.hpp"
#include <cstddef>
#include <cstring>

u32 PushRenderEntryText(RendererPushBuffer * pb, const RenderEntryText * entry, const char * text)
{
    u32 entryOffset = 0;
	size_t totalEntrySize = (sizeof(RenderEntryText) + entry->len);
    if (pb->index + totalEntrySize < pb->size)
    {
        entryOffset = pb->index;
        memcpy(pb->memory + pb->index, entry, sizeof(RenderEntryText));
        memcpy(pb->memory + pb->index + sizeof(RenderEntryText), text, entry->len);
        pb->index += totalEntrySize;
        pb->entryCount++;
    }
    return entryOffset;
}

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
	else
	{
		// TODO: warn if sort entry buffer full.
	}
}

#define PushRenderEntry(pushBuffer, entry) PushRenderEntryStruct(pushBuffer, &entry, sizeof(entry));

void RendererPushImage(RendererPushBuffer * pb, u32 textureID, InstanceData2D instanceData, u16 layer)
{
    RenderEntryTexturedQuad entry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, textureID, instanceData};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_TEXTURED_QUAD, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushSetProjection(RendererPushBuffer * pb, mat4 projection)
{
	RenderEntrySetProj entry = {RENDER_ENTRY_TYPE_SET_PROJ, projection};
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

void RendererPushLine(RendererPushBuffer * pb, vec2 startPos, vec2 endPos, vec4 color, float width, u16 layer)
{
    LineInstanceData instanceData = {{startPos.x, startPos.y, 0.0f}, {endPos.x, endPos.y, 0.0f}, color, width};
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
    RenderEntrySubTexture entry = {RENDER_ENTRY_TYPE_SUB_TEXTURE, textureID, position, rotation, scale, uvTransform};
    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_SUB_TEXTURE, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushText(RendererPushBuffer * pb, const char * text, f32 fontSize, i32 fontID, vec2 startPos, vec4 color, bool isWorldSpace, u16 layer)
{
	u32 textLen = strlen(text);
	RenderEntryText entry = {RENDER_ENTRY_TYPE_TEXT, fontID, fontSize, startPos, color, textLen, isWorldSpace};
	u32 entryOffset = PushRenderEntryText(pb, &entry, text);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_TEXT, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}

void RendererPushSDFRect(RendererPushBuffer * pb, vec2 pos, vec2 scale, const SDFShapeStyle * style, u16 layer)
{
	if (style == NULL) { return; }
	
	RenderEntrySDFRect entry = {RENDER_ENTRY_TYPE_SDF_RECT, pos, scale, style->fillColor, style->strokeColor, style->cornerRadius};

    u32 entryOffset = PushRenderEntry(pb, entry);

    RenderSortEntry sortEntry = {RENDER_ENTRY_TYPE_SDF_RECT, layer, entryOffset};
    PushRenderSortEntry(pb, sortEntry);
}
