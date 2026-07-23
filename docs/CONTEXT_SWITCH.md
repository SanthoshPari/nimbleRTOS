# Context Switch — Cortex-M4

This is the section of the project that actually proves RTOS
understanding rather than RTOS usage, so it gets the most thorough
writeup. Read this alongside `port/arch/cortex-m4/context_switch.S`.

## Why PendSV, and not SysTick itself, does the switch

`SysTick_Handler` (`exception_handlers.c`) only calls
`nimble_scheduler_tick()` and then, indirectly, requests a switch by
setting `PendSV`'s pending bit. It never switches stacks itself. Two
reasons:

1. **Priority.** PendSV is configured at the *lowest* exception
   priority in the system. If `SysTick_Handler` did the switch
   directly, a context switch could happen in the middle of some other
   time-critical ISR whenever SysTick's priority happened to be higher
   — instead, by only *pending* PendSV, the actual switch is guaranteed
   to happen only after every higher-priority exception currently
   running (or about to run) has finished. This is "tail-chaining":
   the CPU finishes the current ISR, sees PendSV pending, and jumps
   straight into it without returning to Thread mode in between.
2. **Every** reason to switch — a tick, a semaphore give from an ISR,
   a task blocking — funnels through the same one pended-bit mechanism
   (`nimble_port_request_context_switch()`), so there is exactly one
   code path that performs a switch. One path to reason about, one
   path to get right.

## The two stacks-worth of registers

On exception entry, Cortex-M *hardware* automatically pushes 8
registers onto whichever stack was active (`{R0-R3, R12, LR, PC,
xPSR}`) before jumping into the handler — this is the same mechanism
whether the exception is PendSV, SysTick, or any peripheral IRQ. The
*handler* is responsible for the other 8 general-purpose registers
(`R4-R11`) if it's going to change what's running, which is exactly
what `PendSV_Handler` does with `stmdb`/`ldmia`.

```
Higher addr
┌───────────────┐
│     xPSR      │  ─┐
│      PC       │   │
│      LR       │   │  pushed automatically
│      R12      │   │  by hardware on
│      R3       │   │  exception entry
│      R2       │   │
│      R1       │   │
│      R0       │  ─┘
├───────────────┤
│      R11      │  ─┐
│      R10      │   │
│      R9       │   │  pushed manually by
│      R8       │   │  PendSV_Handler
│      R7       │   │  (stmdb r0!, {r4-r11})
│      R6       │   │
│      R5       │   │
│      R4       │  ─┘
└───────────────┘  <- tcb->sp points here after PendSV saves
Lower addr
```

## EXC_RETURN — how the CPU knows where to return *to*

`LR` inside any exception handler doesn't hold a normal return
address; it holds one of a small set of magic `EXC_RETURN` values
(anything with bits `[31:4] = 0xFFFFFFF` marks it as one). The CPU
decodes it on `bx lr` to decide which stack and which mode to resume:

| EXC_RETURN   | Meaning                                      |
|--------------|-----------------------------------------------|
| `0xFFFFFFF1` | Return to Handler mode, use MSP               |
| `0xFFFFFFF9` | Return to Thread mode, use **MSP**            |
| `0xFFFFFFFD` | Return to Thread mode, use **PSP**            |

nimbleRTOS runs every task in Thread mode off the **process** stack
(PSP), reserving the **main** stack (MSP) for exception handling
itself — this is the standard split, and it's what lets a task's own
stack usage be sized and audited completely independently of interrupt
stack usage. `PendSV_Handler` simply `bx lr`s with whatever
`EXC_RETURN` it entered with (it's always `0xFFFFFFFD` once tasks are
running, since tasks only ever get preempted from Thread+PSP). The
first switch is different — see below.

## Why starting the very first task needs `SVC`, not a direct jump

`EXC_RETURN` values only mean anything to the CPU when they're
branched to *from inside an active exception* (`IPSR != 0`, i.e.
Handler mode). `nimble_scheduler_start()` runs from ordinary
`main()`-level Thread-mode code — there is no exception active yet, so
there is no legal way to forge an `EXC_RETURN` return directly from
there.

The fix, `nimble_port_start_first_task()`, is two instructions:
`cpsie i` (make sure interrupts are actually enabled) then `svc 0`.
`svc` is itself an instruction that *raises* an exception — so
`SVC_Handler` genuinely is running in Handler mode, where forging
`0xFFFFFFFD` and `bx`-ing to it is legal. `SVC_Handler` and the second
half of `PendSV_Handler` restore a task's context in exactly the same
way on purpose — the first task's fabricated stack frame (see
`task_stack_init.c`) is indistinguishable from a real preempted task's
frame, so both paths can share the same restore logic conceptually
even though they're separate handlers here for clarity.

## Critical sections use BASEPRI, not a global interrupt disable

`nimble_port_enter_critical()` raises `BASEPRI` to
`NIMBLE_KERNEL_SYSCALL_BASEPRI` rather than executing `cpsid i`. The
difference matters: `BASEPRI` only masks interrupts at that priority
number or numerically higher (i.e. equal-or-lower urgency); anything
configured with a numerically lower priority — reserved for genuinely
critical, latency-sensitive interrupts like a hardware fault or a
safety-relevant ADC watchdog — still preempts *through* a kernel
critical section. A global `cpsid i` cannot make that distinction. The
concrete priority split (which peripheral interrupts sit above vs.
below the kernel's syscall priority) is configured once, at boot, in
the STM32F4 port bring-up.
