#ifndef FONTS_HPP
#define FONTS_HPP

#include "../includes.h"
#include "platform.hpp"
#include "cJSON.h"
#include "../array.h"

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

bool InitializeFonts();

i32 LoadFontAtlas(const char * metadataPath, const char * atlasPath);

PLATFORM_MEASURE_TEXT(PlatformMeasureText);

const FontData * GetFontAssetData(i32 fontID);

#endif // FONTS_HPP
