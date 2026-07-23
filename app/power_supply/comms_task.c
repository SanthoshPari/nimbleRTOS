#include <string.h>
#include <stdio.h>
#include "power_supply.h"
#include "uart_bridge.h"
#include "nimble_task.h"

/*
 * CommsTask - the UART CLI.
 *
 * Sits below ControlTask/ProtectionTask in priority on purpose: a
 * human typing commands can tolerate a few milliseconds of jitter in
 * a way the control loop and fault path cannot. Blocks on
 * g_uart_rx_queue one byte at a time (filled by USART1_IRQHandler in
 * isr_bridge.c) and assembles a line buffer, same handoff pattern as
 * ControlTask's ADC queue.
 */

#define CLI_LINE_BUF_LEN 64

static void handle_command(const char *line)
{
    if (strcmp(line, "status") == 0) {
        ps_status_t status;
        ps_status_get(&status);

        char buf[96];
        snprintf(buf, sizeof(buf), "state=%s v=%.0fmV i=%.0fmA faults=%lu\r\n",
                 ps_state_name(status.state), (double)status.voltage_mv,
                 (double)status.current_ma, (unsigned long)status.fault_count);
        ps_uart_puts(buf);
    } else if (line[0] != '\0') {
        ps_uart_puts("unknown command\r\n");
    }
}

void comms_task_fn(void *arg)
{
    (void)arg;
    uart_bridge_init();

    char line_buf[CLI_LINE_BUF_LEN];
    uint32_t line_len = 0;

    for (;;) {
        uint8_t byte;
        nimble_status_t result = nimble_queue_receive(&g_uart_rx_queue, &byte, NIMBLE_WAIT_FOREVER);
        if (result != NIMBLE_OK) {
            continue; /* NIMBLE_WAIT_FOREVER never actually times out; defensive only */
        }

        if (byte == '\r' || byte == '\n') {
            line_buf[line_len] = '\0';
            handle_command(line_buf);
            line_len = 0;
        } else if (line_len < CLI_LINE_BUF_LEN - 1) {
            line_buf[line_len++] = (char)byte;
        }
        /* Silently drop characters beyond CLI_LINE_BUF_LEN rather than
         * overflow - a real CLI would echo an error; kept minimal here
         * since the RTOS task/queue plumbing is the point of this
         * file, not CLI UX polish. */
    }
}
