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
    // We keep this handler byte-oriented (not line-oriented) because PPP is
    // binary and may contain arbitrary bytes.
    static const unsigned char PPP_FLAG = 0x7E;
    unsigned char byte = 0;
    unsigned char frame_buf[512];
    size_t frame_len = 0;

    ESP_LOGI(TAG, "Entered PPP data mode — collecting HDLC frames");

    while (config->state == SIM_MODEM_STATE_DATA_MODE) {
        // Busy wait for bytes
        int got = uart_read_bytes(config->uart_num, &byte, 1, pdMS_TO_TICKS(200));
        if (got <= 0) {
            // Idle timeout: no bytes right now, keep waiting.
            continue;
        }

        // Step 1: Read raw bytes from UART (PPP is binary, not line-based).
        if (byte == PPP_FLAG) {
            // Step 2: PPP frame delimiter 0x7E found (HDLC flag bytes).
            // This marks frame boundary. Consecutive flags are legal and
            // represent empty fill between frames, so ignore zero-length frames.
            if (frame_len == 0) {
                continue;
            }

            // Frame boundary reached — we now have one complete PPP frame.
            // PPP header is usually: Address(0xFF), Control(0x03), Protocol(2B).
            if (frame_len >= 4 && frame_buf[0] == 0xFF && frame_buf[1] == 0x03) {
                unsigned short protocol = ((unsigned short)frame_buf[2] << 8) | frame_buf[3];
                if (protocol == 0xC021) {
                    ESP_LOGI(TAG, "PPP frame complete: LCP (len=%u)", (unsigned int)frame_len);
                    // TODO Step 3a: parse LCP Config-Request and send Config-Ack.
                } else {
                    ESP_LOGI(TAG, "PPP frame complete: proto=0x%04X (len=%u)",
                             protocol, (unsigned int)frame_len);
                }
            } else {
                ESP_LOGW(TAG, "PPP frame complete but too short/unknown header (len=%u)",
                         (unsigned int)frame_len);
            }

            // Ready for next frame.
            frame_len = 0;
            continue;
        }

        // Accumulate frame bytes until the next 0x7E delimiter.
        if (frame_len < sizeof(frame_buf)) {
            frame_buf[frame_len++] = byte;
        } else {
            // Overflow guard: drop this frame and wait for next delimiter.
            ESP_LOGW(TAG, "PPP frame overflow; dropping partial frame");
            frame_len = 0;
        }
    }
}


// ============================================================================
//  PUBLIC API
// ============================================================================

void sim_modem_init(sim_modem_config_t *config)
{
    // 1. Set initial state to OFF so the task knows the modem hasn't booted yet.
    //    The sim_modem_task will delay (power-on) then transition to READY before
    //    accepting AT commands — the driver must not send until the modem is READY.
    config->state = SIM_MODEM_STATE_OFF;

    // 2. Describe the UART's electrical parameters: baud rate, word format,
    //    and flow control. This struct doesn't touch hardware yet — it's just
    //    a settings object that uart_param_config() will apply to the peripheral.
    //    8N1 (8 data bits, no parity, 1 stop bit) is the universal default for
    //    AT command interfaces — every cellular modem uses it.
    uart_config_t uart_cfg = {
        .baud_rate           = config->baud_rate,
        .data_bits           = UART_DATA_8_BITS, // <-- various defined types in uart.h
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = config->flow_control
                                   ? UART_HW_FLOWCTRL_CTS_RTS
                                   : UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // 3. Apply the config to the UART peripheral registers. After this call,
    //    the hardware knows the baud rate, word format, and flow control mode,
    //    but no pins are assigned yet and no driver buffers exist — the UART
    //    can't actually send or receive until steps 4 and 5 complete.
    uart_param_config(config->uart_num, &uart_cfg);

    // 4. Route the UART's TX, RX, RTS, and CTS signals to physical GPIO pins
    //    via the ESP32's GPIO matrix. Without this, the UART has no electrical
    //    connection — bytes written would go nowhere. When flow control is off,
    //    rts_pin and cts_pin are -1 (UART_PIN_NO_CHANGE), so those signals
    //    stay unconnected and the UART ignores them.
    uart_set_pin(config->uart_num,
                 config->tx_pin, config->rx_pin,
                 config->rts_pin, config->cts_pin);

    // 5. Install the UART driver: allocate RX and TX ring buffers (1024-byte) 
    //    and register the driver with the ESP-IDF UART layer. This call allows uart_*
    //    read/write calls to be used, which abstracts away the direct register reading
    //    and polling you have to do to read from UART otherwise
    uart_driver_install(config->uart_num, 1024, 1024, 0, NULL, 0);

    ESP_LOGI(TAG, "sim_modem_init: uart=%d tx=%d rx=%d baud=%d",
             config->uart_num, config->tx_pin, config->rx_pin,
             config->baud_rate);
}

void sim_modem_task(void *param)
{
    sim_modem_config_t *config = (sim_modem_config_t *)param;
    char line_buf[256];

    // -----------------------------------------------------------------------
    // Step 1: Simulate modem power-on sequence.
    // Real modems need time before they can accept AT commands, so we emulate
    // that boot window to keep driver timing behavior realistic.
    // -----------------------------------------------------------------------
    vTaskDelay(pdMS_TO_TICKS(1500));
    config->state = SIM_MODEM_STATE_READY;
    ESP_LOGI(TAG, "Modem powered on, state=READY");

    ESP_LOGI(TAG, "sim_modem_task started");

    while (1) {
        // -------------------------------------------------------------------
        // Step 2: Main AT command loop
        // Keep servicing AT commands until the modem is told to switch into
        // PPP data mode (ATD*99#). A read timeout is normal when idle.
        // -------------------------------------------------------------------
        while (config->state != SIM_MODEM_STATE_DATA_MODE) {
            // Basically busy wait for commands
            int line_len = read_line(config, line_buf, sizeof(line_buf), pdMS_TO_TICKS(500));

            // Test if command found this cycle
            if (line_len <= 0) {
                // Timeout/no data: stay responsive but do nothing this cycle.
                continue;
            }

            // Handle command
            config->state = handle_at_command(config, line_buf);
        }

        // -------------------------------------------------------------------
        // Step 3: Enter PPP data mode.
        // This function blocks while in DATA_MODE. If it returns (e.g. because
        // an escape sequence switched state), we loop back to Step 2.
        // -------------------------------------------------------------------
        handle_ppp_data_mode(config);
    }
}
