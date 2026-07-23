#include "power_supply.h"
#include "uart_bridge.h"
#include "nimble_port.h"

/*
 * These override the weak Default_Handler aliases for ADC_IRQHandler
 * and USART1_IRQHandler declared in
 * port/stm32f4/startup/startup_stm32f407xx.s - standard weak-symbol
 * linking, no registration step needed.
 *
 * Both follow the same shape, and it's the shape that matters more
 * than the peripheral specifics: do the absolute minimum in the ISR
 * (read one register, hand the data off), then request a context
 * switch only if that handoff actually woke a higher-or-equal
 * priority task - the exact "give from ISR, then
 * portYIELD_FROM_ISR()-equivalent" pattern this project's design docs
 * call out (see kernel/include/nimble_semaphore.h /
 * nimble_queue.h).
 */

#define ADC1_DR         (*(volatile uint32_t *)0x4001204CUL)
#define USART1_SR       (*(volatile uint32_t *)0x40011000UL)
#define USART1_DR       (*(volatile uint32_t *)0x40011004UL)
#define USART1_SR_RXNE  (1UL << 5)

void ADC_IRQHandler(void)
{
    /* A full driver would check/clear ADC1->SR's EOC flag here first.
     * Stubbed to "always ready" - the ADC channel-scan/DMA
     * configuration itself is peripheral-driver work, out of scope
     * for what this project demonstrates (see docs/ARCHITECTURE.md).
     * The handoff below is the real, complete part. */
    ps_adc_sample_t sample;
    sample.voltage_raw = (uint16_t)(ADC1_DR & 0xFFFU);
    sample.current_raw = sample.voltage_raw; /* stub: real hardware scans two channels */

    bool needs_yield = false;
    nimble_queue_send_from_isr(&g_adc_sample_queue, &sample, &needs_yield);

    /* Fault check happens right here, in the ISR, rather than waiting
     * for ControlTask to notice - this is what gives ProtectionTask
     * its low, bounded reaction latency: the semaphore give (and the
     * resulting preemption once we yield below) happens within this
     * same ISR invocation, not one full ControlTask scheduling round
     * later. */
    float voltage_mv = (float)sample.voltage_raw * (16500.0f / 4095.0f);
    float current_ma = (float)sample.current_raw * (5000.0f / 4095.0f);
    if (voltage_mv > PS_OVERVOLTAGE_LIMIT_MV || current_ma > PS_OVERCURRENT_LIMIT_MA) {
        bool fault_needs_yield = false;
        nimble_semaphore_give_from_isr(&g_fault_semaphore, &fault_needs_yield);
        needs_yield = needs_yield || fault_needs_yield;
    }

    if (needs_yield) {
        nimble_port_request_context_switch();
    }
}

void USART1_IRQHandler(void)
{
    if ((USART1_SR & USART1_SR_RXNE) == 0) {
        return; /* some other USART1 interrupt source, not RX-not-empty */
    }
    uint8_t byte = (uint8_t)(USART1_DR & 0xFFU);

    bool needs_yield = false;
    nimble_queue_send_from_isr(&g_uart_rx_queue, &byte, &needs_yield);
    if (needs_yield) {
        nimble_port_request_context_switch();
    }
}
