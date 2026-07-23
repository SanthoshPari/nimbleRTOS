# Priority Inheritance — Worked Example

Using this project's own task set: `ProtectionTask` (high priority),
`CommsTask` (medium), `LoggerTask` (low) — same shape as the power
supply firmware this kernel is meant to run.

## Without priority inheritance (the bug)

```
Priority
 High │ ProtectionTask         ░░░░░░░░░░░░░░░░░░BLOCKED░░░░░░░░░░░░░░░░│───RUNS
      │
 Med  │ CommsTask                              ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
      │
 Low  │ LoggerTask   ──RUNS(holds mutex)──┐ preempted, mutex still held │
      └────────────────────────────────────────────────────────────────┘
       t0                                 t1                    t2      t3
```

- `t0`: `LoggerTask` acquires the mutex, starts its critical section.
- `t1`: `ProtectionTask` becomes ready, immediately blocks on the same
  mutex (`LoggerTask` still holds it) — expected, bounded wait.
- Also around `t1`: `CommsTask` becomes ready. It's **higher priority
  than `LoggerTask`**, so it preempts `LoggerTask` mid-critical-section
  and runs freely — `CommsTask` has no relationship to the mutex at
  all, it's just unrelated work that happens to outrank `LoggerTask`.
- `LoggerTask` cannot resume, cannot finish its critical section,
  cannot release the mutex — until `CommsTask` is done, however long
  that takes.
- `ProtectionTask`, the **highest-priority task in the system**, stays
  blocked the entire time. Its wait is now bounded only by `CommsTask`'s
  behavior — a task it has no direct dependency on. That's unbounded
  priority inversion.

## With priority inheritance (`nimble_mutex_lock`)

```
Priority
 High │ ProtectionTask         ░░BLOCKED░░│───────────────RUNS
      │
 Med  │ CommsTask                          ▓▓▓(can't preempt - Logger now runs AT High)▓▓▓
      │
 Low  │ LoggerTask   ──RUNS(holds mutex)───┼──runs AT BOOSTED (High) priority──┐unlock,restored
      └───────────────────────────────────────────────────────────────────────┘
       t0                                  t1                            t1'  t2
```

- `t1`: `ProtectionTask` blocks on the mutex. `nimble_mutex_lock()`
  compares priorities (`self->priority > mutex->owner->priority`) and
  boosts `LoggerTask`'s effective `priority` field to
  `ProtectionTask`'s level, right there, before blocking.
- Because `LoggerTask` is now *running at High priority*, `CommsTask`
  (Medium) cannot preempt it — the scheduler has no idea `LoggerTask`
  is "really" a low-priority task; `tcb->priority` is what the
  bitmap-indexed ready list actually uses.
- `LoggerTask` finishes its critical section quickly (`t1'`), calls
  `nimble_mutex_unlock()`, which restores `owner->priority =
  owner->base_priority` and wakes `ProtectionTask` via
  `nimble_scheduler_wake_waiter()`.
- Net effect: `ProtectionTask`'s wait is now bounded by "however long
  `LoggerTask`'s critical section legitimately takes" — a quantity you
  can actually measure and reason about — not by an unrelated
  medium-priority task's runtime.

## What this implementation does NOT do (and why that's stated, not hidden)

- **No transitive/chained inheritance.** If `LoggerTask` were itself
  blocked on a *second* mutex held by a still-lower-priority task, that
  third task would not automatically inherit `ProtectionTask`'s
  priority. Real RTOS priority-inheritance implementations (including
  FreeRTOS's) do handle single-level inheritance the same way this one
  does, and chained inheritance is a known hard edge case even in
  production kernels — deep mutex nesting is also a design smell in
  firmware at this scale, so the pragmatic answer here is "avoid nested
  mutex acquisition across tasks of very different priority," documented
  rather than half-implemented.
- **No priority ceiling protocol.** This is inheritance (boost on
  contention), not the alternative ceiling-based approach (every mutex
  pre-assigned the highest priority of any task that might lock it).
  Ceiling protocols avoid inversion entirely at the cost of every
  mutex-holder always running boosted, which trades average-case
  responsiveness for a simpler worst-case bound — a legitimate
  alternative design, not implemented here, and worth being able to
  name as the alternative if asked in an interview.
