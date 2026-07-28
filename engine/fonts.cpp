#include "fonts.hpp"
#include <cstddef>

Array FONTS;

bool InitializeFonts()
{
	FONTS = ArrayInit(sizeof(FontData), 16, PlatformAlloc(sizeof(FontData) * 16));
	return FONTS.elements != NULL;
}

const FontData * GetFontAssetData(i32 fontID)
{
	if (fontID < FONTS.count && fontID >= 0)
	{
		return &((const FontData*)FONTS.elements)[fontID];
	}
	else
	{
		return NULL;
	}
}

i32 LoadFontAtlas(const char *metadataPath, const char *atlasPath)
{
	if (ArrayFull(&FONTS)) { return -1; }
    i32 fontHandle = -1;

	i32 atlasTexture = PlatformLoadTexture(atlasPath);
	DEBUG_FileResult metadataJSON = DEBUG_PlatformReadEntireFile(metadataPath);
	if (!metadataJSON.data || atlasTexture == -1) { return -1; }

	((u8*)metadataJSON.data)[metadataJSON.size] = '\0';

	cJSON * root = cJSON_Parse((const char *)metadataJSON.data);

	cJSON * glyphs = cJSON_GetObjectItem(root, "glyphs");
	int glyphCount = cJSON_GetArraySize(glyphs);

	FontData * fontData = new FontData();
	fontData->textureHandle = atlasTexture;

	cJSON * atlasMeta = cJSON_GetObjectItem(root, "atlas");
	fontData->fontSizePx    = cJSON_GetObjectItem(atlasMeta, "size")->valuedouble;
	fontData->atlasWidth    = cJSON_GetObjectItem(atlasMeta, "width")->valueint;
	fontData->atlasHeight   = cJSON_GetObjectItem(atlasMeta, "height")->valueint;
	fontData->distanceRange = cJSON_GetObjectItem(atlasMeta, "distanceRange")->valuedouble;

	cJSON * metrics = cJSON_GetObjectItem(root, "metrics");
	fontData->lineHeight = cJSON_GetObjectItem(metrics, "lineHeight")->valuedouble;
	fontData->ascender   = cJSON_GetObjectItem(metrics, "ascender")->valuedouble;
	fontData->descender  = cJSON_GetObjectItem(metrics, "descender")->valuedouble;

	for (int i = 0; i < glyphCount; i++)
	{
		cJSON * glyph = cJSON_GetArrayItem(glyphs, i);
		GlyphData glyphData = {0};
		
		i32 unicode  = cJSON_GetObjectItem(glyph, "unicode")->valueint;
		glyphData.advance = cJSON_GetObjectItem(glyph, "advance")->valuedouble;

		cJSON* plane = cJSON_GetObjectItem(glyph, "planeBounds");
		if (plane)
		{
			glyphData.pl = cJSON_GetObjectItem(plane, "left")->valuedouble;
			glyphData.pb = cJSON_GetObjectItem(plane, "bottom")->valuedouble;
			glyphData.pr = cJSON_GetObjectItem(plane, "right")->valuedouble;
			glyphData.pt = cJSON_GetObjectItem(plane, "top")->valuedouble;
		}

		cJSON* atlas = cJSON_GetObjectItem(glyph, "atlasBounds");
		if (atlas)
		{
			glyphData.al = cJSON_GetObjectItem(atlas, "left")->valuedouble;
			glyphData.ab = cJSON_GetObjectItem(atlas, "bottom")->valuedouble;
			glyphData.ar = cJSON_GetObjectItem(atlas, "right")->valuedouble;
			glyphData.at = cJSON_GetObjectItem(atlas, "top")->valuedouble;
		}

		fontData->glyphs[unicode] = glyphData;
	}

	ArrayPush(&FONTS, (void*)fontData);
	fontHandle = FONTS.count - 1;

	cJSON_Delete(root);
	DEBUG_PlatformFreeFileMemory(&metadataJSON.data);

	return fontHandle;
}

PLATFORM_MEASURE_TEXT(PlatformMeasureText)
{
	const FontData * font = GetFontAssetData(fontID);
	vec2 size = {0};
	for (int i = 0; i < len; i++)
	{
		i32 codepoint = (i32)text[i];

		auto it = font->glyphs.find(codepoint);
		const GlyphData* glyph = (it == font->glyphs.end()) 
			? &font->glyphs.at(0)  // fallback
			: &it->second;

		size.x += glyph->advance * scale;
		size.y = std::max(size.y, (glyph->pt - glyph->pb) * scale);
	}
	return size;
}
