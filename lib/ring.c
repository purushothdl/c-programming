#include "ring.h"

int ring_empty(const ring_t *r)
{
    return r->head == r->tail;
}

int ring_full(const ring_t *r)
{
    return (uint32_t)(r->head - r->tail) >= RING_SIZE;
}

int ring_push(ring_t *r, uint8_t byte)
{
    if (ring_full(r)) {
        r->drops++;
        return 0;
    }
    r->buf[r->head & RING_MASK] = byte;   /* 1. write the slot */
    COMPILER_BARRIER();
    r->head = r->head + 1u;               /* 2. publish it */
    return 1;
}

uint8_t ring_pop(ring_t *r)
{
    uint8_t byte = r->buf[r->tail & RING_MASK];
    COMPILER_BARRIER();
    r->tail = r->tail + 1u;               /* release the slot */
    return byte;
}
