#include "core.h"
#include "arena.h"

Arena ArenaAlloc(size_t size)
{
    Arena arena = {0};
    arena.data = PlatformAlloc(size);
    arena.size = size;
    arena.pos = 0;
    return arena;
}
void * ArenaPush(Arena * arena, size_t size)
{
    void * memory = NULL;
    if (arena->pos + size <= arena->size)
    {
        memory = (u8*)arena->data + arena->pos;
        arena->pos += size;
    }
    return memory;
};

void ArenaClear(Arena * arena)
{
	arena->pos = 0;
}
