#include "power_supply.h"
#include "nimble_task.h"

/*
 * ProtectionTask - highest priority in the system.
 *
 * Blocked on g_fault_semaphore almost all the time; the moment
 * ADC_IRQHandler (isr_bridge.c) detects an overcurrent/overvoltage
 * condition and gives the semaphore, this task preempts *everything*
 * else immediately - including ControlTask - because it sits above
 * it in the priority scheme (power_supply_types.h). This is the
 * concrete demonstration of priority-based preemption described in
 * docs/SCHEDULER.md: it does not wait for a tick, it does not wait
 * for ControlTask to finish what it's doing.
 *
 * Deliberately does NOT go through the mutex-based inheritance path -
 * it doesn't need to, since it's already the highest priority task in
 * the system, so it can never itself be the victim of priority
 * inversion. It's the *reason* the mutex needs inheritance in the
 * first place (see docs/PRIORITY_INHERITANCE.md's worked example,
 * which uses this exact task as the "who's being starved" case).
 */

/* Stub for the actual output-disable action (e.g. driving a
 * gate-driver enable pin low). Left as a named, obvious extension
 * point rather than inline register-poking, since the real
 * implementation is entirely board-specific. */
static void ps_disable_output_hw(void)
{
    /* TODO(hardware): drive PWM_ENABLE / gate-driver EN pin low here. */
}

void protection_task_fn(void *arg)
{
    (void)arg;

    for (;;) {
        nimble_status_t result = nimble_semaphore_take(&g_fault_semaphore, NIMBLE_WAIT_FOREVER);
        if (result != NIMBLE_OK) {
            continue; /* NIMBLE_WAIT_FOREVER never actually times out; defensive only */
        }

        ps_disable_output_hw();
        ps_status_note_fault(); /* forces g_status.state = PS_STATE_FAULT, bumps fault_count */

        /* A real implementation would also log the fault reason/
         * timestamp for post-mortem analysis (see the black-box
         * logger design this project's docs reference as a natural
         * follow-on - LoggerTask, below, is the natural place to
         * extend for that). */
    }
}
