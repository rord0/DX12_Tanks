#ifndef ARRAY_H
#define ARRAY_H

typedef struct
{
    size_t elementSize;
    size_t capacity;
    size_t count;
    void * elements;
} Array;

bool ArrayPush(Array * array, void * element);
Array ArrayInit(size_t elementSize, size_t capacity, void * elements);

#endif // ARRAY_H