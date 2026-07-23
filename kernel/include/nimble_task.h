#ifndef NIMBLE_TASK_H
#define NIMBLE_TASK_H

#include "nimble_types.h"
#include "nimble_list.h"
#include "nimble_config.h"

typedef void (*nimble_task_fn_t)(void *arg);

/*
 * Task Control Block.
 *
 * IMPORTANT: `sp` MUST remain the first member. The context-switch
 * assembly in port/arch/cortex-m4/context_switch.S accesses it at
 * offset 0 from the TCB pointer without going through the C struct
 * layout — a classic RTOS trick, and a classic place for a padding
 * bug to bite you if a member is ever added above it. This constraint
 * is asserted at compile time in task.c via a static_assert.
 */
typedef struct nimble_tcb {
    uint32_t *sp;                 /* current stack pointer - OFFSET 0, DO NOT MOVE */

    nimble_list_node_t link;      /* intrusive link: ready list or delayed(timeout) list */
    nimble_list_node_t event_link; /* separate intrusive link: mutex/semaphore/queue wait list.
                                     * Kept as a distinct node (not reused from `link`) because a
                                     * timed wait on a sync primitive needs the task on BOTH its
                                     * wait list AND the delayed list simultaneously - one list
                                     * node can only ever be on one list at a time. FreeRTOS solves
                                     * the same problem with two list items per TCB for the same
                                     * reason; this is that same trade-off made explicit. */

    uint8_t  priority;            /* current effective priority (may be boosted) */
    uint8_t  base_priority;       /* priority as created; restored after priority inheritance ends */
    volatile nimble_task_state_t state;

    uint32_t *stack_base;         /* lowest address of the stack (for overflow checking) */
    uint32_t  stack_words;

    nimble_tick_t wake_tick;      /* tick count at which a delayed/blocked-with-timeout task wakes */
    volatile nimble_status_t last_wait_result; /* NIMBLE_OK if woken by a signal (mutex unlock,
                                     * semaphore give, queue send), NIMBLE_ERR_TIMEOUT if woken by
                                     * timeout expiry. state alone can't distinguish these - both
                                     * paths lead through READY -> RUNNING identically - so this
                                     * is set explicitly by whichever path actually wakes the task. */

    struct nimble_mutex *blocked_on_mutex; /* non-NULL if blocked on a mutex (drives priority inheritance) */

    char name[NIMBLE_MAX_TASK_NAME_LEN];
} nimble_tcb_t;

/*
 * Create a task. The task's stack is caller-provided (static allocation
 * only — nimbleRTOS has no heap in the kernel itself, by design: every
 * byte of RAM used by a task is visible in the linker map, not hidden
 * behind a runtime allocator failure mode).
 *
 * Returns NIMBLE_OK, or an error status. On success, *out_tcb is
 * populated and the task is placed in the READY state.
 */
nimble_status_t nimble_task_create(nimble_tcb_t *tcb,
                                    const char *name,
                                    nimble_task_fn_t fn,
                                    void *arg,
                                    uint32_t *stack_base,
                                    uint32_t stack_words,
                                    uint8_t priority);

/* Voluntarily yield the CPU to another ready task of equal-or-higher priority. */
void nimble_task_yield(void);

/* Block the calling task for `ticks` scheduler ticks. */
void nimble_task_delay(nimble_tick_t ticks);

/* Suspend / resume a task by TCB pointer (may be self for suspend). */
void nimble_task_suspend(nimble_tcb_t *tcb);
void nimble_task_resume(nimble_tcb_t *tcb);

/* Returns the TCB of the currently running task. */
nimble_tcb_t *nimble_task_self(void);

#if NIMBLE_ENABLE_STACK_CHECKING
/* Returns the number of unused words remaining on a task's stack,
 * measured via canary high-water-mark. Cheap enough to call from a
 * periodic diagnostics/logger task. */
uint32_t nimble_task_get_stack_high_water_mark(const nimble_tcb_t *tcb);
#endif

#endif /* NIMBLE_TASK_H */
