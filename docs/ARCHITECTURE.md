# nimbleRTOS Architecture

## Goals

1. **Preemptive, priority-based scheduling** — the highest-priority ready
   task always runs; equal-priority tasks time-slice.
2. **Deterministic context switch** — bounded, known-cost switch triggered
   from `PendSV`, never from inside a hardware ISR directly.
3. **Priority inheritance** on mutexes — bounded priority-inversion, not
   just "hope it doesn't happen."
4. **Small and auditable** — every line should be explainable in an
   interview. No feature exists "because FreeRTOS has it."

## Non-goals (explicitly out of scope, and why)

- **MPU / memory protection** — high value for safety-critical/automotive,
  low marginal value for demonstrating RTOS internals. Documented as a
  "future work" item instead of half-implemented.
- **Tickless idle / low-power modes** — real engineering effort, but
  orthogonal to the scheduling/synchronization internals this project
  exists to demonstrate.
- **SMP / multi-core** — single Cortex-M core target only.

## Scheduling model

- Fixed-priority preemptive scheduler, 32 priority levels (bitmap-indexed
  ready list, `CLZ`-based O(1) highest-priority lookup — same technique
  FreeRTOS uses, implemented independently here).
- Round-robin time-slicing *only* among tasks at the same priority level,
  driven by `SysTick`.
- A task can be in exactly one of: `READY`, `RUNNING`, `BLOCKED`,
  `SUSPENDED`.

## Context switch trigger chain

```
Hardware event (SysTick tick / ISR unblocks a task)
        │
        ▼
Scheduler decides a switch is needed
        │
        ▼
Set PendSV pending bit (never switch stacks inside the triggering ISR)
        │
        ▼
CPU finishes current ISR, tail-chains into PendSV at lowest exception priority
        │
        ▼
PendSV_Handler (asm): save software-saved registers, swap stack pointers,
restore next task's context
```

PendSV is used — not SVC, not a raw SysTick-driven switch — for the same
reason FreeRTOS and most production Cortex-M kernels use it: it can be
pended and will only actually execute once all higher-priority
exceptions have finished, so a context switch never happens in the
middle of a time-critical ISR. This is detailed with register-level
diagrams in `docs/CONTEXT_SWITCH.md` (added alongside the port layer).

## Directory-to-concept map

| Concept                          | Where it lives                              |
|-----------------------------------|----------------------------------------------|
| Task Control Block, ready lists   | `kernel/include/nimble_task.h`, `kernel/src/task.c` |
| Scheduler core                    | `kernel/src/scheduler.c`                     |
| Context switch (asm)              | `port/arch/cortex-m4/context_switch.S`       |
| SysTick / PendSV handlers         | `port/arch/cortex-m4/exception_handlers.c`   |
| Mutex + priority inheritance      | `kernel/src/mutex.c`                         |
| Semaphore                         | `kernel/src/semaphore.c`                     |
| Queue                             | `kernel/src/queue.c`                         |
| Software timers                   | `kernel/src/timer.c`                         |
| STM32F4 startup / linker          | `port/stm32f4/`                              |
| Power-supply demo app             | `app/power_supply/`                          |

## Why a power-supply controller as the demo app

It's a realistic multi-task system with real timing pressure:

- `ControlTask` — highest priority, runs the control loop, must never be
  starved.
- `ProtectionTask` — reacts to ADC-detected fault conditions (OCP/OVP),
  second-highest priority.
- `CommsTask` — UART CLI, lower priority, can tolerate jitter.
- `LoggerTask` — lowest priority, soaks up idle time.

This shape is exactly where priority inversion bugs and scheduling
mistakes actually bite in real firmware, which makes it a meaningful
stress test for the kernel rather than a toy "blink two LEDs" demo.
