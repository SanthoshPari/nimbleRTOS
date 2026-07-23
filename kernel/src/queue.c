#include <string.h>
#include <stddef.h>
#include "nimble_queue.h"
#include "nimble_scheduler.h"
#include "nimble_port.h"

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

void nimble_queue_init(nimble_queue_t *queue, uint8_t *buffer, uint32_t item_size, uint32_t capacity)
{
    queue->buffer = buffer;
    queue->item_size = item_size;
    queue->capacity = capacity;
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    nimble_list_init(&queue->receivers_waiting);
    nimble_list_init(&queue->senders_waiting);
}

static void copy_in(nimble_queue_t *queue, const void *item)
{
    memcpy(queue->buffer + (queue->tail * queue->item_size), item, queue->item_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
}

static void copy_out(nimble_queue_t *queue, void *out_item)
{
    memcpy(out_item, queue->buffer + (queue->head * queue->item_size), queue->item_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
}

nimble_status_t nimble_queue_send(nimble_queue_t *queue, const void *item, nimble_tick_t timeout_ticks)
{
    uint32_t crit = nimble_port_enter_critical();

    if (queue->count < queue->capacity) {
        copy_in(queue, item);
        nimble_tcb_t *woken = nimble_scheduler_wake_waiter(&queue->receivers_waiting);
        nimble_port_exit_critical(crit);
        if (woken != NULL) {
            nimble_port_request_context_switch();
        }
        return NIMBLE_OK;
    }

    if (timeout_ticks == NIMBLE_NO_WAIT) {
        nimble_port_exit_critical(crit);
        return NIMBLE_ERR_FULL;
    }

    nimble_tcb_t *self = nimble_current_tcb;
    insert_waiter_priority_sorted(&queue->senders_waiting, self);
    nimble_scheduler_prepare_block(self, timeout_ticks);
    nimble_port_exit_critical(crit);

    nimble_port_request_context_switch();

    /* Resumed: either a receiver freed up space and we should retry
     * the copy, or we timed out. Re-check under the lock rather than
     * assuming success, since a third task could theoretically have
     * refilled the queue in between on a more complex system. */
    crit = nimble_port_enter_critical();
    nimble_list_remove(&self->event_link);
    nimble_status_t result;
    if (self->last_wait_result == NIMBLE_ERR_TIMEOUT) {
        result = NIMBLE_ERR_TIMEOUT;
    } else if (queue->count < queue->capacity) {
        copy_in(queue, item);
        result = NIMBLE_OK;
    } else {
        result = NIMBLE_ERR_FULL; /* woken but lost the race - documented edge case */
    }
    nimble_port_exit_critical(crit);
    return result;
}

nimble_status_t nimble_queue_receive(nimble_queue_t *queue, void *out_item, nimble_tick_t timeout_ticks)
{
    uint32_t crit = nimble_port_enter_critical();

    if (queue->count > 0) {
        copy_out(queue, out_item);
        nimble_tcb_t *woken = nimble_scheduler_wake_waiter(&queue->senders_waiting);
        nimble_port_exit_critical(crit);
        if (woken != NULL) {
            nimble_port_request_context_switch();
        }
        return NIMBLE_OK;
    }

    if (timeout_ticks == NIMBLE_NO_WAIT) {
        nimble_port_exit_critical(crit);
        return NIMBLE_ERR_EMPTY;
    }

    nimble_tcb_t *self = nimble_current_tcb;
    insert_waiter_priority_sorted(&queue->receivers_waiting, self);
    nimble_scheduler_prepare_block(self, timeout_ticks);
    nimble_port_exit_critical(crit);

    nimble_port_request_context_switch();

    crit = nimble_port_enter_critical();
    nimble_list_remove(&self->event_link);
    nimble_status_t result;
    if (self->last_wait_result == NIMBLE_ERR_TIMEOUT) {
        result = NIMBLE_ERR_TIMEOUT;
    } else if (queue->count > 0) {
        copy_out(queue, out_item);
        result = NIMBLE_OK;
    } else {
        result = NIMBLE_ERR_EMPTY;
    }
    nimble_port_exit_critical(crit);
    return result;
}

nimble_status_t nimble_queue_send_from_isr(nimble_queue_t *queue, const void *item, bool *needs_yield)
{
    /* No critical-section entry: called from ISR context, where the
     * NVIC has already masked interrupts at this priority or below -
     * see the same rationale in nimble_semaphore_give_from_isr(). */
    *needs_yield = false;

    if (queue->count >= queue->capacity) {
        return NIMBLE_ERR_FULL;
    }

    copy_in(queue, item);
    nimble_tcb_t *woken = nimble_scheduler_wake_waiter(&queue->receivers_waiting);
    if (woken != NULL) {
        *needs_yield = true;
    }
    return NIMBLE_OK;
}
