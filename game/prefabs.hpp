#ifndef TANKS_PREFABS_HPP
#define TANKS_PREFABS_HPP
#include "../core.h"
#include "../array.h"
#include "arena.hpp"
#include "tanks_math.hpp"
#include "tanks.hpp"
#include "transforms.hpp"
#include "collision.hpp"

typedef struct
{
	const char * textureName;
	vec2 position;
	vec2 scale;
	f32 rotation;
	u32 layer;
	u32 colliderCount;
	const ColliderData2D * colliders;
} Prefab;

typedef struct
{
	u32  prefabID;
	vec2 position;
	vec2 scale;
	f32  rotation;
} PrefabInstanceData;

typedef struct
{
	u32 prefabID;
	u32 transformIndex;
	u32 textureTransformIndex;
	u32 colliderID;
} PrefabInstance;

#define PROPS_RENDER_SORT_LAYER 2

const ColliderData2D AIRDROP_COLLIDERS[] = {COLLIDER_RECTANGLE, {0.0f, 0.0f}, 0.0f, {0.2f, 0.2f}, true};
const ColliderData2D BARREL_COLLIDERS[]  = {COLLIDER_CIRCLE,    {0.0f,0.0f}, 0.0f,  {0.2f, 0.2f}, true};

const ColliderData2D BUILDING_LARGE_COLLIDERS[2] = {{COLLIDER_RECTANGLE, {0.025f, 0.0f}, 0.0f, {0.45f, 0.475f}, true},
												    {COLLIDER_RECTANGLE, {-0.22f, 0.044f}, 0.0f, {0.09f, 0.275f}, true}};

const Prefab PREFAB_AIRDROP = {TEXTURE_AIRDROP_PATH, {0.0f, 0.0f}, {0.5f, 0.5f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(AIRDROP_COLLIDERS), AIRDROP_COLLIDERS};
const Prefab PREFAB_BARREL = {TEXTURE_BARREL_PATH, {0.0f, 0.0f}, {0.25f, 0.25f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(BARREL_COLLIDERS), BARREL_COLLIDERS};
const Prefab PREFAB_BUILDING_LARGE = {TEXTURE_BUILDING_LARGE_PATH, {0.0f, 0.0f}, {1.15f, 1.0f}, 0.0f,
								 	 PROPS_RENDER_SORT_LAYER, _countof(BUILDING_LARGE_COLLIDERS), BUILDING_LARGE_COLLIDERS};

const Prefab * PREFABS[3] = {&PREFAB_AIRDROP, &PREFAB_BARREL, &PREFAB_BUILDING_LARGE};

PrefabInstance TEST_PREFAB_INSTANCE = {};
PrefabInstanceData DEBUG_PREFAB_INSTANCE_DATA[] = {
	{0, {0.0f, -0.5f}, {1.0f, 1.0f}, 0.0f},
	{1, {0.0f,  0.0f},  {1.0f, 1.0f}, 0.57f},
	{2, {-0.5f, 0.75f},  {1.5f, 1.5f}, 0.0f},
	{0, {0.5f, 1.0f},	{1.0f, 1.0f}, 0.57f},
};

Array ParsePrefabInstancesCSV(const char * filename, Arena * arena);

PrefabInstance CreatePrefabInstance(PrefabInstanceData data, CollisionSystem2D * collision, TransformHierarchy * transforms);

#endif // PREFABS_HPP
