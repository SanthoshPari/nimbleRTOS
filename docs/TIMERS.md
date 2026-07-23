# Software Timers

## Why a dedicated task, not inline SysTick callbacks

Running timer callbacks directly from `SysTick_Handler` is the naive
approach and the wrong one for anything beyond toggling a GPIO:

1. **ISR context can't safely call blocking kernel APIs.** A timer
   callback that wants to `nimble_mutex_lock()` or
   `nimble_queue_send()`-with-a-timeout simply cannot do that from
   inside an interrupt — there's no task to block.
2. **Every SysTick ISR would grow by however long the callback takes.**
   SysTick's period is the one timing reference the entire kernel
   (delayed-task wakeups, round-robin slicing) depends on. A slow
   callback directly injects jitter into that reference for *every*
   task in the system, not just the one that owns the timer.

`nimble_timer_service_start()` creates one task (`TimerTask`) whose
whole job is: sleep until the next timer is due, run its callback in
normal task context, go back to sleep. Its **priority is a normal,
visible design decision** — pick it low enough that it doesn't starve
`ControlTask`/`ProtectionTask`, high enough that timer callbacks (e.g.
a UART CLI "print stats every 500ms" timer) don't get arbitrarily
delayed by low-priority work.

## Why it uses a semaphore to wake early

If `TimerTask` is sleeping for 800 ticks waiting on a timer due then,
and a *new* timer with a 10-tick period gets started in the meantime,
it must not oversleep past the new, sooner deadline. `nimble_timer_start()`
gives `timer_wake_sem` whenever the new/restarted timer becomes the new
list head; `TimerTask` is waiting on that same semaphore (with a
timeout equal to its current computed sleep), so either the timeout or
the give wakes it — and either way it just recomputes from the current
list state, so there's no special-casing needed for "was I woken early
or did I time out."

## Drift correction

A periodic timer reschedules from its **previous** `expiry_tick`, not
from "now" — `head->expiry_tick + head->period_ticks` — so small,
one-off scheduling delays in when `TimerTask` actually got to run don't
accumulate into long-term drift. If `TimerTask` falls more than a full
period behind (e.g. it was itself starved by higher-priority work for a
while), the code deliberately catches up to `now + period` rather than
firing a burst of already-missed expiries back-to-back — a burst of
queued-up callbacks all firing in a row is rarely what anyone actually
wants from a "run every 100ms" timer.
