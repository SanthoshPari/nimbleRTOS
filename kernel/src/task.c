#include <string.h>
#include <stddef.h>
#include "nimble_task.h"
#include "nimble_scheduler.h"
#include "nimble_port.h"

/* The asm context switch (port/arch/cortex-m4/context_switch.S) reads
 * the stack pointer straight out of offset 0 of the TCB, without going
 * through the C struct layout. This assertion is the safety net if
 * anyone (including future-me) ever adds a member above `sp`. */
_Static_assert(offsetof(nimble_tcb_t, sp) == 0,
               "nimble_tcb_t.sp must remain the first member - the asm context switch depends on this offset");

#if NIMBLE_ENABLE_STACK_CHECKING
#define NIMBLE_STACK_CANARY 0xA5A5A5A5U
#endif

nimble_status_t nimble_task_create(nimble_tcb_t *tcb,
                                    const char *name,
                                    nimble_task_fn_t fn,
                                    void *arg,
                                    uint32_t *stack_base,
                                    uint32_t stack_words,
                                    uint8_t priority)
{
    if (tcb == NULL || fn == NULL || stack_base == NULL) {
        return NIMBLE_ERR_INVALID_ARG;
    }
    if (stack_words < NIMBLE_MIN_STACK_WORDS) {
        return NIMBLE_ERR_INVALID_ARG;
    }
    if (priority >= NIMBLE_MAX_PRIORITIES) {
        return NIMBLE_ERR_INVALID_ARG;
    }

    memset(tcb, 0, sizeof(*tcb)); /* also zero-initializes last_wait_result to NIMBLE_OK */
    nimble_list_init(&tcb->link);
    nimble_list_init(&tcb->event_link);

#if NIMBLE_ENABLE_STACK_CHECKING
    for (uint32_t i = 0; i < stack_words; i++) {
        stack_base[i] = NIMBLE_STACK_CANARY;
    }
#endif

    /* Cortex-M stacks grow down (full-descending); the initial SP
     * starts one-past the top of the provided buffer. */
    uint32_t *stack_top = stack_base + stack_words;
    tcb->sp = nimble_port_init_task_stack(stack_top, fn, arg);
    tcb->stack_base = stack_base;
    tcb->stack_words = stack_words;
    tcb->priority = priority;
    tcb->base_priority = priority;
    tcb->blocked_on_mutex = NULL;

    size_t name_len = 0;
    if (name != NULL) {
        while (name[name_len] != '\0' && name_len < (NIMBLE_MAX_TASK_NAME_LEN - 1)) {
            name_len++;
        }
        memcpy(tcb->name, name, name_len);
    }
    tcb->name[name_len] = '\0';

    uint32_t crit = nimble_port_enter_critical();
    nimble_scheduler_add_ready(tcb);
    nimble_port_exit_critical(crit);

    return NIMBLE_OK;
}

void nimble_task_yield(void)
{
    nimble_port_request_context_switch();
}

void nimble_task_delay(nimble_tick_t ticks)
{
    if (ticks == 0) {
        return;
    }

    uint32_t crit = nimble_port_enter_critical();
    nimble_tcb_t *self = nimble_current_tcb;
    nimble_tick_t wake_at = nimble_scheduler_get_tick_count() + ticks;
    nimble_scheduler_block_current_delay(self, wake_at);
    nimble_port_exit_critical(crit);

    /* Switch happens outside the critical section: PendSV runs at the
     * lowest exception priority and will naturally wait until we're
     * done here anyway, but requesting it while BASEPRI is still
     * raised would just delay the pend, not the switch itself - no
     * functional difference, kept outside for clarity. */
    nimble_port_request_context_switch();
}

void nimble_task_suspend(nimble_tcb_t *tcb)
{
    uint32_t crit = nimble_port_enter_critical();
    if (tcb->state == NIMBLE_TASK_READY || tcb->state == NIMBLE_TASK_RUNNING) {
        bool suspending_self = (tcb == nimble_current_tcb);
        if (tcb->state == NIMBLE_TASK_READY) {
            nimble_scheduler_remove(tcb);
        }
        tcb->state = NIMBLE_TASK_SUSPENDED;
        nimble_port_exit_critical(crit);
        if (suspending_self) {
            nimble_port_request_context_switch();
        }
        return;
    }
    nimble_port_exit_critical(crit);
}

void nimble_task_resume(nimble_tcb_t *tcb)
{
    uint32_t crit = nimble_port_enter_critical();
    if (tcb->state == NIMBLE_TASK_SUSPENDED) {
        nimble_scheduler_add_ready(tcb);
    }
    nimble_port_exit_critical(crit);
}

nimble_tcb_t *nimble_task_self(void)
{
    return nimble_current_tcb;
}

#if NIMBLE_ENABLE_STACK_CHECKING
uint32_t nimble_task_get_stack_high_water_mark(const nimble_tcb_t *tcb)
{
    uint32_t i = 0;
    while (i < tcb->stack_words && tcb->stack_base[i] == NIMBLE_STACK_CANARY) {
        i++;
    }
    return i; /* words never touched since creation */
}
#endif
