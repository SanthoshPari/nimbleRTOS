#ifndef NIMBLE_QUEUE_H
#define NIMBLE_QUEUE_H

#include "nimble_types.h"
#include "nimble_task.h"
#include "nimble_list.h"

/*
 * Fixed-capacity, fixed-item-size blocking queue. Backed by a
 * caller-provided static buffer - like everything else in this
 * kernel, no heap allocation, so the memory cost of every queue in a
 * firmware image is visible in the linker map, not hidden behind a
 * runtime allocator that can fail at 2am in the field.
 *
 * Two separate wait lists: tasks blocked in nimble_queue_send()
 * because the queue is full, and tasks blocked in
 * nimble_queue_receive() because it's empty. Both are priority-sorted
 * for the same reason the mutex/semaphore waiter lists are.
 */

typedef struct nimble_queue {
    uint8_t      *buffer;
    uint32_t      item_size;
    uint32_t      capacity;   /* number of items, not bytes */
    uint32_t      count;
    uint32_t      head;       /* next slot to read */
    uint32_t      tail;       /* next slot to write */
    nimble_list_t receivers_waiting;
    nimble_list_t senders_waiting;
} nimble_queue_t;

/* `buffer` must be at least item_size * capacity bytes, statically or
 * stack-allocated by the caller with a lifetime that outlives the queue. */
void nimble_queue_init(nimble_queue_t *queue, uint8_t *buffer, uint32_t item_size, uint32_t capacity);

/* Copies item_size bytes from `item` into the queue. Blocks (task
 * context only) up to timeout_ticks if the queue is full. */
nimble_status_t nimble_queue_send(nimble_queue_t *queue, const void *item, nimble_tick_t timeout_ticks);

/* Copies the oldest item out into `out_item`. Blocks up to
 * timeout_ticks if the queue is empty. */
nimble_status_t nimble_queue_receive(nimble_queue_t *queue, void *out_item, nimble_tick_t timeout_ticks);

/*
 * ISR-safe send - never blocks; if the queue is full, fails immediately
 * with NIMBLE_ERR_FULL rather than waiting. This is the primitive an
 * ADC conversion-complete ISR uses to hand a sample to a task without
 * ever risking blocking inside interrupt context. Sets *needs_yield
 * the same way nimble_semaphore_give_from_isr() does - the ISR must
 * call nimble_port_request_context_switch() itself afterward if set.
 */
nimble_status_t nimble_queue_send_from_isr(nimble_queue_t *queue, const void *item, bool *needs_yield);

#endif /* NIMBLE_QUEUE_H */
