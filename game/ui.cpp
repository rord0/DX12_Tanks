#include "../core.h"
#include "util.hpp"
#include <cstddef>
#include <numbers>
#include "ui.hpp"


u32 UILayout::begin(const char * label,
		f32 width, f32 height, f32 childGap,
		LayoutDirection layoutDirection = LayoutDirection::LEFT_TO_RIGHT,
		LayoutType justify = LayoutType::START,
		SizingType sizing = SizingType::HUG,
		LayoutType align = LayoutType::START)
{
	if (count >= 1024) { return 0; }
	u32 index = count;
	nodes[index] = {label, fn1va_32(label), {0,0}, {width, height}, childGap, layoutDirection, 0, justify, sizing, align};
	stack[depth] = index;
	sizes[index] = 1; // NOTE(rordon): Placeholder value since size will be 1 + the number of nodes in the subtrees after calling end().

	count++;
	depth++;

	return 0;
}

u32 UILayout::begin(const char * label, UINodeLayout layout)
{
	if (count >= 1024) { return 0; }
	u32 index = count;
	nodes[index] = {label,
				   fn1va_32(label),
				   {0.0f, 0.0f},
				   {layout.size.x, layout.size.y},
				   layout.childGap,
				   layout.axis,
				   0,
				   layout.justify,
				   layout.sizing,
				   layout.align,
				   layout.padding,
				   layout.data};
	stack[depth] = index;
	sizes[index] = 1; // NOTE(rordon): Placeholder value since size will be 1 + the number of nodes in the subtrees after calling end().

	count++;
	depth++;

	return 0;
}

void UILayout::text(const char * label, i32 fontHandle, f32 fontSize, f32 strokeWidth, const char * text)
{
	UINodeData data = {.type = UINodeType::UI_NODE_TYPE_TEXT, .text = {fontHandle, fontSize, strokeWidth, text}};
	begin(label, {.size = measureText(fontHandle, text, strlen(text), fontSize), .data = data});
	end();
}

void UILayout::image(const char * label, f32 width, f32 height, i32 imageHandle)
{
	UINodeData data = {.type = UINodeType::UI_NODE_TYPE_IMAGE, .image = {imageHandle}};
	begin(label, {.size = {width, height}, .sizing = SizingType::FIXED, .data = data});
	end();
}

void UILayout::button(const char * label, vec2 size, ButtonStyle * buttonStyle)
{
	button(label, size, buttonStyle, NULL, NULL);
}
void UILayout::button(const char * label, vec2 size, ButtonStyle * buttonStyle, const char * buttonText, FontStyle * textStyle)
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

void UILayout::startLayout(f32 width, f32 height, PlatformMeasureTextFn * measureTextFn, UIInput newInput)
{
	input = newInput;
	count = 0;
	depth = 0;
	hot = 0;
	measureText = measureTextFn;
	begin("ROOT", width, height, 30.0f, LayoutDirection::TOP_TO_BOTTOM, LayoutType::SPACE_BETWEEN, SizingType::FIXED, LayoutType::CENTER);
}

f32 CalculateChildGap(UILayout & ui, u32 parentIndex)
{
	u32 numChildren = ui.nodes[parentIndex].numChildren;
	return numChildren > 0 ? (numChildren - 1) * ui.nodes[parentIndex].childGap : 0.0f;
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
		f32 childGap = CalculateChildGap(*this, parentIndex);
		node->size.x += node->padding.right + node->padding.left;
		node->size.y += node->padding.top + node->padding.bottom;

		if (parent->sizing == SizingType::HUG)
		{
			if (parent->layoutDirection == LayoutDirection::LEFT_TO_RIGHT)
			{
				parent->size.x += node->size.x + childGap;
				parent->size.y = std::max(node->size.y, parent->size.y);
			}
			else
			{
				parent->size.x = std::max(node->size.x, parent->size.x);
				parent->size.y += node->size.y + childGap;
			}
		}
	}
}

f32 NodeAxisSize(UINode * node, LayoutDirection axis)
{
	return axis == LayoutDirection::LEFT_TO_RIGHT ? node->size.x : node->size.y;
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
			node->childGap = (NodeAxisSize(node, node->layoutDirection) - CalculateSizeofChildrenOnAxis(ui, index)) / (node->numChildren - 1);
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

void UILayout::endLayout()
{
	end(); // End root.
	CalculateChildPositions(*this, 0);
}
