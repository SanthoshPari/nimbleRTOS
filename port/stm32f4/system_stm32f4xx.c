#include "stm32f4_regs.h"
#include "nimble_config.h"

/*
 * System bring-up for STM32F407.
 *
 * Clock tree: deliberately left at its post-reset default (16 MHz
 * internal HSI oscillator, no PLL). Bringing up the full PLL chain
 * for 168 MHz is a well-understood, mechanical peripheral-init task
 * that adds real lines of register-poking code but zero additional
 * RTOS-internals content - it's explicitly out of scope for what this
 * project exists to demonstrate (see docs/ARCHITECTURE.md). Swapping
 * in a real clock-tree init here later is a self-contained addition;
 * everything below only needs to know the resulting core clock
 * frequency, passed in explicitly rather than assumed.
 */

#define HSI_CLOCK_HZ 16000000UL

/*
 * NVIC priority scheme for this port.
 *
 * STM32F4 implements 4 priority bits (upper nibble of each 8-bit
 * priority field) - so priority values step by 0x10, giving 16 levels
 * (0x00 = highest urgency ... 0xF0 = lowest).
 *
 * - PendSV and SysTick are both pinned to the LOWEST priority (0xF0).
 *   This is required, not just a nice-to-have: PendSV must never
 *   preempt anything, and SysTick's only job is to delegate to the
 *   scheduler and request a (deferred) switch - neither should ever
 *   block a real peripheral IRQ from running.
 * - NIMBLE_KERNEL_SYSCALL_BASEPRI (defined in
 *   port/arch/cortex-m4/exception_handlers.c as 5<<4 = 0x50) is the
 *   line: any interrupt configured at priority 0x50 or lower urgency
 *   (i.e. numerically >= 0x50) is masked while the kernel holds a
 *   critical section, and may safely call kernel APIs
 *   (nimble_semaphore_give_from_isr() etc). Anything configured
 *   *above* that line (0x00-0x40) is reserved for genuinely
 *   safety-critical interrupts - e.g. a hardware analog comparator
 *   tied directly to an overcurrent shutdown - that must preempt even
 *   the kernel itself and therefore must NEVER call a kernel API.
 *   This split is a real design decision an application integrating
 *   this port has to make consciously for each peripheral IRQ it uses,
 *   not something this file can decide generically.
 */
#define NVIC_PRIO_PENDSV_SYSTICK   0xF0U

void nimble_stm32f4_nvic_priority_init(void)
{
    /* SHPR3: bits[23:16] = PendSV priority, bits[31:24] = SysTick priority. */
    uint32_t shpr3 = SCB_SHPR3;
    shpr3 &= ~(0xFFUL << 16);
    shpr3 &= ~(0xFFUL << 24);
    shpr3 |= (NVIC_PRIO_PENDSV_SYSTICK << 16);
    shpr3 |= (NVIC_PRIO_PENDSV_SYSTICK << 24);
    SCB_SHPR3 = shpr3;
}

/* Configures SysTick to fire at NIMBLE_TICK_RATE_HZ, driven from the
 * core clock. Returns nothing to check because a bad reload value
 * (core_clock_hz too small for the requested tick rate) is a
 * compile/config-time mistake, not a runtime one - caught immediately
 * by the tick rate being visibly wrong, not worth an error-handling
 * path for what's effectively a constant computed once at boot. */
void nimble_stm32f4_systick_init(uint32_t core_clock_hz)
{
    uint32_t reload = (core_clock_hz / NIMBLE_TICK_RATE_HZ) - 1UL;
    STK_LOAD = reload;
    STK_VAL = 0;
    STK_CTRL = STK_CTRL_CLKSOURCE | STK_CTRL_TICKINT | STK_CTRL_ENABLE;
}

/* Called once from main(), before nimble_scheduler_start(). */
void nimble_stm32f4_system_init(void)
{
    nimble_stm32f4_nvic_priority_init();
    nimble_stm32f4_systick_init(HSI_CLOCK_HZ);
}
