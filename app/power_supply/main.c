#include "power_supply.h"
#include "power_supply_types.h"
#include "nimble_scheduler.h"
#include "nimble_task.h"
#include "nimble_timer.h"

/*
 * Wires together the whole demo: shared state, four application
 * tasks at four distinct priorities, nimbleRTOS's own timer service
 * task, STM32F4 clock/NVIC/SysTick bring-up, then hands control to
 * the scheduler - which does not return.
 *
 * Every task stack here is a plain static array: no heap, anywhere,
 * in this entire application - consistent with the kernel's own
 * no-heap design (see kernel/include/nimble_config.h).
 */

extern void control_task_fn(void *arg);
extern void protection_task_fn(void *arg);
extern void comms_task_fn(void *arg);
extern void logger_task_fn(void *arg);

extern void nimble_stm32f4_system_init(void);

#define STACK_WORDS(bytes) ((bytes) / sizeof(uint32_t))

static uint32_t control_stack[STACK_WORDS(1024)];
static uint32_t protection_stack[STACK_WORDS(512)];
static uint32_t comms_stack[STACK_WORDS(1024)];
static uint32_t logger_stack[STACK_WORDS(768)];
static uint32_t timer_svc_stack[STACK_WORDS(512)];

static nimble_tcb_t control_tcb;
static nimble_tcb_t protection_tcb;
static nimble_tcb_t comms_tcb;
static nimble_tcb_t logger_tcb;

int main(void)
{
    nimble_stm32f4_system_init(); /* NVIC priorities + SysTick - see port/stm32f4/system_stm32f4xx.c */

    nimble_scheduler_init(); /* also creates the idle task, priority 0 */

    ps_shared_init(); /* status mutex, fault semaphore, ADC sample queue */

    (void)nimble_task_create(&protection_tcb, "protection", protection_task_fn, NULL,
                              protection_stack, STACK_WORDS(512), PS_PRIO_PROTECTION);

    (void)nimble_task_create(&control_tcb, "control", control_task_fn, NULL,
                              control_stack, STACK_WORDS(1024), PS_PRIO_CONTROL);

    (void)nimble_task_create(&comms_tcb, "comms", comms_task_fn, NULL,
                              comms_stack, STACK_WORDS(1024), PS_PRIO_COMMS);

    (void)nimble_task_create(&logger_tcb, "logger", logger_task_fn, NULL,
                              logger_stack, STACK_WORDS(768), PS_PRIO_LOGGER);

    (void)nimble_timer_service_start(PS_PRIO_TIMER_SVC, timer_svc_stack, STACK_WORDS(512));

    nimble_scheduler_start(); /* does not return */

    for (;;) {
        /* Unreachable - trapped here defensively in case
         * nimble_scheduler_start() ever returns, which would itself
         * indicate a serious kernel bug. */
    }
}
