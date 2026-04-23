#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstddef>
#include "core.h"

const u32 FRAME_SKIP = 0xFFFFFFFF;
#define RING_FRAME_ALIGN(size) (((size) + 3) & ~3)

typedef struct RingFrame_t
{
	u32 size;
} RingFrame;

typedef struct RingEntry_t
{
	u32 size;
	void * data;
} RingEntry;

typedef struct RingBuffer_t
{
	size_t cap;
	u32 head;
	u32 tail;
	u32 count;
	u8 * data;
} RingBuffer;

RingBuffer * RingBufferCreate(size_t size);
void * RingBufferPush(RingBuffer * rb, size_t size);
RingEntry RingBufferPop(RingBuffer * rb);

#endif // RING_BUFFER_H
