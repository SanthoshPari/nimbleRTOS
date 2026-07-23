#ifndef NIMBLE_CONFIG_H
#define NIMBLE_CONFIG_H

/*
 * nimbleRTOS configuration.
 *
 * Kept in one file, deliberately small, so every knob is visible and
 * defensible in an interview — no mystery defaults buried in the port
 * layer.
 */

/* Number of distinct priority levels. Priority 0 is reserved for the
 * idle task (lowest); NIMBLE_MAX_PRIORITIES - 1 is highest. Must be
 * <= 32 because the ready-list bitmap is a single uint32_t. */
#define NIMBLE_MAX_PRIORITIES        32U

/* SysTick frequency in Hz -> defines the scheduler tick period. */
#define NIMBLE_TICK_RATE_HZ          1000U

/* Minimum stack size (in 32-bit words) nimbleRTOS will accept for a
 * task. Deliberately conservative; catches "I forgot to size the
 * stack" mistakes at task-creation time instead of as a mystery
 * hard fault three weeks later. */
#define NIMBLE_MIN_STACK_WORDS       64U

/* Maximum length of a task name, for debugging/logging. */
#define NIMBLE_MAX_TASK_NAME_LEN     16U

/* Enable run-time stack high-water-mark tracking (fills stack with a
 * canary pattern at creation, checked later). Costs a small amount of
 * time at task-creation only. */
#define NIMBLE_ENABLE_STACK_CHECKING 1

#endif /* NIMBLE_CONFIG_H */
