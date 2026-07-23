#include <string.h>
#include <stddef.h>
#include "nimble_timer.h"
#include "nimble_task.h"
#include "nimble_semaphore.h"
#include "nimble_scheduler.h"
#include "nimble_port.h"

/* Active timers, sorted ascending by expiry_tick - the timer task only
 * ever needs to look at the head to know "how long until the next
 * thing fires." Same pattern, same justification (small N, simplicity
 * over asymptotic optimality) as the delayed-task list in
 * docs/SCHEDULER.md. */
static nimble_list_t active_timers;

/* Given to whenever a timer is started/stopped/reset in a way that
 * could change what the soonest deadline is, so the timer task can
 * wake up and recompute its sleep instead of over-sleeping past a
 * newly-added, sooner-than-current-head timer. */
static nimble_semaphore_t timer_wake_sem;

static nimble_tcb_t timer_task_tcb;

static void insert_sorted_by_expiry(nimble_timer_t *timer)
{
    nimble_list_node_t *node = active_timers.next;
    while (node != &active_timers) {
        nimble_timer_t *other = NIMBLE_CONTAINER_OF(node, nimble_timer_t, link);
        /* Tick-wraparound-safe comparison, same trick as the scheduler's
         * delayed-list expiry check: (int32_t)(a - b) >= 0 means
         * "a is at or after b" even across a uint32_t wraparound. */
        if ((int32_t)(timer->expiry_tick - other->expiry_tick) < 0) {
            break;
        }
        node = node->next;
    }
    timer->link.next = node;
    timer->link.prev = node->prev;
    node->prev->next = &timer->link;
    node->prev = &timer->link;
}

static void timer_task_fn(void *arg)
{
    (void)arg;

    for (;;) {
        uint32_t crit = nimble_port_enter_critical();
        nimble_tick_t wait_ticks;
        if (nimble_list_is_empty(&active_timers)) {
            wait_ticks = NIMBLE_WAIT_FOREVER; /* woken only by nimble_timer_start() giving the semaphore */
        } else {
            nimble_timer_t *head = NIMBLE_CONTAINER_OF(active_timers.next, nimble_timer_t, link);
            nimble_tick_t now = nimble_scheduler_get_tick_count();
            int32_t delta = (int32_t)(head->expiry_tick - now);
            wait_ticks = (delta > 0) ? (nimble_tick_t)delta : NIMBLE_NO_WAIT;
        }
        nimble_port_exit_critical(crit);

        /* Either we time out (a timer is now due, or was already due),
         * or timer_wake_sem is given (list changed, recompute). Both
         * cases fall through to the same expiry-processing loop below,
         * which is a no-op if nothing is actually due yet. */
        (void)nimble_semaphore_take(&timer_wake_sem, wait_ticks);

        for (;;) {
            crit = nimble_port_enter_critical();
            if (nimble_list_is_empty(&active_timers)) {
                nimble_port_exit_critical(crit);
                break;
            }
            nimble_timer_t *head = NIMBLE_CONTAINER_OF(active_timers.next, nimble_timer_t, link);
            nimble_tick_t now = nimble_scheduler_get_tick_count();
            if ((int32_t)(head->expiry_tick - now) > 0) {
                nimble_port_exit_critical(crit);
                break; /* soonest timer isn't due yet */
            }

            nimble_list_remove(&head->link);
            nimble_timer_callback_t callback = head->callback;
            void *cb_arg = head->arg;

            if (head->is_periodic) {
                /* Reschedule from the *previous* expiry, not "now", so
                 * a periodic timer doesn't drift just because the
                 * timer task itself got delayed slightly - same
                 * technique real timer services use to avoid
                 * long-run skew. If we've fallen more than one full
                 * period behind (timer task was starved), catch up to
                 * "now + period" instead of firing a burst of
                 * already-missed expiries back to back. */
                nimble_tick_t next_expiry = head->expiry_tick + head->period_ticks;
                if ((int32_t)(next_expiry - now) < 0) {
                    next_expiry = now + head->period_ticks;
                }
                head->expiry_tick = next_expiry;
                insert_sorted_by_expiry(head);
            } else {
                head->is_active = false;
            }
            nimble_port_exit_critical(crit);

            /* Callback runs here, in ordinary task context, with no
             * kernel lock held - it's free to call blocking APIs. */
            if (callback != NULL) {
                callback(cb_arg);
            }
        }
    }
}

nimble_status_t nimble_timer_service_start(uint8_t priority, uint32_t *stack, uint32_t stack_words)
{
    nimble_list_init(&active_timers);
    nimble_semaphore_init(&timer_wake_sem, 0, 1);

    return nimble_task_create(&timer_task_tcb, "timer_svc", timer_task_fn, NULL,
                               stack, stack_words, priority);
}

void nimble_timer_init(nimble_timer_t *timer,
                        const char *name,
                        nimble_tick_t period_ticks,
                        bool is_periodic,
                        nimble_timer_callback_t callback,
                        void *arg)
{
    memset(timer, 0, sizeof(*timer));
    nimble_list_init(&timer->link);
    timer->period_ticks = period_ticks;
    timer->is_periodic = is_periodic;
    timer->callback = callback;
    timer->arg = arg;
    timer->is_active = false;

    size_t len = 0;
    if (name != NULL) {
        while (name[len] != '\0' && len < sizeof(timer->name) - 1) {
            len++;
        }
        memcpy(timer->name, name, len);
    }
    timer->name[len] = '\0';
}

nimble_status_t nimble_timer_start(nimble_timer_t *timer)
{
    uint32_t crit = nimble_port_enter_critical();

    if (timer->is_active) {
        nimble_list_remove(&timer->link);
    }
    timer->expiry_tick = nimble_scheduler_get_tick_count() + timer->period_ticks;
    timer->is_active = true;
    insert_sorted_by_expiry(timer);

    bool became_new_head = (active_timers.next == &timer->link);
    nimble_port_exit_critical(crit);

    if (became_new_head) {
        /* The soonest deadline in the system just changed - wake the
         * timer task so it recomputes its sleep instead of oversleeping
         * past this timer. Safe to call from task context; if this is
         * ever needed from ISR context too, use
         * nimble_semaphore_give_from_isr() instead - see
         * docs/TIMERS.md. */
        (void)nimble_semaphore_give(&timer_wake_sem);
    }
    return NIMBLE_OK;
}

nimble_status_t nimble_timer_stop(nimble_timer_t *timer)
{
    uint32_t crit = nimble_port_enter_critical();
    if (timer->is_active) {
        nimble_list_remove(&timer->link);
        timer->is_active = false;
    }
    nimble_port_exit_critical(crit);
    return NIMBLE_OK;
}

nimble_status_t nimble_timer_reset(nimble_timer_t *timer, nimble_tick_t new_period_ticks)
{
    uint32_t crit = nimble_port_enter_critical();
    timer->period_ticks = new_period_ticks;
    nimble_port_exit_critical(crit);
    return nimble_timer_start(timer); /* restarts from now with the new period */
}
