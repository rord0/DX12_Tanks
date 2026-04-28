#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include "../core.h"

// Allocate size amount of bytes on the heap.
void * PlatformAlloc(size_t size);

// Free memory allocated with PlatformAlloc
void PlatformFree(void * memory);

PLATFORM_LOAD_FILE(DEBUG_PlatformReadEntireFile);
PLATFORM_FREE_FILE(DEBUG_PlatformFreeFileMemory);
PLATFORM_LOAD_TEXTURE(PlatformLoadTexture);

#endif // PLATFORM_HPP
