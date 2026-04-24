#ifndef RENDER_COMMANDS_HPP
#define RENDER_COMMANDS_HPP

#include "../core.h"
#include "../render_entry.h"

void RendererPushImage(RendererPushBuffer * pb, u32 textureID, InstanceData2D instanceData, u16 layer);

void RendererPushLine(RendererPushBuffer * pb, vec2 startPos, vec2 endPos, vec4 color, float width, u16 layer);

#endif
