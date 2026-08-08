#include "prefabs.hpp"

Array ParsePrefabInstancesCSV(const char * filename, Arena * arena)
{
	size_t debugCount = _countof(DEBUG_PREFAB_INSTANCE_DATA);
	Array instanceData = ArrayInit(sizeof(PrefabInstanceData), debugCount, ArenaPush(arena, sizeof(PrefabInstanceData) * debugCount));

	for (int i = 0; i < debugCount; i++)
	{
		ArrayPush(&instanceData, &DEBUG_PREFAB_INSTANCE_DATA[i]);
	}
	return instanceData;
}

PrefabInstance CreatePrefabInstance(PrefabInstanceData data, CollisionSystem2D * collision, TransformHierarchy * transforms)
{
	PrefabInstance instance;
	const Prefab * prefab = PREFABS[data.prefabID];
	instance.prefabID = data.prefabID;
	instance.transformIndex = transforms->AddTransform({data.position, data.scale, data.rotation});
	//instance.textureTransformIndex = transforms->AddTransform({prefab->position, prefab->scale, prefab->rotation}, instance.transformIndex);
	instance.colliderID = 100;
	
	if (collision != NULL)
	{
		for (int i = 0; i < prefab->colliderCount; i++)
		{
			const ColliderData2D * colliderData = &prefab->colliders[i];
			u32 transformIndex = transforms->AddTransform({colliderData->position, colliderData->size, colliderData->rotation}, instance.transformIndex);
			collision->AddCollider(colliderData->type, transformIndex, colliderData->isStatic);
		}
	}
	return instance;
}
