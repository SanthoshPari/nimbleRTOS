#include <stdio.h>
#include "power_supply.h"
#include "uart_bridge.h"
#include "nimble_task.h"
#include "nimble_config.h"

/*
 * LoggerTask - lowest priority in the system, deliberately.
 *
 * This is the task that demonstrates time-slicing is a non-event here:
 * at NIMBLE_MAX_PRIORITIES = 32 possible levels with only one task
 * (LoggerTask itself) at priority PS_PRIO_LOGGER, the round-robin
 * requeue logic in nimble_scheduler_select_next() never has anything
 * to rotate it against - it simply runs whenever nothing higher
 * priority is ready, exactly the "soak up idle time" role a logger
 * should have. See docs/SCHEDULER.md's "time-slicing vs preemption"
 * section for the general statement of this.
 */

#define LOGGER_PERIOD_TICKS (NIMBLE_TICK_RATE_HZ) /* once per second */

void logger_task_fn(void *arg)
{
    (void)arg;

    for (;;) {
        ps_status_t status;
        ps_status_get(&status);

        char buf[96];
        snprintf(buf, sizeof(buf), "[log] state=%s v=%.0fmV i=%.0fmA faults=%lu\r\n",
                 ps_state_name(status.state), (double)status.voltage_mv,
                 (double)status.current_ma, (unsigned long)status.fault_count);
        ps_uart_puts(buf);

        nimble_task_delay(LOGGER_PERIOD_TICKS);
    }
}
