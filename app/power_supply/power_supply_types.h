#ifndef POWER_SUPPLY_TYPES_H
#define POWER_SUPPLY_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Task priorities for the demo app. Priority 0 is reserved for
 * nimbleRTOS's idle task (see kernel/src/scheduler.c) - everything
 * here starts at 1.
 *
 * This ordering is the whole point of the demo: it's a realistic
 * multi-task system with a genuine priority hierarchy, not four tasks
 * all sitting at the same priority doing round-robin nothing.
 */
typedef enum {
    PS_PRIO_LOGGER      = 1, /* lowest: soaks up leftover CPU time, never time-critical */
    PS_PRIO_COMMS       = 2, /* UART CLI - can tolerate jitter, a human is on the other end */
    PS_PRIO_CONTROL     = 3, /* the control loop - must run every ADC sample, on time, every time */
    PS_PRIO_PROTECTION  = 4, /* highest: reacts to fault conditions, must preempt everything */
    PS_PRIO_TIMER_SVC   = 3  /* nimbleRTOS's own timer service task (Section 5) - level with
                                 control since logger's periodic timer isn't more urgent than
                                 the control loop, but shouldn't be starved by it either */
} ps_task_priority_t;

typedef enum {
    PS_STATE_IDLE = 0,
    PS_STATE_SOFT_START,
    PS_STATE_RUNNING,
    PS_STATE_FAULT
} ps_state_t;

static inline const char *ps_state_name(ps_state_t state)
{
    switch (state) {
        case PS_STATE_IDLE:       return "IDLE";
        case PS_STATE_SOFT_START: return "SOFT_START";
        case PS_STATE_RUNNING:    return "RUNNING";
        case PS_STATE_FAULT:      return "FAULT";
        default:                  return "UNKNOWN";
    }
}

/* One ADC sample, as handed from ADC_IRQHandler to ControlTask via a
 * nimbleRTOS queue (see isr_bridge.c). Raw counts, not yet scaled -
 * scaling happens in ControlTask so the ISR stays as short as possible. */
typedef struct {
    uint16_t voltage_raw;
    uint16_t current_raw;
} ps_adc_sample_t;

/* System-wide status snapshot, protected by g_status_mutex (see
 * power_supply.c) since ControlTask writes it and both CommsTask and
 * LoggerTask read it from different priority levels. */
typedef struct {
    ps_state_t state;
    float      voltage_mv;
    float      current_ma;
    uint32_t   fault_count;
} ps_status_t;

/* Fault thresholds - deliberately simple fixed limits for the demo.
 * A real design would source these from a config struct rather than
 * compile-time constants, called out here as the obvious next step
 * rather than silently hard-coded without comment. */
#define PS_OVERCURRENT_LIMIT_MA   3000.0f
#define PS_OVERVOLTAGE_LIMIT_MV   13000.0f

#endif /* POWER_SUPPLY_TYPES_H */
