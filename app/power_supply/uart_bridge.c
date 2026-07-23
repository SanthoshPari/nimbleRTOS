#include "uart_bridge.h"

nimble_queue_t g_uart_rx_queue;

#define UART_RX_QUEUE_DEPTH 64
static uint8_t uart_rx_buffer[UART_RX_QUEUE_DEPTH];

void uart_bridge_init(void)
{
    nimble_queue_init(&g_uart_rx_queue, uart_rx_buffer, sizeof(uint8_t), UART_RX_QUEUE_DEPTH);
}

void ps_uart_puts(const char *s)
{
    /* Stub - see uart_bridge.h. Would poll USART1->SR TXE and write
     * USART1->DR per character (or hand the buffer to a DMA stream)
     * in a full board bring-up. */
    (void)s;
}
