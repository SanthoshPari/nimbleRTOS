#include "nimble_scheduler.h"
#include "nimble_port.h"
#include "nimble_config.h"

nimble_tcb_t *nimble_current_tcb = NULL;
nimble_tcb_t *nimble_next_tcb = NULL;

static nimble_list_t ready_lists[NIMBLE_MAX_PRIORITIES];
static nimble_list_t delayed_list;
static uint32_t ready_bitmap = 0;
static volatile nimble_tick_t g_tick_count = 0;

/* Idle task: priority 0, always ready, guarantees the bitmap is never
 * all-zero so nimble_scheduler_select_next() never has to special-case
 * "nothing is ready." Every real RTOS needs an idle task for exactly
 * this reason. */
#define IDLE_STACK_WORDS 128
static uint32_t idle_stack[IDLE_STACK_WORDS];
static nimble_tcb_t idle_tcb;

static void idle_task_fn(void *arg)
{
    (void)arg;
    for (;;) {
        /* Real port hook would be __WFI() here to drop power draw
         * between ticks; left as a documented extension point since
         * low-power/tickless idle is explicitly out of scope for this
         * project (see docs/ARCHITECTURE.md "Non-goals"). */
        __asm volatile ("nop");
    }
}

static inline uint8_t highest_set_bit(uint32_t bitmap)
{
    /* 31 - clz(x) gives the index of the most-significant set bit.
     * Maps directly onto "highest priority number = highest priority"
     * with zero branching — the entire point of using a bitmap instead
     * of scanning 32 lists. Documented with a worked example in
     * docs/SCHEDULER.md. */
    return (uint8_t)(31 - __builtin_clz(bitmap));
}

void nimble_scheduler_init(void)
{
    for (uint32_t i = 0; i < NIMBLE_MAX_PRIORITIES; i++) {
        nimble_list_init(&ready_lists[i]);
    }
    nimble_list_init(&delayed_list);
    ready_bitmap = 0;
    g_tick_count = 0;
    nimble_current_tcb = NULL;
    nimble_next_tcb = NULL;

    (void)nimble_task_create(&idle_tcb, "idle", idle_task_fn, NULL,
                              idle_stack, IDLE_STACK_WORDS, 0);
}

void nimble_scheduler_add_ready(nimble_tcb_t *tcb)
{
    tcb->state = NIMBLE_TASK_READY;
    nimble_list_insert_tail(&ready_lists[tcb->priority], &tcb->link);
    ready_bitmap |= (1u << tcb->priority);
}

void nimble_scheduler_remove(nimble_tcb_t *tcb)
{
    uint8_t prio = tcb->priority;
    nimble_list_remove(&tcb->link);
    if (nimble_list_is_empty(&ready_lists[prio])) {
        ready_bitmap &= ~(1u << prio);
    }
}

void nimble_scheduler_block_current_delay(nimble_tcb_t *tcb, nimble_tick_t wake_tick)
{
    nimble_scheduler_remove(tcb);
    tcb->wake_tick = wake_tick;
    tcb->state = NIMBLE_TASK_BLOCKED;
    nimble_list_insert_tail(&delayed_list, &tcb->link);
}

nimble_tcb_t *nimble_scheduler_select_next(void)
{
    /* If the task that was running is still runnable (i.e. wasn't just
     * blocked/suspended/deleted by whoever triggered this switch),
     * requeue it at the tail of its own priority's ready list. This
     * single line is *all* of round-robin time-slicing: it only ever
     * rotates a task among peers at its own priority, and only when a
     * switch decision is already being made — it can never cause a
     * lower-priority task to preempt a higher one. */
    if (nimble_current_tcb != NULL && nimble_current_tcb->state == NIMBLE_TASK_RUNNING) {
        nimble_scheduler_add_ready(nimble_current_tcb);
    }

    uint8_t prio = highest_set_bit(ready_bitmap);
    nimble_list_node_t *node = ready_lists[prio].next;
    nimble_tcb_t *next = NIMBLE_CONTAINER_OF(node, nimble_tcb_t, link);

    nimble_scheduler_remove(next);
    next->state = NIMBLE_TASK_RUNNING;

    nimble_next_tcb = next;
    return next;
}

void nimble_scheduler_tick(void)
{
    g_tick_count++;

    /* Linear scan of the delayed list. For the scale of task counts
     * this kernel targets (single-digit to low-dozens of tasks in a
     * firmware image, not hundreds), this is simpler and more
     * auditable than a sorted delta list, and costs microseconds.
     * FreeRTOS uses a sorted delta list specifically because it has to
     * scale further; documented here as a known, deliberate trade-off
     * rather than an oversight — see docs/SCHEDULER.md. */
    nimble_list_node_t *node = delayed_list.next;
    while (node != &delayed_list) {
        nimble_list_node_t *next_node = node->next;
        nimble_tcb_t *tcb = NIMBLE_CONTAINER_OF(node, nimble_tcb_t, link);
        if ((int32_t)(g_tick_count - tcb->wake_tick) >= 0) {
            nimble_list_remove(&tcb->link);
            nimble_scheduler_add_ready(tcb);
        }
        node = next_node;
    }

    nimble_port_request_context_switch();
}

void nimble_scheduler_start(void)
{
    nimble_next_tcb = nimble_scheduler_select_next();
    nimble_current_tcb = nimble_next_tcb;
    nimble_port_start_first_task(); /* does not return */
}

nimble_tick_t nimble_scheduler_get_tick_count(void)
{
    return g_tick_count;
}
