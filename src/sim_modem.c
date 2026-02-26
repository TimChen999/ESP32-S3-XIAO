#include "sim_modem.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SIM_MODEM";

// ============================================================================
//  INTERNAL HELPERS
// ============================================================================

// ---------------------------------------------------------------------------
//  send_response
//
//  Sends a string back to the driver over the modem's UART TX.
//  All modem responses end with \r\n.
//
//  TODO:
//    1. Use uart_write_bytes() to send the response string
//    2. Append \r\n after the response
//    3. (Optional) Add a small delay to simulate real modem response latency
// ---------------------------------------------------------------------------
static void send_response(sim_modem_config_t *config, const char *response)
{
    // TODO: implement
}

// ---------------------------------------------------------------------------
//  read_line
//
//  Reads bytes from UART RX until a line terminator ('\r' or '\n') is found.
//  Returns the number of bytes read (excluding the terminator).
//
//  TODO:
//    1. Read one byte at a time from UART using uart_read_bytes() with a timeout
//    2. Accumulate into the provided buffer
//    3. Stop when '\r' or '\n' is received, or buffer is full
//    4. Null-terminate the buffer
//    5. Return the length, or -1 on timeout
// ---------------------------------------------------------------------------
static int read_line(sim_modem_config_t *config, char *buf, size_t buf_size,
                     TickType_t timeout)
{
    // TODO: implement
    return -1;
}

// ---------------------------------------------------------------------------
//  handle_at_command
//
//  The AT command dispatcher. Receives a null-terminated command string,
//  checks it against known commands, and sends back the appropriate response.
//
//  The modem's state machine gates which commands are valid.
//
//  TODO — implement these commands in order of priority:
//
//  ┌─────────────────┬────────────────────────────────────────────────┐
//  │ Command         │ Expected behavior                              │
//  ├─────────────────┼────────────────────────────────────────────────┤
//  │ "AT"            │ Reply "OK". Simplest ping — always works.      │
//  │ "ATE0"          │ Reply "OK". (Disable echo — we never echo.)    │
//  │ "AT+CPIN?"      │ If state >= READY:                             │
//  │                 │   Set state = SIM_READY                        │
//  │                 │   Reply "+CPIN: READY\r\nOK"                   │
//  │ "AT+CSQ"        │ Reply "+CSQ: 20,0\r\nOK"  (good signal)       │
//  │ "AT+CREG?"      │ If state >= SIM_READY:                         │
//  │                 │   Set state = REGISTERED                       │
//  │                 │   Reply "+CREG: 0,1\r\nOK"                     │
//  │ "AT+CEREG?"     │ Same as CREG — reply "+CEREG: 0,1\r\nOK"      │
//  │ "AT+COPS?"      │ Reply "+COPS: 0,0,\"Fake Cellular\"\r\nOK"    │
//  │ "AT+CGDCONT=…"  │ Reply "OK". (Store APN if you want to.)       │
//  │ "AT+CGACT=1,1"  │ If state >= REGISTERED:                       │
//  │                 │   Set state = PDP_ACTIVE                       │
//  │                 │   Reply "OK"                                   │
//  │ "ATD*99#"       │ If state == PDP_ACTIVE:                        │
//  │                 │   Reply "CONNECT 115200"                       │
//  │                 │   Set state = DATA_MODE                        │
//  │                 │   (Caller should then switch to PPP handler)   │
//  │ (unknown)       │ Reply "ERROR"                                  │
//  └─────────────────┴────────────────────────────────────────────────┘
//
//  Hint: use strncmp() or strstr() for matching. AT commands are
//  case-insensitive on real modems, but case-sensitive is fine here.
//
//  Returns: the new state after processing the command
// ---------------------------------------------------------------------------
static sim_modem_state_t handle_at_command(sim_modem_config_t *config,
                                           const char *cmd)
{
    ESP_LOGI(TAG, "RX cmd: \"%s\" (state=%d)", cmd, config->state);

    // TODO: implement command dispatch table
    //
    // Skeleton:
    //   if (strcmp(cmd, "AT") == 0) {
    //       send_response(config, "OK");
    //   } else if (strcmp(cmd, "AT+CPIN?") == 0) {
    //       ...
    //   } else {
    //       send_response(config, "ERROR");
    //   }

    return config->state;
}

