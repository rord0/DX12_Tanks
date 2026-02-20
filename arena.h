#ifndef ARENA_H
#define ARENA_H

typedef struct
{
    size_t pos;
    size_t size;
    void * data;
} Arena;

Arena ArenaAlloc(size_t size);
void * ArenaPush(Arena * arena, size_t size);
void ArenaClear(Arena * arena);

#endif // ARENA_H
