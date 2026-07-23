#ifndef NIMBLE_SEMAPHORE_H
#define NIMBLE_SEMAPHORE_H

#include "nimble_types.h"
#include "nimble_task.h"
#include "nimble_list.h"

/*
 * Counting semaphore.
 *
 * Deliberately has NO ownership concept and NO priority inheritance -
 * unlike nimble_mutex_t. This isn't an oversight: a semaphore is
 * fundamentally a signaling/counting primitive (an ISR can "give" one
 * it never "took"), so there's no well-defined "owner" to boost the
 * priority of. Using a semaphore where a mutex is semantically what's
 * needed - i.e. for mutual exclusion around a shared resource - is a
 * classic real-world bug precisely because it silently drops priority
 * inheritance. This distinction is worth being able to state cleanly
 * in an interview: mutex = ownership + inheritance, for exclusion;
 * semaphore = counting/signaling, no ownership, for synchronization.
 */

typedef struct nimble_semaphore {
    uint32_t      count;
    uint32_t      max_count;
    nimble_list_t waiters; /* priority-ordered, same scheme as nimble_mutex_t.waiters */
} nimble_semaphore_t;

void nimble_semaphore_init(nimble_semaphore_t *sem, uint32_t initial_count, uint32_t max_count);

/* Blocks (task context only) until count > 0 or timeout. Decrements count on success. */
nimble_status_t nimble_semaphore_take(nimble_semaphore_t *sem, nimble_tick_t timeout_ticks);

/* Task-context give: increments count (bounded by max_count) and wakes
 * the highest-priority waiter, if any. */
nimble_status_t nimble_semaphore_give(nimble_semaphore_t *sem);

/*
 * ISR-context give. Never blocks, never calls the scheduler directly -
 * ISRs must not context-switch mid-interrupt. Instead it sets
 * *needs_yield = true if giving this semaphore woke a task, and the
 * ISR is responsible for calling nimble_port_request_context_switch()
 * itself once it's finished (exactly the same
 * "portYIELD_FROM_ISR() after the send" pattern used with FreeRTOS
 * stream buffers - see docs/ISR_SIGNALING.md).
 */
nimble_status_t nimble_semaphore_give_from_isr(nimble_semaphore_t *sem, bool *needs_yield);

#endif /* NIMBLE_SEMAPHORE_H */
