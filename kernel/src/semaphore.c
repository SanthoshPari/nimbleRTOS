#include <stddef.h>
#include "nimble_semaphore.h"
#include "nimble_scheduler.h"
#include "nimble_port.h"

void nimble_semaphore_init(nimble_semaphore_t *sem, uint32_t initial_count, uint32_t max_count)
{
    sem->count = initial_count;
    sem->max_count = max_count;
    nimble_list_init(&sem->waiters);
}

/* Same priority-sorted insertion policy as the mutex waiter list -
 * intentionally duplicated rather than shared through a helper header
 * at this size; if a third caller needs it, that's the point at which
 * to factor it out (see docs/ROADMAP.md notes on avoiding premature
 * abstraction). */
static void insert_waiter_priority_sorted(nimble_list_t *waiters, nimble_tcb_t *tcb)
{
    nimble_list_node_t *node = waiters->next;
    while (node != waiters) {
        nimble_tcb_t *other = NIMBLE_CONTAINER_OF(node, nimble_tcb_t, event_link);
        if (tcb->priority > other->priority) {
            break;
        }
        node = node->next;
    }
    tcb->event_link.next = node;
    tcb->event_link.prev = node->prev;
    node->prev->next = &tcb->event_link;
    node->prev = &tcb->event_link;
}

nimble_status_t nimble_semaphore_take(nimble_semaphore_t *sem, nimble_tick_t timeout_ticks)
{
    uint32_t crit = nimble_port_enter_critical();

    if (sem->count > 0) {
        sem->count--;
        nimble_port_exit_critical(crit);
        return NIMBLE_OK;
    }

    if (timeout_ticks == NIMBLE_NO_WAIT) {
        nimble_port_exit_critical(crit);
        return NIMBLE_ERR_TIMEOUT;
    }

    nimble_tcb_t *self = nimble_current_tcb;
    insert_waiter_priority_sorted(&sem->waiters, self);
    nimble_scheduler_prepare_block(self, timeout_ticks);
    nimble_port_exit_critical(crit);

    nimble_port_request_context_switch();

    /* Resumed either because give() consumed our wait (count was
     * decremented on our behalf, see nimble_semaphore_give below) or
     * because we timed out and are still sitting at count-not-taken. */
    crit = nimble_port_enter_critical();
    nimble_list_remove(&self->event_link); /* no-op if give() already detached us */
    nimble_status_t result = self->last_wait_result;
    nimble_port_exit_critical(crit);

    return result;
}

nimble_status_t nimble_semaphore_give(nimble_semaphore_t *sem)
{
    uint32_t crit = nimble_port_enter_critical();

    nimble_tcb_t *woken = nimble_scheduler_wake_waiter(&sem->waiters);
    if (woken != NULL) {
        /* Hand the unit straight to the waiter we're waking rather
         * than incrementing count and letting it decrement again -
         * avoids a spurious window where a third, unrelated task
         * could steal the unit between the two operations. */
        nimble_port_exit_critical(crit);
        nimble_port_request_context_switch();
        return NIMBLE_OK;
    }

    if (sem->count < sem->max_count) {
        sem->count++;
    }
    nimble_port_exit_critical(crit);
    return NIMBLE_OK;
}

nimble_status_t nimble_semaphore_give_from_isr(nimble_semaphore_t *sem, bool *needs_yield)
{
    /* No nimble_port_enter_critical() here: this function IS the ISR
     * context (or called from one) - by the time it runs, the CPU has
     * already masked interrupts at or below its own priority via the
     * NVIC, which is a stronger guarantee than BASEPRI critical
     * sections need to provide on top of that. Calling
     * nimble_port_request_context_switch() is safe and correct from
     * ISR context; it only pends PendSV, it never switches stacks
     * inline. */
    *needs_yield = false;

    nimble_tcb_t *woken = nimble_scheduler_wake_waiter(&sem->waiters);
    if (woken != NULL) {
        *needs_yield = true;
        return NIMBLE_OK;
    }

    if (sem->count < sem->max_count) {
        sem->count++;
    }
    return NIMBLE_OK;
}
