#ifndef INCLUDES_H
#define INCLUDES_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define UNICODE

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h> 
#include <Xinput.h>
#include <stdint.h>
#include <mmeapi.h>
#include <dsound.h>
#include <algorithm>
#include <unordered_map>

#include "core.h"

typedef struct
{
    HMODULE DLL;
    GameUpdateFunction * Update;
    GameStartFunction * Start;
    bool isValid;
    FILETIME lastWriteTime;
} Win32GameCode;

typedef struct {
    vec3 position;
    vec2 UV;
} VertexPosUV;

typedef struct {
    int width;
    int height;
    int numComponents;
    int size;
    u8 * memory;
} ImageData;

typedef enum {FONT_TYPE_BITMAP,
              FONT_TYPE_SDF,
              FONT_TYPE_MSDF
} FontType;

typedef struct 
{
	f32 advance;
    f32 pl, pb, pr, pt;  // plane bounds
    f32 al, ab, ar, at;  // atlas bounds
} GlyphData;

typedef struct {
	FontType type;

	// ATLAS
	u32 textureHandle;
	f32 fontSizePx;
    i32 atlasWidth;
    i32 atlasHeight;
    f32 distanceRange;

	// METRICS
	f32 lineHeight;
	f32 ascender;
	f32 descender;

	std::unordered_map<i32, GlyphData> glyphs;
} FontData;


#endif // INCLUDES_H
