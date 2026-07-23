#include <stdint.h>
#include "nimble_task.h"

/*
 * nimble_port_init_task_stack()
 *
 * A task that has never run yet has no real saved context. This
 * function fabricates one, laid out so that the *first* time PendSV
 * (or SVC, for the very first task) restores this task, it can't tell
 * the difference between "a task that just got preempted" and "a task
 * that has never run" - both look like a normal saved frame.
 *
 * Memory layout built here, from stack_top downward (Cortex-M stacks
 * are full-descending), matching what PendSV_Handler expects to find:
 *
 *   [ stack_top ]
 *   xPSR                <-  hardware-stacked frame: what the CPU
 *   PC  (= fn)               would have pushed automatically on
 *   LR  (= task_exit_trap)   exception entry. Built by hand here so
 *   R12                      that when PendSV/SVC does its final
 *   R3                       `bx lr` exception return, the hardware's
 *   R2                       own automatic *unstacking* pops exactly
 *   R1                       this frame and lands the CPU at `fn`
 *   R0  (= arg)              with `arg` already in R0 per AAPCS.
 *   ------------------------
 *   R11                 <-  software-stacked frame: what PendSV's own
 *   R10                      `ldmia` instruction pops. Values are
 *   R9                       poisoned (0xN0N0N0N0-style) rather than
 *   R8                       zeroed so an accidental use-before-init
 *   R7                       is easy to spot in a debugger.
 *   R6
 *   R5
 *   R4
 *   [ sp returned here ]
 */

static void task_exit_trap(void)
{
    /* Every task in this kernel is expected to run forever (typically
     * an infinite loop around a blocking wait). Falling off the end of
     * a task function is a programming error, not a normal exit path -
     * nimbleRTOS deliberately has no implicit "task finished, clean up
     * silently" behavior. Trap loudly instead of running into
     * whatever bytes happen to follow in flash. */
    for (;;) {
        __asm volatile ("bkpt #0");
    }
}

uint32_t *nimble_port_init_task_stack(uint32_t *stack_top, nimble_task_fn_t fn, void *arg)
{
    uint32_t *sp = stack_top;

    /* Hardware-stacked frame (order the CPU itself uses on exception entry) */
    *(--sp) = 0x01000000UL;                 /* xPSR: T-bit set (Thumb state - mandatory on Cortex-M) */
    *(--sp) = (uint32_t)fn;                 /* PC: task entry point */
    *(--sp) = (uint32_t)task_exit_trap;     /* LR: caught if fn() ever returns */
    *(--sp) = 0x12121212UL;                 /* R12 - poison value, never meaningfully read before write */
    *(--sp) = 0x03030303UL;                 /* R3  */
    *(--sp) = 0x02020202UL;                 /* R2  */
    *(--sp) = 0x01010101UL;                 /* R1  */
    *(--sp) = (uint32_t)arg;                /* R0: task's argument, per AAPCS calling convention */

    /* Software-stacked frame (what PendSV_Handler's ldmia restores) */
    *(--sp) = 0x11111111UL;                 /* R11 */
    *(--sp) = 0x10101010UL;                 /* R10 */
    *(--sp) = 0x09090909UL;                 /* R9  */
    *(--sp) = 0x08080808UL;                 /* R8  */
    *(--sp) = 0x07070707UL;                 /* R7  */
    *(--sp) = 0x06060606UL;                 /* R6  */
    *(--sp) = 0x05050505UL;                 /* R5  */
    *(--sp) = 0x04040404UL;                 /* R4  */

    return sp;
}
