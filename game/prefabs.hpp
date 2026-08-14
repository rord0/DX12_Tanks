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
	const char * name;
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

#define PROPS_RENDER_SORT_LAYER 4

const ColliderData2D AIRDROP_COLLIDERS[]   = {COLLIDER_RECTANGLE, {0.0f, 0.0f}, 0.0f, {0.2f, 0.2f}, true};
const ColliderData2D CONTAINER_COLLIDERS[] = {COLLIDER_RECTANGLE, {0.0f, 0.0f}, 0.0f, {0.7f, 0.4f}, true};
const ColliderData2D BARREL_COLLIDERS[]    = {COLLIDER_CIRCLE,    {0.0f, 0.0f}, 0.0f,  {0.2f, 0.2f}, true};
const ColliderData2D TIRE_COLLIDERS[]      = {COLLIDER_CIRCLE,    {0.0f, 0.0f}, 0.0f,  {0.25f, 0.25f}, true};
const ColliderData2D TOWER_COLLIDERS[]     = {COLLIDER_RECTANGLE, {0.0f, 0.0f}, 0.0f,  {0.5f, 0.5f}, true};

const ColliderData2D BUILDING_LARGE_COLLIDERS[2] = {{COLLIDER_RECTANGLE, {0.025f, 0.0f}, 0.0f, {0.44f, 0.475f}, true},
												    {COLLIDER_RECTANGLE, {-0.22f, 0.044f}, 0.0f, {0.09f, 0.275f}, true}};

const ColliderData2D BUILDING_SMALL_COLLIDERS[] = {COLLIDER_RECTANGLE, {-0.01f, -0.01f}, 0.0f,  {0.5f, 0.4f}, true};

const Prefab PREFAB_AIRDROP = {TEXTURE_AIRDROP_PATH, {0.0f, 0.0f}, {0.5f, 0.5f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(AIRDROP_COLLIDERS), AIRDROP_COLLIDERS, "Airdrop"};
const Prefab PREFAB_BARREL = {TEXTURE_BARREL_PATH, {0.0f, 0.0f}, {0.25f, 0.25f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(BARREL_COLLIDERS), BARREL_COLLIDERS, "Barrel"};

const Prefab PREFAB_BUILDING_LARGE = {TEXTURE_BUILDING_LARGE_PATH, {0.0f, 0.0f}, {1.15f, 1.0f}, 0.0f,
								 	 PROPS_RENDER_SORT_LAYER, _countof(BUILDING_LARGE_COLLIDERS), BUILDING_LARGE_COLLIDERS, "Building Large"};

const Prefab PREFAB_BUILDING_SMALL = {TEXTURE_BUILDING_SMALL_PATH, {0.0f, 0.0f}, {1.15f, 1.0f}, 0.0f,
								 	 PROPS_RENDER_SORT_LAYER, _countof(BUILDING_SMALL_COLLIDERS), BUILDING_SMALL_COLLIDERS, "Building Small"};

const Prefab PREFAB_TIRE = {TEXTURE_TIRES_PATH, {0.0f, 0.0f}, {0.33f, 0.33f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(TIRE_COLLIDERS), TIRE_COLLIDERS, "Tires"};

const Prefab PREFAB_CONTAINER = {TEXTURE_CONTAINER_PATH, {0.0f, 0.0f}, {1.0f * 1.5f, 0.5859375f * 1.5f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(CONTAINER_COLLIDERS), CONTAINER_COLLIDERS, "Container"};

const Prefab PREFAB_TOWER = {TEXTURE_TOWER_PATH, {0.0f, 0.0f}, {1.2f, 1.2f}, 0.0f,
							  PROPS_RENDER_SORT_LAYER, _countof(TOWER_COLLIDERS), TOWER_COLLIDERS, "Tower"};

const Prefab * PREFABS[] = {&PREFAB_AIRDROP, &PREFAB_BARREL, &PREFAB_BUILDING_LARGE,
							&PREFAB_CONTAINER, &PREFAB_TIRE, &PREFAB_TOWER,
							&PREFAB_BUILDING_SMALL};

PrefabInstance TEST_PREFAB_INSTANCE = {};
PrefabInstanceData DEBUG_PREFAB_INSTANCE_DATA[] = {
	{0, {0.0f, -0.5f}, {1.0f, 1.0f}, 0.0f},
	{1, {0.0f,  0.0f},  {1.0f, 1.0f}, 0.57f},
	{2, {-0.5f, 0.75f},  {1.5f, 1.5f}, 0.0f},
	{0, {0.5f, 1.0f},	{1.0f, 1.0f}, 0.57f},
};

Array ParsePrefabInstancesCSV(const char * filename, PlatformAPI * platform, Arena * arena);

PrefabInstance CreatePrefabInstance(PrefabInstanceData data, CollisionSystem2D * collision, TransformHierarchy * transforms);

bool SerializePrefabInstancesCSV(const char * filepath, const Array * instances, TransformHierarchy * transforms, PlatformAPI * platform, Arena * arena);

#endif // PREFABS_HPP
