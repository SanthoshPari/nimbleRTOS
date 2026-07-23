#ifndef NIMBLE_MUTEX_H
#define NIMBLE_MUTEX_H

#include "nimble_types.h"
#include "nimble_task.h"
#include "nimble_list.h"

/*
 * Priority-inheritance mutex.
 *
 * The problem this solves (and the reason a plain binary semaphore is
 * NOT a substitute for a mutex, despite looking similar on paper):
 * unbounded priority inversion. Classic scenario, using this project's
 * own task set as the example:
 *
 *   1. ProtectionTask (high priority) is BLOCKED waiting on a mutex
 *      currently held by LoggerTask (low priority).
 *   2. CommsTask (medium priority) is ready and CPU-bound.
 *   3. Without inheritance: CommsTask preempts LoggerTask (lower
 *      priority) and runs indefinitely. LoggerTask never gets the CPU
 *      back to finish its critical section and release the mutex.
 *      ProtectionTask - the HIGHEST priority task in the system - is
 *      now blocked for an UNBOUNDED time by a MEDIUM priority task it
 *      has no direct relationship with. That's priority inversion.
 *
 * The fix implemented here: when a higher-priority task blocks on a
 * mutex, the current holder's priority is temporarily boosted to match
 * the highest-priority waiter, for exactly as long as it holds the
 * mutex. In the scenario above, LoggerTask would run at
 * ProtectionTask's priority the moment ProtectionTask blocks on it -
 * CommsTask can no longer preempt it - bounding the inversion to "as
 * long as the critical section legitimately takes," not "however long
 * an unrelated medium-priority task feels like running."
 *
 * Full walkthrough with a timing diagram: docs/PRIORITY_INHERITANCE.md
 */

typedef struct nimble_mutex {
    nimble_tcb_t *owner;          /* NULL if unlocked */
    uint32_t      lock_count;     /* supports recursive locking by the same owner */
    nimble_list_t waiters;        /* tasks blocked on nimble_mutex_lock(), priority-ordered */
} nimble_mutex_t;

void nimble_mutex_init(nimble_mutex_t *mutex);

/* Blocks up to `timeout_ticks` (or NIMBLE_WAIT_FOREVER / NIMBLE_NO_WAIT)
 * waiting to acquire the mutex. Recursive: the owner may lock again
 * without deadlocking itself: each lock() must be matched by an
 * unlock(). Triggers priority inheritance if the caller's priority
 * exceeds the current owner's. */
nimble_status_t nimble_mutex_lock(nimble_mutex_t *mutex, nimble_tick_t timeout_ticks);

/* Releases one lock level. If lock_count drops to 0 and the owner's
 * priority had been boosted, restores it to base_priority and wakes
 * the highest-priority waiter (if any). Returns NIMBLE_ERR_NOT_OWNER
 * if called by a task that doesn't hold the mutex. */
nimble_status_t nimble_mutex_unlock(nimble_mutex_t *mutex);

#endif /* NIMBLE_MUTEX_H */
