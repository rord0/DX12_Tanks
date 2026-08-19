#ifndef RENDERER_HPP
#define RENDERER_HPP

#include "../../includes.h"

int RendererCreateTexture(const ImageData * image);

void InitializeRenderer(HWND windowHandle, bool enableVSync, u32 width, u32 height);

#endif // RENDERER_HPP
