# Scheduler Design

## The ready-list bitmap

`NIMBLE_MAX_PRIORITIES` (32) ready lists, one per priority level, plus a
single `uint32_t ready_bitmap` where bit *N* is set iff ready list *N*
has at least one task in it.

Worked example — say priorities 2, 5, and 9 have ready tasks:

```
bit index:  ...9........5.....2.....
ready_bitmap = 0b0000...1000010001000...0

31 - __builtin_clz(ready_bitmap) = 9   <- highest set bit = highest priority
```

`__builtin_clz` compiles to a single `CLZ` instruction on Cortex-M4, so
"find the highest-priority ready task" is O(1) — one instruction plus
an array index — regardless of how many priority levels exist or how
many tasks are ready. The alternative (scanning all 32 lists looking
for the first non-empty one, or maintaining the tasks in one big
priority-sorted list) is either O(priorities) or O(tasks) per
scheduling decision. This is the same technique FreeRTOS's generic
scheduler uses; implemented independently here.

## Priority preemption vs. time-slicing — two different mechanisms

These get conflated a lot, including in early notes on this exact
project, so it's worth being explicit:

- **Priority-based preemption** happens *immediately*, any time a
  higher-priority task becomes ready (e.g. a semaphore give unblocks
  it) — via `nimble_port_request_context_switch()` pending PendSV,
  which runs as soon as the current ISR chain unwinds. It does **not**
  wait for a tick.
- **Time-slicing** (round-robin) only rotates tasks that are at the
  **same** priority as each other, and only happens as a side effect
  of a scheduling decision already being made (a tick, a block, a
  yield). Look at `nimble_scheduler_select_next()`: the requeue-to-tail
  logic only ever touches `ready_lists[nimble_current_tcb->priority]`
  — a lower-priority task is structurally incapable of preempting a
  higher-priority one through this path, because it lives in a
  different list entirely.

Concretely, in the power-supply demo app: `ControlTask` (highest
priority) is **never** time-sliced with `LoggerTask` (lowest priority)
— they're never in the same ready list, so the round-robin rotation
code never touches them relative to each other. `ControlTask` only
stops running when it blocks (e.g. waiting on the next ADC sample via
a stream buffer) or when `ProtectionTask` preempts it outright on a
fault condition. If you only ever see one task at a given priority
level, time-slicing never engages at all for that level — it's a
tie-breaker, not a scheduling policy in itself.

## Why the delayed-task list is a plain linear list, not a delta list

FreeRTOS keeps delayed tasks in a list sorted by wake time, so the tick
handler only ever has to look at the head. This kernel scans the whole
delayed list every tick instead. That's a deliberate trade-off for a
project targeting single-digit-to-low-dozens of tasks in a firmware
image: at that scale the scan costs low single-digit microseconds and
the code is dramatically simpler to reason about and to explain than
delta-list insertion/removal. It's the kind of trade documented here
specifically so it reads as a decision, not a gap.

## Idle task

Priority 0 is reserved for a permanently-ready idle task. This
guarantees `ready_bitmap` is never all-zero, which removes an entire
class of "what does the scheduler do with nothing to run" edge case
from every other function in this file.
