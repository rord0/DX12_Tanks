#include "prefabs.hpp"
#include "tanks.hpp"
#include "transforms.hpp"
#include "util.hpp"
#include <cstddef>

Array ParsePrefabInstancesCSV(const char * filepath, PlatformAPI * platform, Arena * arena)
{
    // read csv file into buffer.
    DEBUG_FileResult fileResult = platform->platformLoadFile(filepath);
    if (fileResult.size == 0) { return {}; }

    // Count the number of lines 
    u32 numLines = CountNewLines((char*)fileResult.data, fileResult.size);

	Array instances = ArrayInit(sizeof(PrefabInstanceData), numLines - 1, ArenaPush(arena, sizeof(PrefabInstanceData) * (numLines - 1)));

    // Parse records
    const char* ptr = (char*)fileResult.data;
    const char* end = (char*)fileResult.data + fileResult.size;

    // Skip first line
    while (ptr < end && *ptr != '\n') { ptr++; }
    ptr++;

    while (ptr < end)
	{
		PrefabInstanceData instanceData;
		if (!CSVParseU32Field(ptr, end, instanceData.prefabID))   { break; }
		if (!CSVParseF32Field(ptr, end, instanceData.position.x)) { break; }
		if (!CSVParseF32Field(ptr, end, instanceData.position.y)) { break; }
		if (!CSVParseF32Field(ptr, end, instanceData.scale.x))    { break; }
		if (!CSVParseF32Field(ptr, end, instanceData.scale.y))    { break; }
		if (!CSVParseF32Field(ptr, end, instanceData.rotation))   { break; }

		if (!ArrayPush(&instances, &instanceData)) { break; }
	}

	return instances;
}

bool SerializePrefabInstancesCSV(const char * filepath, const Array * instances, TransformHierarchy * transforms, PlatformAPI * platform, Arena * arena)
{
	PrefabInstanceData * instanceData = (PrefabInstanceData*)ArenaPush(arena, sizeof(PrefabInstanceData) * instances->count);
	for (int i = 0; i < instances->count; i++)
	{
		const PrefabInstance * instance = (const PrefabInstance*)instances->elements + i;
		Transform2D * transform =  transforms->GetTransform(instance->transformIndex);

		instanceData[i].prefabID = instance->prefabID;
		instanceData[i].position = transform->position;
		instanceData[i].rotation = transform->rotation;
		instanceData[i].scale = transform->scale;
	}

	char * buffer = (char*)ArenaPush(arena, KB(16));
	size_t bufferSize = KB(16);
	size_t remaining = bufferSize;
	char * ptr = buffer;

	// Write to CSV
	const char * header = "prefab_id,x_pos,y_pos,x_scale,y_scale,rotation\n";

	size_t headerLen = strlen(header);
	memcpy(ptr, header, headerLen);
	ptr += headerLen;
	remaining -= headerLen;

	for (int i = 0; i < instances->count; i++)
	{
		int n = snprintf(ptr, remaining, "%u,%f,%f,%f,%f,%f\n",
			instanceData[i].prefabID,
			instanceData[i].position.x, instanceData[i].position.y,
			instanceData[i].scale.x, instanceData[i].scale.y,
			instanceData[i].rotation);

		if (n < 0 || (size_t)n >= remaining) { return false; }

		ptr += n;
		remaining -= n;
	}

	return platform->writeFile(RESOURCES_PATH"meow.csv", buffer, bufferSize - remaining);
}

PrefabInstance CreatePrefabInstance(PrefabInstanceData data, CollisionSystem2D * collision, TransformHierarchy * transforms)
{
	PrefabInstance instance;
	const Prefab * prefab = PREFABS[data.prefabID];
	instance.prefabID = data.prefabID;
	instance.transformIndex = transforms->AddTransform({data.position, data.scale, data.rotation});
	
	if (collision != NULL)
	{
		for (int i = 0; i < prefab->colliderCount; i++)
		{
			const ColliderData2D * colliderData = &prefab->colliders[i];
			u32 transformIndex = transforms->AddTransform({colliderData->position, colliderData->size, colliderData->rotation}, instance.transformIndex);
			instance.colliderID = collision->AddCollider(colliderData->type, transformIndex, colliderData->isStatic);
		}
	}
	return instance;
}
