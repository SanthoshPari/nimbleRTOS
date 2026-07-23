#include <string.h>
#include "power_supply.h"
#include "nimble_port.h"

nimble_mutex_t     g_status_mutex;
nimble_semaphore_t g_fault_semaphore;
nimble_queue_t     g_adc_sample_queue;

/* Backing storage for the ADC sample queue. 8 slots is enough to
 * absorb ControlTask being briefly delayed by a higher-priority
 * ProtectionTask run without dropping samples - sized as a documented
 * trade-off, not a guess: at the target sample rate, 8 slots is a few
 * milliseconds of buffering, comfortably more than ProtectionTask's
 * worst-case fault-handling time. */
#define ADC_QUEUE_DEPTH 8
static ps_adc_sample_t adc_queue_buffer[ADC_QUEUE_DEPTH];

static ps_status_t g_status;

void ps_shared_init(void)
{
    nimble_mutex_init(&g_status_mutex);
    nimble_semaphore_init(&g_fault_semaphore, 0, 1);
    nimble_queue_init(&g_adc_sample_queue, (uint8_t *)adc_queue_buffer,
                       sizeof(ps_adc_sample_t), ADC_QUEUE_DEPTH);

    memset(&g_status, 0, sizeof(g_status));
    g_status.state = PS_STATE_IDLE;
}

void ps_status_get(ps_status_t *out)
{
    nimble_mutex_lock(&g_status_mutex, NIMBLE_WAIT_FOREVER);
    *out = g_status;
    nimble_mutex_unlock(&g_status_mutex);
}

void ps_status_set_measurement(float voltage_mv, float current_ma)
{
    nimble_mutex_lock(&g_status_mutex, NIMBLE_WAIT_FOREVER);
    g_status.voltage_mv = voltage_mv;
    g_status.current_ma = current_ma;
    nimble_mutex_unlock(&g_status_mutex);
}

void ps_status_set_state(ps_state_t state)
{
    nimble_mutex_lock(&g_status_mutex, NIMBLE_WAIT_FOREVER);
    g_status.state = state;
    nimble_mutex_unlock(&g_status_mutex);
}

void ps_status_note_fault(void)
{
    nimble_mutex_lock(&g_status_mutex, NIMBLE_WAIT_FOREVER);
    g_status.state = PS_STATE_FAULT;
    g_status.fault_count++;
    nimble_mutex_unlock(&g_status_mutex);
}
