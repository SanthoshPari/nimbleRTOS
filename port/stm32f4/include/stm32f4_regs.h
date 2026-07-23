#ifndef STM32F4_REGS_H
#define STM32F4_REGS_H

#include <stdint.h>

/*
 * Minimal, hand-written register definitions - deliberately not a
 * full CMSIS device header. Only what this port actually touches:
 * NVIC system-handler priorities and SysTick. Keeping this hand-rolled
 * (rather than vendoring ST's CMSIS pack) means every register access
 * anywhere in this codebase is something written and understood for
 * this project, not inherited boilerplate.
 */

/* System Control Block */
#define SCB_SHPR3   (*(volatile uint32_t *)0xE000ED20UL) /* System Handler Priority Register 3: PendSV, SysTick */

/* SysTick */
#define STK_CTRL    (*(volatile uint32_t *)0xE000E010UL)
#define STK_LOAD    (*(volatile uint32_t *)0xE000E014UL)
#define STK_VAL     (*(volatile uint32_t *)0xE000E018UL)

#define STK_CTRL_ENABLE     (1UL << 0)
#define STK_CTRL_TICKINT    (1UL << 1)
#define STK_CTRL_CLKSOURCE  (1UL << 2) /* 1 = core clock, 0 = core clock / 8 */

#endif /* STM32F4_REGS_H */
