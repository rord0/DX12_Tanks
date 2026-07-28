#ifndef RING_QUEUE_HPP
#define RING_QUEUE_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdint.h>

typedef struct RingQueue_t
{
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t capacity;
	size_t elementSize;
    void * data;
} RingQueue;

RingQueue rQueueInit(size_t elementSize, uint32_t capacity, void * data);
void * rQueuePeek(RingQueue * q);
void   rQueuePush(RingQueue * q, const void * element);
void   rQueuePop(RingQueue * q, void * dest);

#define RING_QUEUE_PUSH(queue, element) rQueuePush(queue, (void*)(&element));

#endif // RING_QUEUE_HPP

#ifndef RING_QUEUE_IMPLEMENTATION

RingQueue rQueueInit(size_t elementSize, uint32_t capacity, void * data)
{
	RingQueue q = {0, 0, 0, capacity, elementSize, data};
	return q;
}

void rQueuePush(RingQueue * q, const void * element)
{
	if (q == NULL || element == NULL) { return; }
	if (q->count >= q->capacity) { return; }
	uint8_t * dest = ((uint8_t*)q->data) + (q->elementSize * q->tail);
	memcpy((void*)dest, element, q->elementSize);
	q->tail = (q->tail + 1) % q->capacity;
	q->count++;
}

void rQueuePop(RingQueue * q, void * dest)
{
	if (q == NULL || q->count <= 0) { return; }
	if (dest != NULL)
	{
		uint8_t * element = ((uint8_t*)q->data) + (q->elementSize*q->head);
		memcpy(dest, (void*)element, q->elementSize);
	}
	q->head = (q->head + 1) % q->capacity;
	q->count--;
}

void  * rQueuePeek(RingQueue * q)
{
	if (q == NULL || q->count <= 0) { return NULL; }
	return (void*)(((uint8_t*)q->data) + (q->elementSize*q->head));
}

#endif // RING_QUEUE_IMPLEMENTATION
