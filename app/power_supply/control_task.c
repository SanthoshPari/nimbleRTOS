#include "power_supply.h"
#include "nimble_task.h"

/*
 * ControlTask - runs the state machine and the control loop.
 *
 * IDLE -> SOFT_START -> RUNNING is a linear ramp; FAULT is reachable
 * from any state (ProtectionTask forces it directly via
 * ps_status_set_state(), bypassing this task entirely - see
 * protection_task.c) and is a terminal state until an external
 * command clears it (left as a documented extension point for
 * comms_task.c's CLI - not wired up in this demo to keep the fault
 * path simple to reason about).
 *
 * Blocks on g_adc_sample_queue rather than polling - this is the
 * mechanism that lets ControlTask do zero work between samples
 * instead of busy-waiting, and is exactly the ISR-to-task handoff
 * pattern documented for nimbleRTOS's queue in
 * kernel/include/nimble_queue.h.
 */

#define ADC_TO_MILLIVOLTS(raw)  ((float)(raw) * (16500.0f / 4095.0f)) /* 0-16.5V range, 12-bit ADC */
#define ADC_TO_MILLIAMPS(raw)   ((float)(raw) * (5000.0f  / 4095.0f)) /* 0-5A range, 12-bit ADC */

#define SOFT_START_RAMP_STEPS   50   /* number of control iterations to ramp through SOFT_START */

static ps_state_t   local_state = PS_STATE_IDLE;
static uint32_t      soft_start_step = 0;

static void run_state_machine(float voltage_mv, float current_ma)
{
    (void)voltage_mv;  /* not yet used: real closed-loop regulation (PS_STATE_RUNNING) is stubbed, see below */
    (void)current_ma;  /* not used by the ramp logic itself, only by fault detection (protection_task.c) */

    switch (local_state) {
        case PS_STATE_IDLE:
            /* Demo: transition to SOFT_START immediately. A real CLI
             * command handler (comms_task.c) would gate this on an
             * explicit "start" command instead. */
            local_state = PS_STATE_SOFT_START;
            soft_start_step = 0;
            ps_status_set_state(local_state);
            break;

        case PS_STATE_SOFT_START:
            soft_start_step++;
            if (soft_start_step >= SOFT_START_RAMP_STEPS) {
                local_state = PS_STATE_RUNNING;
                ps_status_set_state(local_state);
            }
            /* Actual duty-cycle ramping (PWM register writes) would
             * happen here, scaled by soft_start_step /
             * SOFT_START_RAMP_STEPS - stubbed out since this demo has
             * no real PWM peripheral wired up; the state machine
             * timing and task structure is the part under test. */
            break;

        case PS_STATE_RUNNING:
            /* Closed-loop regulation would happen here (a PI
             * controller adjusting PWM duty cycle against a voltage
             * setpoint using `voltage_mv`). Left as a stub for the
             * same reason as the SOFT_START ramp above. */
            break;

        case PS_STATE_FAULT:
            /* Terminal until something external clears it. ControlTask
             * deliberately does nothing here - it's not this task's
             * job to decide when a fault is safe to clear. */
            break;
    }

    /* If ProtectionTask forced a fault since our last iteration,
     * resync our local copy so we stop advancing the state machine.
     * (g_status is the source of truth; local_state is ControlTask's
     * own cached copy to avoid a mutex lock on every single iteration
     * for a value that only changes rarely.) */
    ps_status_t status;
    ps_status_get(&status);
    if (status.state == PS_STATE_FAULT) {
        local_state = PS_STATE_FAULT;
    }
}

void control_task_fn(void *arg)
{
    (void)arg;

    for (;;) {
        ps_adc_sample_t sample;
        nimble_status_t result = nimble_queue_receive(&g_adc_sample_queue, &sample, NIMBLE_WAIT_FOREVER);
        if (result != NIMBLE_OK) {
            continue; /* NIMBLE_WAIT_FOREVER never actually times out; defensive only */
        }

        float voltage_mv = ADC_TO_MILLIVOLTS(sample.voltage_raw);
        float current_ma = ADC_TO_MILLIAMPS(sample.current_raw);

        ps_status_set_measurement(voltage_mv, current_ma);
        run_state_machine(voltage_mv, current_ma);
    }
}
