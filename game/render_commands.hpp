#ifndef RENDER_COMMANDS_HPP
#define RENDER_COMMANDS_HPP

#include "../core.h"
#include "../render_entry.h"
#include "ui.hpp"

void RendererPushImage(RendererPushBuffer * pb, u32 textureID, InstanceData2D instanceData, u16 layer);

void RendererPushLine(RendererPushBuffer * pb, vec2 startPos, vec2 endPos, vec4 color, float width, u16 layer);

void RendererPushText(RendererPushBuffer * pb, const char * text, f32 fontSize, u32 fontID, vec2 startPos, TextStyle style, bool isWorldSpace, u16 layer);

void RendererPushSetProjection(RendererPushBuffer * pb, mat4 projection);

void RendererPushSDFRect(RendererPushBuffer * pb, vec2 pos, vec2 scale, const SDFShapeStyle * style, u16 layer);

void RendererPushSDFTriangle(RendererPushBuffer * pb, vec2 pos, f32 rotation, vec2 scale, const SDFShapeStyle * style, u16 layer);

#endif
