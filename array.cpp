#include "core.h"

#include "array.h"

bool ArrayPush(Array * array, void * element)
{
    if (array->count < array->capacity)
    {
        u8 * dest = (u8*)array->elements + (array->elementSize * array->count);
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
