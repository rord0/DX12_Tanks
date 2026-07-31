#include "../core.h"
#include "util.hpp"
#include <cstddef>
#include <cstring>
#include <execution>
#include <numbers>
#include <ctype.h>
#include "ui.hpp"

u32 UILayout::begin(const char * label, UINodeLayout layout)
{
	if (count >= 1024) { return 0; }
	u32 index = count;
	nodes[index] = {label,
				   fn1va_32(label),
				   layout.pos,
				   {layout.size.x, layout.size.y},
				   layout.childGap,
				   layout.axis,
				   0,
				   layout.justify,
				   layout.sizing,
				   layout.sizingY,
				   layout.align,
				   layout.positioning,
				   layout.padding,
				   layout.data};
	stack[depth] = index;
	sizes[index] = 1; // NOTE(rordon): Placeholder value since size will be 1 + the number of nodes in the subtrees after calling end().

	count++;
	depth++;

	return 0;
}

void UILayout::text(const char * label, FontStyle style, const char * text)
{
	if (!text) { return; }
	size_t textSize = strlen(text) + 1;
	if (stringIndex + textSize > sizeof(strings)) { return; }
	memcpy(&strings[stringIndex], text, textSize);
	UINodeData data = {.type = UINodeType::UI_NODE_TYPE_TEXT,
					   .text = {style.fontHandle, style.fontSize, style.strokeWidth, style.fillColor, &strings[stringIndex]}};

	begin(label, {.size = measureText(style.fontHandle, text, strlen(text), style.fontSize), .data = data});
	end();
	stringIndex += textSize;
}

void UILayout::text(const char * label, i32 fontHandle, f32 fontSize, f32 strokeWidth, const char * text)
{
	UILayout::text(label, {fontHandle, fontSize, strokeWidth, {1.0f, 1.0f, 1.0f}}, text);
}

void UILayout::text(const char * label, i32 fontHandle, f32 fontSize, vec3 fillColor, const char * text)
{
	UILayout::text(label, {fontHandle, fontSize, 0.0f, fillColor}, text);
}

void UILayout::image(const char * label, f32 width, f32 height, i32 imageHandle, vec2 pos = vec2::zero, vec4 uv = vec4::zero)
{
	UINodeData data = {.type = UINodeType::UI_NODE_TYPE_IMAGE, .image = {imageHandle, uv}};
	PositionType positioning = pos == vec2::zero ? PositionType::RELATIVE : PositionType::ABSOLUTE;
	begin(label, {.pos = pos, .size = {width, height}, .sizing = SizingType::FIXED, .positioning = positioning,.data = data});
	end();
}

void UILayout::button(const char * label, vec2 size, const ButtonStyle * buttonStyle)
{
	button(label, size, buttonStyle, NULL, NULL);
}
void UILayout::button(const char * label, vec2 size, const ButtonStyle * buttonStyle, const char * buttonText, FontStyle * textStyle)
{
	UINodeLayout buttonLayout = { .size = size,
								  .justify = LayoutType::CENTER,
								  .align = LayoutType::CENTER,
								  .sizing = SizingType::FIXED,
								  .data = {.type = UINodeType::UI_NODE_TYPE_BUTTON, .button = {.style = buttonStyle}}};

	begin(label, buttonLayout);
		if (buttonText != NULL && textStyle != NULL)
		{
			text("I_HOPE_THIS_DOESNT_BREAK_ANYTHING", textStyle->fontHandle, textStyle->fontSize, textStyle->strokeWidth, buttonText);
		}
	end();
}
void UILayout::begin_button(const char * label, vec2 size, ButtonStyle * buttonStyle)
{
	UINodeLayout buttonLayout = { .size = size,
								  .justify = LayoutType::CENTER,
								  .align = LayoutType::CENTER,
								  .sizing = SizingType::FIXED,
								  .data = {.type = UINodeType::UI_NODE_TYPE_BUTTON, .button = {.style = buttonStyle}}};

	begin(label, buttonLayout);
}

