# nimbleRTOS

A small preemptive, priority-based real-time kernel for ARM Cortex-M4,
written from scratch in C and ARM assembly — no FreeRTOS, no CMSIS-RTOS,
no borrowed scheduler code.

This exists to demonstrate RTOS *internals*, not just RTOS *usage*: how a
context switch actually works at the register level, how priority
inheritance prevents priority inversion, how a tick interrupt drives
preemption, and how all of that gets wired into a real embedded
application.

The application ported on top of the kernel is a digital power-supply
controller (`IDLE -> SOFT_START -> RUNNING -> FAULT`), the same class of
system used as the running example throughout the design docs — proof
the kernel isn't just a toy that runs one demo task.

## Status

🚧 Work in progress, built section by section. See `docs/ROADMAP.md` for
what's done and what's next.

## Why this exists

Most embedded portfolios show "I used FreeRTOS." This shows "I know what
FreeRTOS is doing when you call `xTaskCreate()`." The scheduler, the
context switch, the mutex priority-inheritance protocol — all written
and explained from first principles in `docs/`.

## Structure

```
nimbleRTOS/
├── kernel/           # Core kernel: scheduler, TCB, sync primitives, timers
│   ├── include/       # Public kernel API headers
│   └── src/           # Kernel implementation
├── port/              # Hardware/architecture-specific code
│   ├── arch/cortex-m4/  # Context switch, exception handlers (ASM + C)
│   └── stm32f4/         # Startup code, linker script, register access
├── app/power_supply/  # Demo application: power-supply state machine
├── tests/
│   ├── unit/           # Host-side unit tests (no hardware/QEMU needed)
│   └── host/           # Test runner scaffolding
├── docs/              # Architecture & design-decision documentation
└── .github/workflows/ # CI: build for target + run host tests
```

## Building

See `docs/BUILDING.md` (added in a later section) for toolchain setup,
QEMU instructions, and real-hardware flashing steps.

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) — kernel design & scheduling model
- [`docs/ROADMAP.md`](docs/ROADMAP.md) — build progress
- Design-decision docs are added alongside each kernel subsystem as it's built
