#include "ring_buffer.hpp"

RingBuffer * RingBufferCreate(size_t size)
{
    assert((size & (size - 1)) == 0);  // must be power of two
	RingBuffer * out = (RingBuffer*)PlatformAlloc(sizeof(RingBuffer) + size);
	out->head = 0;
	out->tail = 0;
	out->cap = size;
	out->data = (u8*)out + sizeof(RingBuffer);
	return out;
}

void * RingBufferPush(RingBuffer * rb, size_t size)
{
	u32 total_size = sizeof(RingFrame) + RING_FRAME_ALIGN(size);
	u32 used = rb->head - rb->tail;
	u32 free = rb->cap - used;
	u32 offset = rb->head & (rb->cap - 1); // Write offset

	// Check if there's enough space.
	if (free < total_size) return NULL;

	// Check if we need to wrap.
	u32 space_to_end = rb->cap - offset;   // Remaining space to end of buffer.
	if (space_to_end < total_size)
	{
		// Not enough space after wrapping.
		if (free < (total_size + space_to_end)) { return NULL; }

		// Add skip marker to go to start of buffer.
		RingFrame * skip = (RingFrame*)(rb->data + offset);
		skip->size = FRAME_SKIP;
		rb->head += space_to_end;
		offset = 0;
	}

	RingFrame *f = (RingFrame*)(rb->data + offset);
    f->size = size;
    rb->head += total_size;
	rb->count++;
	assert(rb->head - rb->tail <= rb->cap);
    return f + 1;
}

RingEntry RingBufferPop(RingBuffer * rb)
{
	RingEntry entry = {0};
	if (rb->head == rb->tail) { return entry; }

    u32 offset = rb->tail & (rb->cap - 1);
    RingFrame * f = (RingFrame*)(rb->data + offset);

	if (f->size == FRAME_SKIP)
	{
		u32 space_to_end = rb->cap - offset;
		rb->tail += space_to_end;
		offset = 0;
		f = (RingFrame*)(rb->data);
	}

	entry.size = f->size;
	entry.data = (void*)(f+1);
    rb->tail += sizeof(RingFrame) + RING_FRAME_ALIGN(f->size);
	rb->count--;
	assert(rb->tail <= rb->head);
	return entry;
}

void RingBufferFree(RingBuffer * rb)
{
	memset((void*)rb, 0, sizeof(RingBuffer));
	PlatformFree(rb);
}
