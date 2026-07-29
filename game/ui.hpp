#ifndef TANKS_UI_HPP
#define TANKS_UI_HPP

#include "../core.h"
#include "ring_queue.hpp"

typedef struct 
{
	vec4 fillColor;
	vec4 strokeColor;
} ShapeColor;

typedef struct
{
	vec4 fillColor;
	vec4 strokeColor;
	f32 cornerRadius;
	f32 strokeWidth;
} SDFShapeStyle;

typedef struct
{
	ShapeColor normal;
	ShapeColor hovered;
	ShapeColor pressed;
	f32 cornerRadius;
	f32 strokeWidth;
	bool isTriangle;
	f32 rotation;
} ButtonStyle;

typedef struct
{
	i32 fontHandle;
	f32 fontSize;
	f32 strokeWidth;
	vec3 fillColor;
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
	FILL,
};

enum class PositionType
{
	RELATIVE,
	ABSOLUTE,
	FIXED
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
	UI_NODE_TYPE_BUTTON,
	UI_NODE_TYPE_INPUT_FIELD,
};

typedef struct 
{
	i32 fontHandle;
	f32 fontSize;
	f32 strokeWidth;
	vec3 fillColor;
	const char * text;
} UIText;

typedef struct
{
	bool visible;
	SDFShapeStyle style;
} UIContainer;

typedef struct
{
	const ButtonStyle * style;
} UIButton;

typedef struct
{
	i32 handle;
	vec4 uv;
} UIImage;

typedef enum : u32
{
	UI_INPUT_ALLOW_NONE 	   = 0,
	UI_INPUT_ALLOW_LOWERCASE   = 1 << 0,
	UI_INPUT_ALLOW_UPPERCASE   = 1 << 2,
	UI_INPUT_ALLOW_DIGITS	   = 1 << 3,
	UI_INPUT_ALLOW_UNDERSCORES = 1 << 4,
	UI_INPUT_ALLOW_PERIODS     = 1 << 5,
	UI_INPUT_ALLOW_COLONS      = 1 << 6,
	UI_INPUT_ALLOW_SYMBOLS     = 1 << 7,

	UI_INPUT_ALLOW_ALPHANUM    = UI_INPUT_ALLOW_LOWERCASE
					 		   | UI_INPUT_ALLOW_UPPERCASE
							   | UI_INPUT_ALLOW_DIGITS
} UIInputFlag;

typedef struct
{
	ButtonStyle * style;
	char * buf;
	u32 bufSize;
	u8 flags;
} UIInputField;

typedef struct 
{
	UINodeType type;
	union
	{
		UIText text;
		UIContainer container;
		UIImage image;
		UIButton button;
		UIInputField input;
	};
} UINodeData;

typedef struct 
{
	vec2 pos;
	vec2 size;
	f32 childGap;
	LayoutDirection axis;
	LayoutType justify;
	LayoutType align;
	SizingType sizing;
	SizingType sizingY;
	PositionType positioning;
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
	SizingType sizingY;
	LayoutType align;
	PositionType positioning;
	UIPadding padding;
	UINodeData data;
} UINode;

typedef struct 
{
	vec2 mousePos;
	ButtonInput mouseL;
	const u8 * charsPressed;
	u32 charCount;
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
	char strings[1024];
	u32 stringIndex;
	PlatformMeasureTextFn * measureText;
	UIInput input;

	void startLayout(vec2 size, LayoutType justify, LayoutType align, PlatformMeasureTextFn * measureTextFn, UIInput input);
	u32 begin(const char * label, UINodeLayout layout);
	void text(const char * label, i32 fontHandle, f32 fontSize, f32 strokeWidth, const char * text);
	void text(const char * label, FontStyle style, const char * text);
	void text(const char * label, i32 fontHandle, f32 fontSize, vec3 fillColor, const char * text);
	void image(const char * label, f32 width, f32 height, i32 imageHandle, vec2 absPos, vec4 uv);
	void button(const char * label, vec2 size, const ButtonStyle * buttonStyle, const char * text, FontStyle * textStyle);
	void button(const char * label, vec2 size, const ButtonStyle * buttonStyle);
	void inputField(const char * label, vec2 size, char * inputBuffer, u32 bufSize, ButtonStyle * style, FontStyle fontStyle, u8 inputFlags);
	void begin_button(const char * label, vec2 size, ButtonStyle * buttonStyle);
	bool isButtonPressed(const char * label);
	bool inputEntered(const char * label);
	void end();
	void endLayout();
} UILayout;

#endif
