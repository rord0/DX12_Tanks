#include "../core.h"
#include <numbers>
#include "ui.hpp"


u32 UILayout::begin(const char * label,
		f32 width, f32 height, f32 childGap,
		LayoutDirection layoutDirection = LayoutDirection::LEFT_TO_RIGHT,
		LayoutType justify = LayoutType::START,
		SizingType sizing = SizingType::HUG)
{
	if (count >= 1024) { return 0; }
	u32 index = count;
	nodes[index] = {label, {0,0}, {width, height}, childGap, layoutDirection, 0, justify, sizing};
	stack[depth] = index;
	sizes[index] = 1; // NOTE(rordon): Placeholder value since size will be 1 + the number of nodes in the subtrees after calling end().

	count++;
	depth++;

	return 0;
}

void UILayout::startLayout(f32 width, f32 height)
{
	begin("ROOT", width, height, 0.0f, LayoutDirection::TOP_TO_BOTTOM, LayoutType::SPACE_BETWEEN, SizingType::FIXED);
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

	u32 end = index + ui.sizes[index];
	u32 childIndex = index + 1;
	if (node->justify == LayoutType::SPACE_BETWEEN && node->numChildren > 0)
	{
		node->childGap = (NodeAxisSize(node, node->layoutDirection) - CalculateSizeofChildrenOnAxis(ui, index)) / (node->numChildren - 1);
	}

	while (childIndex < end)
	{
		UINode * childNode = &ui.nodes[childIndex];
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

void UILayout::endLayout()
{
	end(); // End root.
	CalculateChildPositions(*this, 0);
}
