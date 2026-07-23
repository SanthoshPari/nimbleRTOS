#ifndef NIMBLE_TIMER_H
#define NIMBLE_TIMER_H

#include "nimble_types.h"
#include "nimble_list.h"

typedef void (*nimble_timer_callback_t)(void *arg);

typedef struct nimble_timer {
    nimble_list_node_t     link;         /* intrusive link into the sorted active-timer list */
    nimble_tick_t          period_ticks;
    nimble_tick_t          expiry_tick;  /* absolute tick count at which this timer next fires */
    bool                   is_periodic;
    bool                   is_active;
    nimble_timer_callback_t callback;
    void                   *arg;
    char                    name[16];
} nimble_timer_t;

/*
 * Software timers, callback-driven, resolution = one scheduler tick.
 *
 * Deliberately NOT implemented by running callbacks inline in
 * SysTick_Handler. Two reasons, detailed in docs/TIMERS.md:
 *   1. ISR context can't call blocking kernel APIs (mutex lock,
 *      queue send-with-wait, etc), which real timer callbacks often
 *      need to.
 *   2. A slow callback would extend every single SysTick ISR,
 *      directly adding jitter to the entire system's tick period -
 *      the one piece of timing every other subsystem depends on.
 *
 * Instead, a single dedicated "TimerTask" (started once via
 * nimble_timer_service_start()) sleeps until the next timer is due,
 * runs its callback in ordinary task context, and goes back to sleep.
 * Its priority controls the worst-case callback-firing jitter, and is
 * a normal, visible priority-assignment decision like any other task
 * in the system - not a hidden ISR-time cost.
 */

/* Must be called once before creating any timers. `stack`/`stack_words`
 * back the timer service task itself. */
nimble_status_t nimble_timer_service_start(uint8_t priority, uint32_t *stack, uint32_t stack_words);

void nimble_timer_init(nimble_timer_t *timer,
                        const char *name,
                        nimble_tick_t period_ticks,
                        bool is_periodic,
                        nimble_timer_callback_t callback,
                        void *arg);

/* (Re)starts the timer: expiry is set to now + period_ticks. Safe to
 * call on an already-running timer to restart it from now. */
nimble_status_t nimble_timer_start(nimble_timer_t *timer);

nimble_status_t nimble_timer_stop(nimble_timer_t *timer);

/* Changes the period and restarts the timer (expiry = now + new period). */
nimble_status_t nimble_timer_reset(nimble_timer_t *timer, nimble_tick_t new_period_ticks);

#endif /* NIMBLE_TIMER_H */
