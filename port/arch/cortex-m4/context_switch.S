/*
 * nimbleRTOS context switch — Cortex-M4, no hardware FPU lazy-stacking.
 *
 * Two exception handlers live here:
 *
 *   PendSV_Handler   - the actual context switch, runs on every task
 *                       switch after the scheduler is started.
 *   SVC_Handler       - used exactly once, at boot, to start the very
 *                       first task (see nimble_port_start_first_task
 *                       below for why this needs a real exception
 *                       context rather than a plain function call).
 *
 * Full narrative explanation, register-frame diagram, and the EXC_RETURN
 * encoding this file relies on are in docs/CONTEXT_SWITCH.md — read
 * that alongside this file, not just this file alone.
 */

    .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

    .extern nimble_current_tcb
    .extern nimble_next_tcb
    .extern nimble_scheduler_select_next

    .global PendSV_Handler
    .global SVC_Handler
    .global nimble_port_start_first_task

/* ------------------------------------------------------------------ */
    .section .text.PendSV_Handler, "ax", %progbits
    .type PendSV_Handler, %function
PendSV_Handler:
    /* r0 = the *outgoing* task's process stack pointer. The hardware
     * has already auto-stacked {r0-r3, r12, lr, pc, xpsr} onto this
     * stack on the way into whatever ISR pended us; we only need to
     * add the registers the hardware doesn't save (r4-r11). */
    mrs     r0, psp
    isb

    ldr     r3, =nimble_current_tcb
    ldr     r2, [r3]               /* r2 = nimble_current_tcb */
    cbz     r2, pendsv_skip_save   /* NULL only during the very first switch, which never
                                     * reaches here (see nimble_port_start_first_task) - kept
                                     * as a defensive guard, not load-bearing logic. */

    stmdb   r0!, {r4-r11}          /* push software-saved regs onto the OUTGOING task's stack */
    str     r0, [r2]               /* nimble_current_tcb->sp = r0  (sp is TCB offset 0) */

pendsv_skip_save:
    /* Ask the portable scheduler which TCB runs next. This is a real
     * C function call, so LR (which currently holds this exception's
     * EXC_RETURN value - we need it again after the call) must be
     * preserved across it explicitly. */
    push    {r3, lr}
    bl      nimble_scheduler_select_next
    pop     {r3, lr}

    ldr     r1, =nimble_next_tcb
    ldr     r2, [r1]               /* r2 = nimble_next_tcb, chosen by select_next() */
    str     r2, [r3]               /* nimble_current_tcb = nimble_next_tcb (r3 still &nimble_current_tcb) */

    ldr     r0, [r2]               /* r0 = incoming task's saved sp */
    ldmia   r0!, {r4-r11}          /* pop its software-saved regs */
    msr     psp, r0
    isb

    bx      lr                     /* exception return: hardware pops {r0-r3,r12,lr,pc,xpsr}
                                     * from the (now-updated) PSP and resumes the incoming
                                     * task exactly where it left off - or, the first time,
                                     * exactly where nimble_port_init_task_stack() pointed it. */
    .size PendSV_Handler, . - PendSV_Handler

/* ------------------------------------------------------------------ */
    .section .text.SVC_Handler, "ax", %progbits
    .type SVC_Handler, %function
SVC_Handler:
    /* Reached exactly once, via the `svc 0` in nimble_port_start_first_task.
     * Loads the first task's context the same way PendSV's second half
     * does, then forges an EXC_RETURN that switches the CPU onto PSP /
     * Thread mode for good. */
    ldr     r3, =nimble_current_tcb
    ldr     r2, [r3]               /* r2 = the task nimble_scheduler_start() already chose */
    ldr     r1, [r2]               /* r1 = its (pre-built, fake-exception-frame) sp */
    ldmia   r1!, {r4-r11}
    msr     psp, r1
    isb

    mov     r0, #0
    msr     basepri, r0            /* make sure critical sections aren't left engaged at boot */

    ldr     r0, =0xFFFFFFFD        /* EXC_RETURN: return to Thread mode, use PSP, no FPU state */
    bx      r0
    .size SVC_Handler, . - SVC_Handler

/* ------------------------------------------------------------------ */
    .section .text.nimble_port_start_first_task, "ax", %progbits
    .type nimble_port_start_first_task, %function
nimble_port_start_first_task:
    /* Why this can't just be "restore registers and jump" in plain C:
     * an EXC_RETURN value (anything with bits[27:24] == 0xF) only has
     * its special meaning to the CPU when it's branched to from
     * *inside* an active exception (Handler mode) - IPSR must be
     * non-zero. We're called from ordinary Thread-mode startup code
     * (main() -> nimble_scheduler_start()), so we can't forge that
     * return directly; we deliberately trigger a real SVC exception
     * and do the actual work inside SVC_Handler, above, where the
     * CPU really is in Handler mode. */
    cpsie   i
    svc     0
    /* Never reached. */
    .size nimble_port_start_first_task, . - nimble_port_start_first_task
