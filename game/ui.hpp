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

typedef struct
{
	SDFShapeStyle normal;
	SDFShapeStyle hovered;
	SDFShapeStyle pressed;
} ButtonStyle;

typedef struct
{
	i32 fontHandle;
	f32 fontSize;
	f32 strokeWidth;
} FontStyle;

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
	f32 left;
	f32 right;
	f32 top;
	f32 bottom;
} UIPadding;

enum class UINodeType {
	UI_NODE_TYPE_CONTAINER,
	UI_NODE_TYPE_TEXT,
	UI_NODE_TYPE_IMAGE,
	UI_NODE_TYPE_BUTTON
};

typedef struct 
{
	i32 fontHandle;
	f32 fontSize;
	f32 strokeWidth;
	const char * text;
} UIText;

typedef struct
{
	bool visible;
	SDFShapeStyle style;
} UIContainer;

typedef struct
{
	ButtonStyle * style;
} UIButton;

typedef struct
{
	i32 handle;
} UIImage;

typedef struct 
{
	UINodeType type;
	union
	{
		UIText text;
		UIContainer container;
		UIImage image;
		UIButton button;
	};
} UINodeData;

typedef struct 
{
	vec2 size;
	f32 childGap;
	LayoutDirection axis;
	LayoutType justify;
	LayoutType align;
	SizingType sizing;
	UIPadding padding;
	UINodeData data;
} UINodeLayout;

typedef struct 
{
	const char * label;
	u32 id;
	vec2 pos;
	vec2 size;
	f32 childGap;
	LayoutDirection layoutDirection;
	u32 numChildren;
	LayoutType justify;
	SizingType sizing;
	LayoutType align;
	UIPadding padding;
	UINodeData data;
} UINode;

typedef struct 
{
	vec2 mousePos;
	ButtonInput mouseL;
} UIInput;

typedef struct UILayout_t
{
	UINode nodes[1024];
	u32	sizes[1024];
	u32	stack[128];
	u32 count;
	u32 depth;
	u32 hot;
	u32 active;
	PlatformMeasureTextFn * measureText;
	UIInput input;

	void startLayout(f32 frameWidth, f32 frameHeight, PlatformMeasureTextFn * measureTextFn, UIInput input);
	u32 begin(const char * label, f32 width, f32 height, f32 childGap, LayoutDirection layoutDirection, LayoutType justify, SizingType sizing, LayoutType align);
	u32 begin(const char * label, UINodeLayout layout);
	void text(const char * label, i32 fontHandle, f32 fontSize, f32 strokeWidth, const char * text);
	void image(const char * label, f32 width, f32 height, i32 imageHandle);
	void button(const char * label, vec2 size, ButtonStyle * buttonStyle, const char * text, FontStyle * textStyle);
	void button(const char * label, vec2 size, ButtonStyle * buttonStyle);
	void begin_button(const char * label, vec2 size, ButtonStyle * buttonStyle);
	bool isButtonPressed(const char * label);
	void end();
	void endLayout();
} UILayout;

#endif
