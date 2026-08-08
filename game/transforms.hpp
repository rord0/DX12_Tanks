#ifndef TANKS_TRANSFORMS_HPP
#define TANKS_TRANSFORMS_HPP

#include "../core.h"

typedef struct Transform2D
{
	vec2 position;
	vec2 scale;
	f32 rotation;
	mat4 world;
	bool isDirty;
	void SetPosition(vec2 pos);
	void SetRotation(f32 rot);
} Transform2D;

#define MAX_TRANSFORMS 512
typedef struct Transform2DHierarchy_t
{
	Transform2D transforms[MAX_TRANSFORMS];
	u32 parentIndexes[MAX_TRANSFORMS];
	u32 count;

	u32 AddTransform(Transform2D transform, u32 parentIndex);
	u32 AddTransform(Transform2D transform);
	Transform2D * GetTransform(u32 index);
	Transform2D * GetRootTransform(u32 index);
	void UpdateTransforms(void);
} TransformHierarchy;

#endif // TANKS_TRANSFORMS_HPP
