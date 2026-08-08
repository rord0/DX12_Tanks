#ifndef RENDER_COMMANDS_HPP
#define RENDER_COMMANDS_HPP

#include "../core.h"
#include "../render_entry.h"
#include "ui.hpp"

void RendererPushImage(RendererPushBuffer * pb, u32 textureID, f32 alpha, mat4 model, u16 layer);

void RendererPushRectangle(RendererPushBuffer * pb, DebugGeoInstanceData instanceData, u16 layer);

void RendererPushCircle(RendererPushBuffer * pb, vec2 position, f32 rotation, vec2 scale, vec3 color, float fill, u16 layer);

void RendererPushLine(RendererPushBuffer * pb, vec2 startPos, vec2 endPos, vec4 color, float width, u16 layer);

void RendererPushText(RendererPushBuffer * pb, const char * text, f32 fontSize, u32 fontID, vec2 startPos, TextStyle style, bool isWorldSpace, u16 layer);

void RendererPushSetProjection(RendererPushBuffer * pb, mat4 VP, mat4 proj);

void RendererPushSubTexture(RendererPushBuffer * pb, u32 textureID, vec3 position, f32 rotation, vec2 scale, vec4 uvTransform, u16 layer);

void RendererPushSDFRect(RendererPushBuffer * pb, vec2 pos, vec2 scale, const SDFShapeStyle * style, u16 layer);

void RendererPushSDFTriangle(RendererPushBuffer * pb, vec2 pos, f32 rotation, vec2 scale, const SDFShapeStyle * style, u16 layer);

void RendererPushScrollingTexture(RendererPushBuffer * pb, u32 textureID, vec2 pos, vec2 size, vec2 offset, vec2 tilingAmount, u16 layer);

void RendererPushWorldBorder(RendererPushBuffer * pb, vec2 pos, vec2 size, f32 alpha, f32 offset, u16 layer);

void RendererPushSetClear(RendererPushBuffer * pb, vec4 clearColor);

#endif
