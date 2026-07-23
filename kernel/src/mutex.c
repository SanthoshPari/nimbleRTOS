#include <stddef.h>
#include "nimble_mutex.h"
#include "nimble_scheduler.h"
#include "nimble_port.h"

void nimble_mutex_init(nimble_mutex_t *mutex)
{
    mutex->owner = NULL;
    mutex->lock_count = 0;
    nimble_list_init(&mutex->waiters);
}

/* Insert `tcb` into the mutex's waiter list ordered by priority,
 * highest first - so the list head is always "who to wake next"
 * without a search at wake time. Waiter counts on a single mutex are
 * small in this class of firmware (a handful of tasks, not hundreds),
 * so a linear insertion scan is the right trade-off: simple, correct,
 * and cheap at this scale - the same reasoning as the delayed-task
 * list in docs/SCHEDULER.md. */
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
    /* Insert immediately before `node` (which is either the first
     * lower-priority waiter, or the list head if tcb is lowest/only). */
    tcb->event_link.next = node;
    tcb->event_link.prev = node->prev;
    node->prev->next = &tcb->event_link;
    node->prev = &tcb->event_link;
}

nimble_status_t nimble_mutex_lock(nimble_mutex_t *mutex, nimble_tick_t timeout_ticks)
{
    uint32_t crit = nimble_port_enter_critical();

    nimble_tcb_t *self = nimble_current_tcb;

    if (mutex->owner == NULL) {
        mutex->owner = self;
        mutex->lock_count = 1;
        nimble_port_exit_critical(crit);
        return NIMBLE_OK;
    }

    if (mutex->owner == self) {
        /* Recursive lock: same task already holds it. */
        mutex->lock_count++;
        nimble_port_exit_critical(crit);
        return NIMBLE_OK;
    }

    if (timeout_ticks == NIMBLE_NO_WAIT) {
        nimble_port_exit_critical(crit);
        return NIMBLE_ERR_TIMEOUT;
    }

    /*
     * Priority inheritance: if we (the blocking task) are higher
     * priority than the current owner, boost the owner's *effective*
     * priority to ours for as long as it holds the mutex. This is the
     * whole mechanism - one comparison, one assignment - the rest of
     * this file is bookkeeping around it. base_priority is preserved
     * separately so nimble_mutex_unlock() knows what to restore.
     *
     * Note this only handles a single level of inheritance directly;
     * chained inheritance (owner is itself blocked on a second mutex
     * held by a third, lower-priority task) is NOT propagated
     * transitively here. Real-world firmware at this scale almost
     * never nests mutexes deeply enough to hit that case, and it's
     * called out explicitly in docs/PRIORITY_INHERITANCE.md as a
     * known, documented limitation rather than something the code
     * pretends to handle.
     */
    if (self->priority > mutex->owner->priority) {
        mutex->owner->priority = self->priority;
    }

    insert_waiter_priority_sorted(&mutex->waiters, self);
    nimble_scheduler_prepare_block(self, timeout_ticks);

    nimble_port_exit_critical(crit);
    nimble_port_request_context_switch();

    /* Execution resumes here once woken - either because
     * nimble_mutex_unlock() handed us the mutex, or because we timed
     * out. Figure out which. */
    crit = nimble_port_enter_critical();
    nimble_status_t result;
    if (mutex->owner == self) {
        result = NIMBLE_OK;
    } else {
        /* Timed out: make sure we're fully detached from the waiter
         * list (nimble_scheduler_tick() already did this via
         * event_link on the timeout path, but this is defensive in
         * case of a signal/timeout race). */
        nimble_list_remove(&self->event_link);
        result = NIMBLE_ERR_TIMEOUT;
    }
    nimble_port_exit_critical(crit);
    return result;
}

nimble_status_t nimble_mutex_unlock(nimble_mutex_t *mutex)
{
    uint32_t crit = nimble_port_enter_critical();

    if (mutex->owner != nimble_current_tcb) {
        nimble_port_exit_critical(crit);
        return NIMBLE_ERR_NOT_OWNER;
    }

    mutex->lock_count--;
    if (mutex->lock_count > 0) {
        /* Still held (recursive lock not fully unwound yet). */
        nimble_port_exit_critical(crit);
        return NIMBLE_OK;
    }

    /* Restore our own priority in case it was boosted by inheritance. */
    mutex->owner->priority = mutex->owner->base_priority;
    mutex->owner = NULL;

    nimble_tcb_t *woken = nimble_scheduler_wake_waiter(&mutex->waiters);
    if (woken != NULL) {
        mutex->owner = woken;
        mutex->lock_count = 1;
    }

    nimble_port_exit_critical(crit);

    if (woken != NULL) {
        /* A (possibly higher-priority) task just became ready; give
         * the scheduler a chance to switch to it immediately rather
         * than waiting for the next tick. */
        nimble_port_request_context_switch();
    }
    return NIMBLE_OK;
}
