#ifndef TANKS_UI_HPP
#define TANKS_UI_HPP

#include "../core.h"

typedef struct
{
	vec4 fillColor;
	vec4 strokeColor;
	f32 cornerRadius;
	f32 strokeWidth;
} SDFShapeStyle;

enum class UIType
{
	UI_TYPE_BOX,
	UI_TYPE_TEXT,
	UI_TYPE_IMAGE
};

enum class LayoutType
{
	START,
	CENTER,
	SPACE_BETWEEN,
	END,
};

enum class SizingType
{
	HUG,
	FIXED,
};

enum class LayoutDirection
{
	LEFT_TO_RIGHT,
	TOP_TO_BOTTOM,
};

typedef struct 
{
	const char * label;
	vec2 pos;
	vec2 size;
	f32 childGap;
	LayoutDirection layoutDirection;
	u32 numChildren;
	LayoutType justify;
	SizingType sizing;
} UINode;

typedef struct UILayout_t
{
	UINode nodes[1024];
	u32	sizes[1024];
	u32	stack[128];
	u32 count;
	u32 depth;

	void startLayout(f32 frameWidth, f32 frameHeight);
	u32 begin(const char * label, f32 width, f32 height, f32 childGap, LayoutDirection layoutDirection, LayoutType justify, SizingType sizing);
	void end();
	void endLayout();
} UILayout;

#endif