// ---------------------------------------------------------------------------
//  handle_ppp_data_mode
//
//  Called when the modem enters DATA_MODE after ATD*99#.
//  This is Phase 2 — PPP/IP packet handling.
//
//  TODO (implement later):
//    1. Read raw bytes from UART (no longer line-based — binary PPP frames)
//    2. PPP frames are delimited by 0x7E bytes (HDLC framing)
//    3. First negotiate LCP:
//       - Receive LCP Config-Request from driver
//       - Send LCP Config-Ack back
//       - Send our own LCP Config-Request
//       - Receive LCP Config-Ack
//    4. Then negotiate IPCP:
//       - Receive IPCP Config-Request (driver requesting IP)
//       - Send IPCP Config-Nak with IP we want to assign (e.g. 10.0.0.2)
//       - Receive IPCP Config-Request again with our assigned IP
//       - Send IPCP Config-Ack
//       - Send our IPCP Config-Request (our IP: 10.0.0.1)
//       - Receive IPCP Config-Ack
//    5. Link is now up — IP packets arrive wrapped in PPP/HDLC frames
//    6. Extract IP packets, inspect protocol:
//       - ICMP echo request → reply with ICMP echo response
//       - TCP SYN → do 3-way handshake, serve canned payload
//       - UDP DNS query → reply with canned A record
//    7. If "+++" escape sequence detected, transition back to AT command mode
//
//  For now, just log that we entered data mode and sit in a delay loop.
// ---------------------------------------------------------------------------
static void handle_ppp_data_mode(sim_modem_config_t *config)
{
    ESP_LOGI(TAG, "Entered PPP data mode — not yet implemented");

    // TODO: replace with PPP frame handler
    while (config->state == SIM_MODEM_STATE_DATA_MODE) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


// ============================================================================
//  PUBLIC API
// ============================================================================

void sim_modem_init(sim_modem_config_t *config)
{
    // TODO:
    //   1. Set config->state = SIM_MODEM_STATE_OFF
    //   2. Populate a uart_config_t struct:
    //        .baud_rate           = config->baud_rate
    //        .data_bits           = UART_DATA_8_BITS
    //        .parity              = UART_PARITY_DISABLE
    //        .stop_bits           = UART_STOP_BITS_1
    //        .flow_ctrl           = config->flow_control
    //                               ? UART_HW_FLOWCTRL_CTS_RTS
    //                               : UART_HW_FLOWCTRL_DISABLE
    //        .rx_flow_ctrl_thresh = 122  (only matters when flow_control=true)
    //   3. Call uart_param_config(config->uart_num, &uart_cfg)
    //   4. Call uart_set_pin(config->uart_num,
    //                        config->tx_pin, config->rx_pin,
    //                        config->rts_pin, config->cts_pin)
    //      When flow control is disabled, rts_pin/cts_pin are
    //      UART_PIN_NO_CHANGE — the UART ignores them.
    //   5. Call uart_driver_install(config->uart_num,
    //                               RX_BUF_SIZE, TX_BUF_SIZE, 0, NULL, 0)
    //
    //   Suggested buffer sizes: 1024 for RX, 1024 for TX

    ESP_LOGI(TAG, "sim_modem_init: uart=%d tx=%d rx=%d baud=%d",
             config->uart_num, config->tx_pin, config->rx_pin,
             config->baud_rate);
}

void sim_modem_task(void *param)
{
    sim_modem_config_t *config = (sim_modem_config_t *)param;
    char line_buf[256];

    // -----------------------------------------------------------------------
    // Step 1: Simulate modem power-on sequence
    //   TODO:
    //     - Delay 1-2 seconds (modem boot time)
    //     - Set state = SIM_MODEM_STATE_READY
    //     - Log "Modem powered on"
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Step 2: Main AT command loop
    //   TODO:
    //     - While state != DATA_MODE:
    //         a. Call read_line() to get the next command from the driver
    //         b. If read timed out, continue (maybe send a URC periodically)
    //         c. Call handle_at_command() with the received line
    //         d. Update state from the return value
    //     - When state transitions to DATA_MODE, break out of this loop
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Step 3: Enter PPP data mode
    //   TODO:
    //     - Call handle_ppp_data_mode()
    //     - If we ever exit data mode (e.g. +++ escape), loop back to Step 2
    // -----------------------------------------------------------------------

    ESP_LOGI(TAG, "sim_modem_task started");

    // TODO: implement the flow above
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
