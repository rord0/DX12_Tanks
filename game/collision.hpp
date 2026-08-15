#ifndef TANKS_COLLISION_HPP
#define TANKS_COLLISION_HPP

#include "../core.h"
#include "../array.h"
#include "arena.hpp"
#include "tanks_math.hpp"
#include "transforms.hpp"

typedef enum {
	COLLIDER_RECTANGLE,
	COLLIDER_CIRCLE
} ColliderType;

typedef struct {
	ColliderType type;
	vec2 position;
	f32 rotation;
	vec2 size;
	bool isStatic;
} ColliderData2D;

typedef struct {
	ColliderType type;
	u32 id;
	u32 transformIndex;
	bool isStatic;
	bool colliding;
} Collider2D;

typedef struct {
	f32 penetration;
	vec2 normal;
	vec2 point;
} CollisionData;

typedef struct CollisionSystem2D
{
	u32 nextColliderID;
	Array staticColliders;
	Array dynamicColliders;
	TransformHierarchy * transforms;
	u32 collisionChecks;
	u32 AddCollider(ColliderType type, u32 transformIndex, bool isStatic);
	bool RemoveCollider(u32 colliderID);
	void ResolveCollisions(TransformHierarchy * transforms);
	bool RaycastHit(vec2 start, vec2 end, vec2 * outPoint, u32 ignoreID, u32 * outColliderID);
	bool PointHit(vec2 point, u32 * outColliderID);
	bool AABBHits(vec2 pos, vec2 extents, Array * ids);
} CollisionSystem2D;

CollisionSystem2D InitCollisionSystem2D(Arena * arena, u32 maxStaticColliders, u32 maxDynamicColliders);

void GetRectWorldVertices(const Transform2D * t, vec2 vertices[4]);

#endif // TANKS_COLLISION_HPP
