#ifndef ARRAY_H
#define ARRAY_H

#include <cstddef>
#include <cstring>
#include <stdint.h>

typedef struct
{
    size_t elementSize;
    size_t capacity;
    size_t count;
    void * elements;
} Array;

Array ArrayInit(size_t elementSize, size_t capacity, void * elements);
bool ArrayPush(Array * array, const void * element);
void ArrayRemove(Array * array, size_t index);
bool ArrayFull(const Array * array);

#endif // ARRAY_H

#ifdef ARRAY_IMPLEMENTATION

bool ArrayPush(Array * array, const void * element)
{
	if (array == NULL || element == NULL) { return false; }

    if (array->count < array->capacity)
    {
        uint8_t * dest = (uint8_t*)array->elements + (array->elementSize * array->count);
        memcpy(dest, element, array->elementSize);
        array->count++;
        return true;
    }
    else
    {
        return false;
    }
}

Array ArrayInit(size_t elementSize, size_t capacity, void * elements)
{
    Array array = {};
    array.capacity = capacity;
    array.elementSize = elementSize;
    array.elements = elements;
    array.count = 0;
    return array;
}

void ArrayRemove(Array * array, size_t index)
{
	if (!array || index >= array->count) { return; }

	if (index != array->count - 1)
	{
		size_t elementsToShift = array->count - index - 1;
        uint8_t * dest = (uint8_t*)array->elements + (array->elementSize * index);
        uint8_t * src  = (uint8_t*)array->elements + (array->elementSize * (index + 1));
		memmove(dest, src, array->elementSize * elementsToShift);
	}
	array->count--;
}

bool ArrayFull(const Array * array)
{
	if (array == NULL) { return true; }
	return array->count >= array->capacity;
}

#endif // ARRAY_IMPLEMENTATION