void UILayout::inputField(const char * label, vec2 size, char * inputBuffer, u32 bufSize, ButtonStyle * style, FontStyle fontStyle, u8 inputFlags)
{
	UINodeData containerData = {.type = UINodeType::UI_NODE_TYPE_INPUT_FIELD,
								.input = {.style = style, .buf = inputBuffer, .bufSize = bufSize, .flags = inputFlags}};
	UINodeLayout containerLayout = {.size = {0.0f, size.y},
								    .justify = LayoutType::START,
									.align = LayoutType::CENTER,
									.sizing = SizingType::FILL,
									.padding = {12.0f, 0, 0, 0}, .data = containerData};
	FontStyle placeholderFontStyle = {fontStyle.fontHandle, fontStyle.fontSize, 0.0f, ColorHexToRBGNormalized(0xA4A4A4)};
	begin(label, containerLayout);
		if (inputBuffer[0] != '\0')
		{
			text("I_HOPE_THIS_DOESNT_BREAK_ANYTHING", fontStyle.fontHandle, fontStyle.fontSize, fontStyle.strokeWidth, inputBuffer);
			if (active == fn1va_32(label))
			{
				text("I_HOPE_THIS_DOESNT_BREAK_ANYTHING", fontStyle.fontHandle, fontStyle.fontSize, fontStyle.strokeWidth, "|");
			}
		}
		else
		{
			text("I_HOPE_THIS_DOESNT_BREAK_ANYTHING", placeholderFontStyle, "Enter Text");
		}
	end();
}

void UILayout::startLayout(vec2 size,
						   LayoutType justify,
						   LayoutType align,
						   PlatformMeasureTextFn * measureTextFn,
						   PlatformCopyClipboardTextFn * copyClipboardTextFn,
						   UIInput newInput)
{
	input = newInput;
	count = 0;
	depth = 0;
	hot = 0;
	stringIndex = 0;
	measureText = measureTextFn;
	copyClipboardText = copyClipboardTextFn;
	UINodeLayout rootNodeLayout = {.size = size,
								   .childGap = 30.0f,
								   .axis = LayoutDirection::TOP_TO_BOTTOM,
								   .justify = justify,
								   .align = align,
								   .sizing = SizingType::FIXED};

	begin("ROOT", rootNodeLayout);
}

f32 CalculateChildGap(UILayout & ui, u32 parentIndex)
{
	u32 numChildren = ui.nodes[parentIndex].numChildren;
	return numChildren > 0 ? (numChildren - 1) * ui.nodes[parentIndex].childGap : 0.0f;
}

f32 CalculateTotalChildGap(UILayout & ui, u32 parentIndex)
{
	u32 numChildren = ui.nodes[parentIndex].numChildren;
	return numChildren > 0 ? (numChildren - 1) * ui.nodes[parentIndex].childGap : 0.0f;
}

f32 CalculateNodeSizeOnAxis(UILayout & ui, bool xAxis, u32 index)
{
	UINode * node = &ui.nodes[index];
	f32 size = xAxis ? node->size.x : node->size.y;
	SizingType sizing = xAxis ? node->sizing : node->sizingY;

	if (sizing != SizingType::FIXED)
	{
		f32 totalChildGap = CalculateTotalChildGap(ui, index);
		size += xAxis ? (node->padding.left + node->padding.right) : (node->padding.top + node->padding.bottom);
		size += totalChildGap;
	}

	return size;
}

f32 CalculateNodeSizeCrossAxis(UILayout & ui, bool xAxis, u32 index)
{
	UINode * node = &ui.nodes[index];
	f32 size = xAxis ? node->size.x : node->size.y;
	SizingType sizing = xAxis ? node->sizing : node->sizingY;
	if (sizing != SizingType::FIXED)
	{
		size += xAxis ? (node->padding.left + node->padding.right) : (node->padding.top + node->padding.bottom);
	}

	return size;
}

