#ifndef POWER_SUPPLY_H
#define POWER_SUPPLY_H

#include "power_supply_types.h"
#include "nimble_mutex.h"
#include "nimble_semaphore.h"
#include "nimble_queue.h"

/*
 * Shared state and the primitives that connect the four tasks.
 * Deliberately kept in one small header/source pair rather than
 * scattered across each task file, so the "what talks to what"
 * picture is visible in one place:
 *
 *   ADC_IRQHandler --(queue: g_adc_sample_queue)--> ControlTask
 *   ADC_IRQHandler --(semaphore: g_fault_semaphore, on threshold trip)--> ProtectionTask
 *   ControlTask    --(mutex: g_status_mutex)--> shared g_status, read by CommsTask/LoggerTask
 *   ProtectionTask --(mutex: g_status_mutex)--> forces state = PS_STATE_FAULT
 */

extern nimble_mutex_t     g_status_mutex;
extern nimble_semaphore_t g_fault_semaphore;
extern nimble_queue_t     g_adc_sample_queue;

void ps_shared_init(void);

/* Thread-safe read of the current status snapshot. */
void ps_status_get(ps_status_t *out);

/* Thread-safe partial update - only ControlTask and ProtectionTask
 * call this. Internal helper, not a public "anyone can mutate global
 * state" API. */
void ps_status_set_measurement(float voltage_mv, float current_ma);
void ps_status_set_state(ps_state_t state);
void ps_status_note_fault(void);

#endif /* POWER_SUPPLY_H */
