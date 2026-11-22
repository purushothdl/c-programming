#ifndef RING_H
#define RING_H

#include <stdint.h>

/*
 * Single-producer, single-consumer byte ring. Pure logic, no hardware.
 *
 * head is written only by the producer and tail only by the consumer, so no
 * lock is needed. Both count freely (never wrapped by hand) and the slot is
 * index & RING_MASK, so RING_SIZE must be a power of two.
 */
#define RING_SIZE 64u            /* power of two: index & MASK wraps correctly */
#define RING_MASK (RING_SIZE - 1u)

/* Keeps the compiler from moving memory accesses across the index update. */
#define COMPILER_BARRIER() __asm volatile ("" ::: "memory")

typedef struct {
    uint8_t buf[RING_SIZE];
    volatile uint32_t head;      /* written only by the producer */
    volatile uint32_t tail;      /* written only by the consumer */
    volatile uint32_t drops;     /* producer-side count of rejected bytes */
} ring_t;

int ring_empty(const ring_t *r);
int ring_full(const ring_t *r);
int ring_push(ring_t *r, uint8_t byte);      /* 0 if full (and counts a drop) */
uint8_t ring_pop(ring_t *r);                 /* caller checks !ring_empty first */

#endif
