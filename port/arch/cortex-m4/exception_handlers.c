#include <stdint.h>
#include "nimble_scheduler.h"
#include "nimble_port.h"

/* SCB->ICSR, and the PENDSVSET bit within it. Address is architectural
 * (fixed for every Cortex-M implementation), not STM32-specific -
 * that's why this lives in port/arch/cortex-m4/ rather than
 * port/stm32f4/. */
#define SCB_ICSR         (*(volatile uint32_t *)0xE000ED04UL)
#define ICSR_PENDSVSET   (1UL << 28)

/*
 * BASEPRI value used for kernel critical sections.
 *
 * This masks interrupts at numeric priority >= this value (i.e.
 * "equal or less urgent") while leaving anything strictly more urgent
 * free to run - notably hard-fault-class exceptions, which should
 * never be blocked by the kernel. This is the entire reason
 * BASEPRI-based masking is used here instead of a blunt global
 * cpsid i: a critical section that can still take a real fault
 * interrupt is a critical section that doesn't turn a bug into a
 * silent hang.
 *
 * The concrete value (5 << 4, i.e. priority group 5 assuming 4
 * implemented priority bits, matching STM32F4's NVIC) is set once at
 * boot alongside SysTick/PendSV priority configuration in
 * port/stm32f4/ (added in the port-layer section) - anything using
 * interrupt priorities numerically below this must never call into
 * the kernel API, and that boundary is documented there.
 */
#define NIMBLE_KERNEL_SYSCALL_BASEPRI (5U << 4)

void nimble_port_request_context_switch(void)
{
    SCB_ICSR = ICSR_PENDSVSET;
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb");
}

uint32_t nimble_port_enter_critical(void)
{
    uint32_t old_basepri;
    __asm volatile (
        "mrs %0, basepri                       \n"
        "msr basepri, %1                       \n"
        "isb                                    \n"
        "dsb                                    \n"
        : "=&r" (old_basepri)
        : "r" (NIMBLE_KERNEL_SYSCALL_BASEPRI)
        : "memory"
    );
    return old_basepri;
}

void nimble_port_exit_critical(uint32_t saved_state)
{
    __asm volatile (
        "msr basepri, %0 \n"
        "isb              \n"
        "dsb              \n"
        :
        : "r" (saved_state)
        : "memory"
    );
}

/*
 * SysTick fires at NIMBLE_TICK_RATE_HZ. All it does is delegate to the
 * portable scheduler tick handler - no scheduling policy lives here,
 * only the "which architectural interrupt drives the tick" wiring.
 */
void SysTick_Handler(void)
{
    uint32_t crit = nimble_port_enter_critical();
    nimble_scheduler_tick();
    nimble_port_exit_critical(crit);
}
