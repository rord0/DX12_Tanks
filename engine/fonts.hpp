#ifndef FONTS_HPP
#define FONT_HPP

#include "../includes.h"
#include "platform.hpp"
#include "cJSON.h"

bool InitializeFonts();

i32 LoadFontAtlas(const char * metadataPath, const char * atlasPath);

PLATFORM_MEASURE_TEXT(PlatformMeasureText);

const FontData * GetFontAssetData(i32 fontID);

#endif // FONTS_HPP
