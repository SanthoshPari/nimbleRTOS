#ifndef NIMBLE_PORT_H
#define NIMBLE_PORT_H

#include "nimble_types.h"
#include "nimble_task.h"

/*
 * Port interface.
 *
 * The portable kernel (kernel/src/*.c) never touches a register or an
 * NVIC bit directly. Every architecture-specific action goes through
 * this interface, implemented once per target in port/arch/<arch>/.
 * This is the same separation FreeRTOS draws between "kernel" and
 * "portable layer" — here it's five functions instead of a sprawling
 * portmacro.h, but the boundary is the same idea and is what makes
 * porting to a second architecture later a bounded task instead of a
 * rewrite.
 */

/* Build the initial stack frame for a not-yet-run task so the first
 * context switch into it behaves exactly like returning from an
 * exception into fn(arg). Returns the stack pointer value to store in
 * tcb->sp. Implemented in port/arch/cortex-m4/context_switch.c. */
uint32_t *nimble_port_init_task_stack(uint32_t *stack_top, nimble_task_fn_t fn, void *arg);

/* Request a context switch at the next safe opportunity. On Cortex-M
 * this sets the PendSV pending bit; the actual switch happens later,
 * at PendSV's low fixed priority, never inside the caller's context. */
void nimble_port_request_context_switch(void);

/* Enter / exit a kernel critical section. Cortex-M raises BASEPRI to
 * mask everything at or below configMAX_SYSCALL priority rather than
 * a blunt global interrupt disable, so higher-priority hard-fault-class
 * interrupts still run even while the kernel holds this section. */
uint32_t nimble_port_enter_critical(void);
void nimble_port_exit_critical(uint32_t saved_state);

/* Starts the first task and never returns. Implemented in assembly
 * (has to manipulate CONTROL/PSP directly, not expressible in C). */
void nimble_port_start_first_task(void);

#endif /* NIMBLE_PORT_H */
