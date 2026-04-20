#ifndef ARENA_HPP
#define ARENA_HPP

#include <cstddef>
typedef struct Arena_t
{
    size_t pos;
    size_t size;
    void * data;
} Arena;

typedef struct ScratchArena_t
{
	Arena * arena;
	size_t mark;

	ScratchArena_t(Arena *arena) : arena(arena), mark(arena->pos) { }
    ~ScratchArena_t() { arena->pos = mark; }

	ScratchArena_t(const ScratchArena_t&)            = delete;
    ScratchArena_t& operator=(const ScratchArena_t&) = delete;
} ScratchArena;

Arena ArenaInit(void * data, size_t size);
void * ArenaPush(Arena * arena, size_t size);
size_t ArenaGetRemainingSize(Arena * arena);
void ArenaClear(Arena * arena);

#endif // ARENA_HPP

#ifdef ARENA_IMPLEMENTATION

Arena ArenaInit(void * data, size_t size)
{
	Arena a = {0, size, data};
	return a;
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
}

size_t ArenaGetRemainingSize(Arena * arena)
{
    return arena->size - arena->pos;
}

void ArenaClear(Arena * arena)
{
	if (arena) { arena->pos = 0; }
}

#endif // ARENA_IMPLEMENTATION
