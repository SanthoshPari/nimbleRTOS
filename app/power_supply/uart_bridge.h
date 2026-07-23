#ifndef UART_BRIDGE_H
#define UART_BRIDGE_H

#include "nimble_queue.h"

extern nimble_queue_t g_uart_rx_queue;

void uart_bridge_init(void);

/*
 * TX helper - intentionally a stub. A full USART peripheral driver
 * (baud-rate register math, DMA-driven TX, etc.) is real work but
 * doesn't teach anything further about RTOS internals, which is what
 * this project exists to demonstrate (see docs/ARCHITECTURE.md). The
 * RX path (isr_bridge.c -> g_uart_rx_queue -> comms_task.c) is real
 * and complete because *that's* the ISR-to-task handoff pattern worth
 * showing; TX is a named, obvious place to plug in a real driver
 * later without touching any task logic.
 */
void ps_uart_puts(const char *s);

#endif /* UART_BRIDGE_H */
