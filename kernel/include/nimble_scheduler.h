#ifndef NIMBLE_SCHEDULER_H
#define NIMBLE_SCHEDULER_H

#include "nimble_task.h"

/*
 * Scheduler core.
 *
 * Design: one ready-list per priority level (NIMBLE_MAX_PRIORITIES of
 * them), plus a single uint32_t bitmap where bit N is set iff ready
 * list N is non-empty. Finding the highest-priority ready task is then
 * "find the highest set bit in a 32-bit word", done in O(1) with a
 * single CLZ (count-leading-zeros) instruction on Cortex-M4 rather
 * than scanning 32 lists. This is the same technique FreeRTOS's
 * generic scheduler uses; implemented independently here. See
 * docs/SCHEDULER.md for the full derivation and a worked example.
 */

/* Called once at startup, before any task exists. */
void nimble_scheduler_init(void);

/* Register a task (already in READY state) with the scheduler. */
void nimble_scheduler_add_ready(nimble_tcb_t *tcb);

/* Remove a task from its current ready/blocked list (state transition only). */
void nimble_scheduler_remove(nimble_tcb_t *tcb);

/* Move the calling task out of its ready list and onto the delayed
 * (timed-wait) list until wake_tick. Used by nimble_task_delay() and,
 * later, by mutex/semaphore/queue timeout paths. */
void nimble_scheduler_block_current_delay(nimble_tcb_t *tcb, nimble_tick_t wake_tick);

/*
 * Generic blocking used by sync primitives (mutex/semaphore/queue).
 * The caller has already linked `tcb` onto its own wait list via
 * tcb->event_link (typically priority-sorted so the head is the
 * highest-priority waiter). This function does the *scheduler-side*
 * half: removes tcb from the ready structures, marks it BLOCKED, and
 * - unless timeout_ticks is NIMBLE_WAIT_FOREVER - also links it onto
 * the delayed list (via tcb->link) so nimble_scheduler_tick() will
 * wake it on timeout even if it's never signaled.
 */
void nimble_scheduler_prepare_block(nimble_tcb_t *tcb, nimble_tick_t timeout_ticks);

/*
 * Pops and readies the highest-priority waiter (list head) from a
 * sync-primitive wait list linked via tcb->event_link. Also detaches
 * it from the delayed list if it had a pending timeout. Returns NULL
 * if the wait list is empty. This is the "signal" side of every
 * blocking primitive - mutex unlock, semaphore give, queue send.
 */
nimble_tcb_t *nimble_scheduler_wake_waiter(nimble_list_t *wait_list);

/*
 * Core decision function: returns a pointer to the TCB that should run
 * next. Called from PendSV_Handler with interrupts effectively locked
 * out of the scheduler's data structures (BASEPRI masking — see
 * port/arch/cortex-m4/exception_handlers.c). Never call this from
 * application code directly.
 */
nimble_tcb_t *nimble_scheduler_select_next(void);

/*
 * Called from the SysTick handler every tick. Responsible for:
 *   - waking any delayed tasks whose wake_tick has arrived
 *   - rotating the ready list of the *currently running* priority level
 *     for round-robin time-slicing (only affects tasks at the same
 *     priority as the current task — this is the mechanism, and the
 *     distinction, documented in docs/SCHEDULER.md's "time-slicing vs
 *     preemption" section)
 *   - requesting a context switch (pending PendSV) if needed
 */
void nimble_scheduler_tick(void);

/* Starts the scheduler. Does not return. */
void nimble_scheduler_start(void);

/* Current tick count since nimble_scheduler_start(). */
nimble_tick_t nimble_scheduler_get_tick_count(void);

/* Pointer to the task currently selected as running (used by the asm
 * context-switch code and by nimble_task_self()). */
extern nimble_tcb_t *nimble_current_tcb;
/* Pointer to the task selected to run *next*, set by
 * nimble_scheduler_select_next() and consumed by PendSV_Handler. */
extern nimble_tcb_t *nimble_next_tcb;

#endif /* NIMBLE_SCHEDULER_H */