void UILayout::end()
{
	depth--;
	u32 index = stack[depth];
	sizes[index] = count - index;

	UINode * node = &nodes[index];
	if (depth > 0)
	{
		u32 parentIndex = stack[depth - 1];
		UINode * parent = &nodes[parentIndex];
		parent->numChildren++;

		if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
		{
			node->size.x = CalculateNodeSizeOnAxis(*this, true, index);
			node->size.y = CalculateNodeSizeCrossAxis(*this, false, index);
		}
		else
		{
			node->size.x = CalculateNodeSizeCrossAxis(*this, true, index);
			node->size.y = CalculateNodeSizeOnAxis(*this, false, index);
		}

		if (parent->sizing != SizingType::FIXED)
		{
			if (parent->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
			{
				parent->size.x += node->size.x;
				parent->size.y = std::max(node->size.y, parent->size.y);
			}
			else
			{
				parent->size.x = std::max(node->size.x, parent->size.x);
				parent->size.y += node->size.y;
			}
		}
	}
}

f32 NodeAxisSize(UINode * node, LayoutDirection axis)
{
	return axis == LayoutDirection::LEFT_TO_RIGHT ? node->size.x : node->size.y;
}

f32 NodePaddingOnAxis(UINode * node, LayoutDirection axis)
{
	return axis == LayoutDirection::LEFT_TO_RIGHT ? node->padding.left + node->padding.right : node->padding.top + node->padding.bottom;
}

f32 CalculateSizeofChildrenOnCrossAxis(UILayout & ui, u32 index)
{
	f32 sum = 0.0f;
	u32 end = index + ui.sizes[index];
	u32 childIndex = index + 1;

	while (childIndex < end)
	{
		UINode * childNode = &ui.nodes[childIndex];
		sum += NodeAxisSize(childNode, ui.nodes[index].layoutDirection == LayoutDirection::LEFT_TO_RIGHT ? LayoutDirection::TOP_TO_BOTTOM : LayoutDirection::LEFT_TO_RIGHT);
		childIndex += ui.sizes[childIndex];
	}
	return sum;
}

f32 CalculateSizeofChildrenOnAxis(UILayout & ui, u32 index)
{
	f32 sum = 0.0f;
	u32 end = index + ui.sizes[index];
	u32 childIndex = index + 1;

	while (childIndex < end)
	{
		UINode * childNode = &ui.nodes[childIndex];
		sum += NodeAxisSize(childNode, ui.nodes[index].layoutDirection);
		childIndex += ui.sizes[childIndex];
	}
	return sum;
}

void FillChildElementsOnAxis(UILayout & ui, bool xAxis, u32 parentIndex)
{
	UINode * parent = &ui.nodes[parentIndex];
	f32 remainingSize = xAxis ? parent->size.x : parent->size.y;
	remainingSize -= xAxis ? (parent->padding.left + parent->padding.right) : (parent->padding.top + parent->padding.bottom);
	remainingSize -= CalculateTotalChildGap(ui, parentIndex);
	remainingSize -= CalculateSizeofChildrenOnAxis(ui, parentIndex);

	u32 end = parentIndex + ui.sizes[parentIndex];
	u32 childIndex = parentIndex + 1;

	while (childIndex < end)
	{
		UINode * childNode = &ui.nodes[childIndex];
		SizingType childSizing = xAxis ? childNode->sizing : childNode->sizingY;
		if (childSizing == SizingType::FILL)
		{
			if (xAxis)
			{
				childNode->size.x += remainingSize;
			}
			else
			{
				childNode->size.y += remainingSize;
			}
		}
		childIndex += ui.sizes[childIndex];
	}
}

void FillChildElementsCrossAxis(UILayout & ui, bool xAxis, u32 parentIndex)
{
	UINode * parent = &ui.nodes[parentIndex];
	f32 remainingSize = xAxis ? parent->size.x : parent->size.y;
	remainingSize -= xAxis ? (parent->padding.left + parent->padding.right) : (parent->padding.top + parent->padding.bottom);

	u32 end = parentIndex + ui.sizes[parentIndex];
	u32 childIndex = parentIndex + 1;

	while (childIndex < end)
	{
		UINode * childNode = &ui.nodes[childIndex];
		SizingType childSizing = xAxis ? childNode->sizing : childNode->sizingY;
		if (childSizing == SizingType::FILL)
		{
			if (xAxis)
			{
				childNode->size.x += remainingSize - childNode->size.x;
			}
			else
			{
				childNode->size.y += remainingSize - childNode->size.y;
			}
		}
		childIndex += ui.sizes[childIndex];
	}
}

void CalculateFillSizes(UILayout & ui, u32 rootIndex)
{
	u32 indexes[1024];
	RingQueue queue = rQueueInit(sizeof(u32), 1024, (void*)indexes);
	RING_QUEUE_PUSH(&queue, rootIndex);

	while (queue.count != 0)
	{
		u32 index;
		rQueuePop(&queue, &index);

		UINode * node = &ui.nodes[index];
		//ProcessNode
		if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
		{
			FillChildElementsOnAxis(ui, true, index);
		}
		else
		{
			FillChildElementsCrossAxis(ui, true, index);
		}
		u32 end = index + ui.sizes[index];
		u32 childIndex = index + 1;

		while (childIndex < end)
		{
			rQueuePush(&queue, &childIndex);
			childIndex += ui.sizes[childIndex];
		}
	}
}

void CalculateChildPositions(UILayout & ui, u32 index)
{
	UINode * node = &ui.nodes[index];
	vec2 curser = node->pos;
	curser.x += node->padding.left;
	curser.y += node->padding.top;

	u32 end = index + ui.sizes[index];
	u32 childIndex = index + 1;
	if (node->numChildren > 0)
	{
		if (node->justify == LayoutType::SPACE_BETWEEN && node->numChildren > 0)
		{
			node->childGap = (NodeAxisSize(node, node->layoutDirection) - NodePaddingOnAxis(node, node->layoutDirection)- CalculateSizeofChildrenOnAxis(ui, index)) / (node->numChildren - 1);
		}
		else if (node->justify == LayoutType::END)
		{
			if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
			{
				curser.x = node->pos.x + (node->size.x - CalculateSizeofChildrenOnAxis(ui, index)) - CalculateChildGap(ui, index) - node->padding.right;
			}
			else
			{
				curser.y = node->pos.y + (node->size.y - CalculateSizeofChildrenOnAxis(ui, index)) - CalculateChildGap(ui, index);
			}
		}
		else if (node->justify == LayoutType::CENTER)
		{
			if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
			{
				curser.x = node->pos.x + (node->size.x - CalculateSizeofChildrenOnAxis(ui, index) - CalculateChildGap(ui, index))/2.0f;
			}
			else
			{
				curser.y = node->pos.y + (node->size.y - CalculateSizeofChildrenOnAxis(ui, index) - CalculateChildGap(ui, index))/2.0f;
			}
		}
	}

	while (childIndex < end)
	{
		UINode * childNode = &ui.nodes[childIndex];
		if (childNode->positioning == PositionType::RELATIVE)
		{
			if (node->align == LayoutType::CENTER)
			{
				if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
				{
					curser.y = node->pos.y + (node->size.y - NodeAxisSize(childNode, LayoutDirection::TOP_TO_BOTTOM)) / 2.0f;
				}
				else
				{
					curser.x = node->pos.x + (node->size.x - NodeAxisSize(childNode, LayoutDirection::LEFT_TO_RIGHT)) / 2.0f;
				}
			}
			else if (node->align == LayoutType::END)
			{
				if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
				{
					curser.y = node->pos.y + (node->size.y - CalculateSizeofChildrenOnCrossAxis(ui, index)) - CalculateChildGap(ui, index);
				}
				else
				{
					curser.x = node->pos.x + (node->size.x - CalculateSizeofChildrenOnCrossAxis(ui, index)) - CalculateChildGap(ui, index) - node->padding.right;
				}
			}
			childNode->pos = curser;
			CalculateChildPositions(ui, childIndex);
			if (node->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
			{
				curser.x += childNode->size.x + node->childGap;
			}
			else
			{
				curser.y += childNode->size.y + node->childGap;
			}
		}
		else
		{
			childNode->pos = node->pos + childNode->pos;
			CalculateChildPositions(ui, childIndex);
		}

		childIndex += ui.sizes[childIndex];
	}
}

UINode * FindNode(UILayout * layout, u32 id)
{
	for (int i = 0; i < layout->count; i++)
	{
		if (layout->nodes[i].id == id)
		{
			return &layout->nodes[i];
		}
	}
	return NULL;
}

bool isMouseInsideElement(vec2 pos, UINode * node)
{
	return (pos.x >= node->pos.x && pos.x <= node->pos.x + node->size.x) &&
		   (pos.y >= node->pos.y && pos.y <= node->pos.y + node->size.y);
}

bool UILayout::isButtonPressed(const char * label)
{
	u32 id = fn1va_32(label); // calc the hash.
							  //
	UINode * node = FindNode(this, id);
	if (!node) { return false; }

	if (isMouseInsideElement(input.mousePos, node))
	{
		hot = id; // Set element to hot. (fire emoji)
	}

	if (active == node->id)
	{
		if (input.mouseL.wasReleased)
		{
			active = 0;
			if (hot == id)
			{
				return true;
			}
		}
	}
	else if (hot == node->id && input.mouseL.wasPressed)
	{
		active = id; // Set this to be active element.
	}

	return false;
}

bool InputFieldAddChar(UIInputField * inputField, char c, u32 & strLen)
{
	u8 flags = inputField->flags;
	if (c >= 32 && c <= 126)
	{
		// lowercase ASCII
		if ((islower(c) && (flags & UI_INPUT_ALLOW_LOWERCASE))   ||
			(isupper(c) && (flags & UI_INPUT_ALLOW_UPPERCASE))   ||
			(isdigit(c) && (flags & UI_INPUT_ALLOW_DIGITS))      ||
			(c == '_'   && (flags & UI_INPUT_ALLOW_UNDERSCORES)) ||
			(c == '.'   && (flags & UI_INPUT_ALLOW_PERIODS))     ||
			(c == ':'   && (flags & UI_INPUT_ALLOW_COLONS)))
		{
			if (strLen < (inputField->bufSize - 1))
			{
				inputField->buf[strLen++] = c; 
			}
		}
	}
	if (c == '\b')
	{
		if (strLen > 0)
		{
			inputField->buf[--strLen] = '\0'; 
		}
	}

	return (strLen < (inputField->bufSize - 1));
}

bool UILayout::inputEntered(const char * label)
{
	u32 id = fn1va_32(label); // calc the hash.
							  //
	UINode * node = FindNode(this, id);
	u32 len = strnlen(node->data.input.buf, node->data.input.bufSize);
	if (!node) { return false; }

	if (isMouseInsideElement(input.mousePos, node))
	{
		hot = id; // Set element to hot. (fire emoji)
	}
	else if (active == node->id)
	{
		if (input.mouseL.wasReleased)
		{
			active = 0;
		}
	}
	if (active == node->id)
	{
		if (input.CTRL_V.isDown && !input.CTRL_V.wasDown)
		{
			char clipboard[128] = {0};
			u32 numChars = copyClipboardText(clipboard, 128);
			for (int i = 0; i < numChars; i++)
			{
				if (!InputFieldAddChar(&node->data.input, clipboard[i], len)) { break; }
			}
		}
		for (int i = 0; i < input.charCount; i++)
		{
			char c = input.charsPressed[i];
			if (!InputFieldAddChar(&node->data.input, c,len)) { break; }
		}
	}

	if (hot == node->id && input.mouseL.wasPressed)
	{
		active = id; // Set this to be active element.
	}

	return false;
}

void UILayout::endLayout()
{
	end(); // End root.
	CalculateFillSizes(*this, 0);
	CalculateChildPositions(*this, 0);
}
