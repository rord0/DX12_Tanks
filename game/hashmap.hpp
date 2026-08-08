#ifndef HASHMAP_HPP
#define HASHMAP_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdint.h>

typedef struct 
{
	uint8_t state;
	const char * key;
} HashEntry;

typedef struct {
    size_t capacity;
    size_t elementSize;
    size_t count;
    void * memory;
} HashMap;

#define HASHMAP_OCCUPIED_ELEMENT 0x0C
#define HASHMAP_DELETED_ELEMENT 0xDE
#define HASHMAP_EMPTY_ELEMENT  0x00

size_t HashMapSizeRequired(size_t elementSize, size_t minCapacity);
HashMap HashMapInit(size_t elementSize, size_t minCapacity, void * memory);
bool HashMapInsert(HashMap * h, const char * key, const void * element);

#endif // HASHMAP_HPP

#ifndef HASHMAP_IMPLEMENTATION

uint32_t DJ2B(const char * str)
{
	uint32_t hash = 5381;
	int c;

	while (c = *str++)
	{
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

uint32_t isPrime(uint32_t n)
{
    if (n == 2 || n == 3)                   return 1;
    if (n <= 1 || n % 2 == 0 || n % 3 == 0) return 0;

    for (int i = 5; i * i <= n; i += 6)
    {
        if (n % i == 0 || n % (i + 2) == 0) return 0;
    }

    return 1;
}

uint32_t getNearestPrimeGE(uint32_t n)
{
    while (!isPrime(n)) { n++; }
    return n;
}

size_t HashMapSizeRequired(size_t elementSize, size_t minCapacity)
{
	return (sizeof(HashEntry) + elementSize) * getNearestPrimeGE(minCapacity);
}

HashMap HashMapInit(size_t elementSize, size_t minCapacity, void * memory)
{
	HashMap h = {0};
	if (!memory || elementSize == 0) { return h; }

	h.elementSize = elementSize;
	h.capacity = getNearestPrimeGE(minCapacity);
	h.count = 0;
	h.memory = memory;

	return h;
}

bool HashMapInsert(HashMap * h, const char * key, const void * element)
{
	if (h == NULL || key == NULL || element == NULL) { return false; }

    uint32_t hashIndex = DJ2B(key) % h->capacity;
    int32_t insertIndex = -1;

	for (int i = 0; i < h->capacity; i++)
	{
		uint32_t currentIndex = (hashIndex + i) % h->capacity;
		uint8_t * entryAddress = (uint8_t*)h->memory + (currentIndex * (sizeof(HashEntry) + h->elementSize));
		HashEntry * entry = (HashEntry*)(entryAddress);

		if (entry->state == HASHMAP_OCCUPIED_ELEMENT && strcmp(entry->key, key) == 0)
		{
			memcpy(entryAddress + sizeof(HashEntry), element, h->elementSize);
			return true;
		}

       	if (entry->state == HASHMAP_DELETED_ELEMENT)
        {
            if (insertIndex == -1) { insertIndex = (int32_t)currentIndex; }
            continue;
        }

        if (entry->state == HASHMAP_EMPTY_ELEMENT)
        {
            // End of probe chain.
			if (insertIndex == -1) { insertIndex = currentIndex; }
            break;
        }
	}

	if (insertIndex == -1) { return false; } // Table full.

	uint8_t * insertAddress = (uint8_t*)h->memory + (insertIndex * (sizeof(HashEntry) + h->elementSize));
	HashEntry * insertEntry = (HashEntry*)insertAddress;

	insertEntry->key = key;
	insertEntry->state = HASHMAP_OCCUPIED_ELEMENT;
	memcpy(insertAddress + sizeof(HashEntry), element, h->elementSize);

	h->count++;
	return true;
}

int32_t __hashMapGetIndex(HashMap * h, const char * key)
{
    uint32_t hashIndex = DJ2B(key) % h->capacity;

	for (int i = 0; i < h->capacity; i++)
	{
		uint32_t currentIndex = (hashIndex + i) % h->capacity;
		uint8_t * entryAddress = (uint8_t*)h->memory + (currentIndex * (sizeof(HashEntry) + h->elementSize));
		HashEntry * entry = (HashEntry*)(entryAddress);

		if (entry->state == HASHMAP_OCCUPIED_ELEMENT && strcmp(entry->key, key) == 0)
		{
			return (int32_t)currentIndex;
		}
		else if (entry->state == HASHMAP_EMPTY_ELEMENT)
		{
			return -1;
		}
	}

	return -1;
}

bool HashMapGet(HashMap * h, const char * key, void * dest)
{
	int32_t index = __hashMapGetIndex(h, key);
	if (index == -1) { return false; }

    uint8_t * address = ((uint8_t*)h->memory) + (index * (sizeof(HashEntry) + h->elementSize) + sizeof(HashEntry));
    memcpy(dest, (void*)address, h->elementSize);

	return true;
}

bool HashmapRemove(HashMap * h, const char * key)
{
	int32_t index = __hashMapGetIndex(h, key);
	if (index == -1) { return false; }

    uint8_t * address = ((uint8_t*)h->memory) + (index * (sizeof(HashEntry) + h->elementSize));
	HashEntry * entry = (HashEntry*)(address);
	entry->state = HASHMAP_DELETED_ELEMENT;
	h->count--;
	return true;
}

#endif // HASHMAP_IMPLEMENTATION
