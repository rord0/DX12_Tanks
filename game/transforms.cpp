#include "transforms.hpp"
#include "tanks_math.hpp"

void Transform2D::SetPosition(vec2 pos)
{
	position = pos;
	isDirty = true;
}

void Transform2D::SetRotation(f32 rot)
{
	rotation = rot;
	isDirty = true;
}

u32 TransformHierarchy::AddTransform(Transform2D transform, u32 parentIndex)
{
	u32 index = count + 1;

	// Full
	if (index >= MAX_TRANSFORMS) { return 0; }

	transforms[index] = transform;
	if (parentIndex != 0 && parentIndex < MAX_TRANSFORMS)
	{
		parentIndexes[index] = parentIndex;
	}
	else
	{
		parentIndexes[index] = 0;
	}

	transforms[index].isDirty = true;
	count++;

	return index;
}

u32 TransformHierarchy::AddTransform(Transform2D transform) { return AddTransform(transform, 0); }

Transform2D * TransformHierarchy::GetRootTransform(u32 index)
{
	if (index != 0 && index < MAX_TRANSFORMS)
	{
		u32 parentIndex = parentIndexes[index];
		if (parentIndex == 0)
		{
			return &transforms[index];
		}
		else
		{
			return GetRootTransform(parentIndex);
		}
	}
	else
	{
		return NULL;
	}
}
Transform2D * TransformHierarchy::GetTransform(u32 index)
{
	if (index != 0 && index < MAX_TRANSFORMS)
	{
		return &transforms[index];
	}
	else
	{
		return NULL;
	}
}
mat4 LocalTransformMatrix(Transform2D * t)
{
	return ModelMatrix2D(t->position, t->rotation, t->scale);
}

void TransformHierarchy::UpdateTransforms(void)
{
	for (u32 i = 1; i < (count + 1); i++)
	{
		Transform2D * transform = &transforms[i];
		u32 parentIndex = parentIndexes[i];
		bool parentDirty = (parentIndex != 0 && transforms[parentIndex].isDirty);

		if (transforms[i].isDirty || parentDirty)
		{
			if (parentIndex != 0)
			{
				transform->world = LocalTransformMatrix(transform) * transforms[parentIndex].world;
			}
			else
			{
				transform->world = LocalTransformMatrix(transform);
			}
		}
	}

	for (u32 i = 1; i < count; i++)
	{
		transforms[i].isDirty = false;
	}
}
