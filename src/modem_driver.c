#include "modem_driver.h"

#include <string.h>
#include <stdio.h>
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "MODEM_DRV";

// ============================================================================
//  INTERNAL HELPERS
// ============================================================================

// ---------------------------------------------------------------------------
//  send_command_raw
//
//  Sends a command string over UART with a trailing '\r'.
//
//  TODO:
//    1. Write the command bytes using uart_write_bytes()
//    2. Write a '\r' byte to terminate the command
//    3. Log the command being sent for debugging
// ---------------------------------------------------------------------------
static void send_command_raw(modem_driver_config_t *config, const char *cmd)
{
    // TODO: implement
    ESP_LOGI(TAG, "TX: %s", cmd);
}

// ---------------------------------------------------------------------------
//  read_response
//
//  Reads bytes from UART RX until a known terminator line is found
//  ("OK", "ERROR", "CONNECT", "NO CARRIER", or timeout).
//
//  TODO:
//    1. Read one byte at a time using uart_read_bytes() in a loop
//    2. After each byte, check if buffer contains any terminator:
//       "OK", "ERROR", "CONNECT", or "NO CARRIER"
//    3. If found, null-terminate and return the total length
//    4. If timeout expires, return -1
//
//  Important: if echo is still ON (before ATE0 is sent), the modem echoes
//  your command back BEFORE sending the actual response. So the buffer
//  will contain:
//    "AT+CPIN?\r\n"              ← echoed command
//    "\r\n+CPIN: READY\r\nOK\r\n" ← actual response
//  Don't try to parse until a terminator is found — the echo bytes are
//  harmless as long as you scan the whole buffer with strstr().
//
//  After ATE0 succeeds, echo stops and you'll only see the response.
//
//  Hint: modem responses often span multiple lines, e.g.:
//    "\r\n+CREG: 0,1\r\nOK\r\n"
//  You want to capture everything up to and including the "OK".
// ---------------------------------------------------------------------------
static int read_response(modem_driver_config_t *config,
                         char *buf, size_t buf_size,
                         TickType_t timeout)
{
    // TODO: implement
    return -1;
}

// ---------------------------------------------------------------------------
//  response_contains
//
//  Utility: returns true if the response buffer contains the given substring.
//  Use this to check for "OK", "+CPIN: READY", "+CREG: 0,1", etc.
// ---------------------------------------------------------------------------
static bool response_contains(const char *response, const char *needle)
{
    return (response != NULL) && (strstr(response, needle) != NULL);
}


// ============================================================================
//  PUBLIC API
// ============================================================================

void modem_driver_init(modem_driver_config_t *config)
{
    // TODO:
    //   1. Set config->state = MODEM_DRIVER_IDLE
    //   2. Populate uart_config_t:
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

    ESP_LOGI(TAG, "modem_driver_init: uart=%d tx=%d rx=%d baud=%d",
             config->uart_num, config->tx_pin, config->rx_pin,
             config->baud_rate);
}

int modem_send_at(modem_driver_config_t *config,
                  const char *cmd,
                  char *resp_buf, size_t resp_buf_size,
                  TickType_t timeout)
{
    // TODO:
    //   1. Flush any stale data in the UART RX buffer (uart_flush_input)
    //   2. Call send_command_raw() to transmit the command
    //   3. Call read_response() to receive the reply
    //   4. Log the response for debugging
    //   5. Return bytes read, or -1 on timeout

    return -1;
}

int modem_check_sim(modem_driver_config_t *config)
{
    char resp[256];

    // Suggested timeout for AT commands: 2 seconds (pdMS_TO_TICKS(2000)).
    // The modem sim boots in 1.5s and the task waits 3s, so this is plenty.
    // On real hardware, some commands (CGACT, ATD) need longer — see those TODOs.
    TickType_t at_timeout = pdMS_TO_TICKS(2000);

    // TODO:
    //   Step 1 — Ping the modem with "AT"
    //     - Call modem_send_at(config, "AT", resp, sizeof(resp), at_timeout)
    //     - If response_contains(resp, "OK") → modem is alive
    //     - If timeout, retry up to 5 times with 1s delays (modem booting)
    //     - If still no response after retries, return -1

    // TODO:
    //   Step 2 — Disable echo with "ATE0"
    //     - Call modem_send_at(config, "ATE0", resp, sizeof(resp), at_timeout)
    //     - Expect "OK"
    //     - After this succeeds, the modem stops echoing commands back.
    //       Subsequent responses will NOT be prefixed with the echoed command.
    //       read_response() handles both cases (with/without echo) because it
    //       scans the whole buffer for terminators regardless.

    // TODO:
    //   Step 3 — Check SIM with "AT+CPIN?"
    //     - Call modem_send_at(config, "AT+CPIN?", resp, sizeof(resp), at_timeout)
    //     - Check response_contains(resp, "+CPIN: READY")
    //     - If ready: set state = MODEM_DRIVER_SIM_OK, return 0
    //     - If not: log error, set state = MODEM_DRIVER_ERROR, return -1
    //     - On real hardware, may get "+CPIN: SIM PIN" (needs PIN) or
    //       "+CME ERROR: 10" (no SIM inserted) — handle as fatal

    (void)resp;
    return -1;
}

int modem_register_network(modem_driver_config_t *config)
{
    char resp[256];

    TickType_t at_timeout = pdMS_TO_TICKS(2000);

    // TODO:
    //   Step 1 — Check signal quality
    //     - Send modem_send_at(config, "AT+CSQ", resp, sizeof(resp), at_timeout)
    //     - Parse "+CSQ: <rssi>,<ber>" from response
    //       Hint: use sscanf(strstr(resp, "+CSQ:"), "+CSQ: %d,%d", &rssi, &ber)
    //     - Log: "Signal: RSSI=%d, BER=%d"
    //     - If rssi == 99 (unknown), signal is bad — may want to retry

    // TODO:
    //   Step 2 — Poll registration status (with retries)
    //     - Send modem_send_at(config, "AT+CREG?", resp, sizeof(resp), at_timeout)
    //     - Check for "+CREG: 0,1" (registered home) or "+CREG: 0,5" (roaming)
    //     - If "+CREG: 0,2" (searching), wait 2s and retry (up to 10 times)
    //     - If "+CREG: 0,3" (denied), return -1 immediately

    // TODO:
    //   Step 3 — Query operator
    //     - Send "AT+COPS?"
    //     - Log the operator name from the response

    // TODO:
    //   Step 4 — On success:
    //     - Set state = MODEM_DRIVER_REGISTERED
    //     - printf() to console: "Registered on network"
    //     - Return 0

    (void)resp;
    return -1;
}

int modem_activate_pdp(modem_driver_config_t *config)
{
    char resp[256];

    // PDP activation can take 5-30s on real networks. Use a longer timeout.
    TickType_t at_timeout = pdMS_TO_TICKS(2000);
    TickType_t pdp_timeout = pdMS_TO_TICKS(30000);

    // TODO:
    //   Step 1 — Define PDP context
    //     - Send modem_send_at(config, "AT+CGDCONT=1,\"IP\",\"internet\"",
    //                          resp, sizeof(resp), at_timeout)
    //     - Expect "OK"

    // TODO:
    //   Step 2 — Activate PDP context
    //     - Send modem_send_at(config, "AT+CGACT=1,1",
    //                          resp, sizeof(resp), pdp_timeout)
    //     - Expect "OK"
    //     - On success: set state = MODEM_DRIVER_PDP_ACTIVE, return 0
    //     - On failure: return -1
    //     - On real hardware, may get "+CME ERROR: 30" (no network) or
    //       "+CME ERROR: 33" (not subscribed) — handle as fatal

    (void)resp;
    return -1;
}

int modem_enter_data_mode(modem_driver_config_t *config)
{
    char resp[256];

    // Dial can take 5-10s on real modems while network-side PPP sets up.
    TickType_t dial_timeout = pdMS_TO_TICKS(10000);

    // TODO:
    //   Step 1 — Request PPP data mode
    //     - Send modem_send_at(config, "ATD*99#",
    //                          resp, sizeof(resp), dial_timeout)
    //     - Check for "CONNECT" in the response (NOT "OK")
    //     - If response_contains(resp, "NO CARRIER"): PDP dropped, return -1
    //     - If timeout: modem may be stuck, return -1

    // TODO:
    //   Step 2 — On success:
    //     - Set state = MODEM_DRIVER_DATA_MODE
    //     - printf() to console: "PPP data mode established"
    //     - Return 0
    //     - After this, UART is no longer AT — it carries PPP frames

    (void)resp;
    return -1;
}

void modem_driver_task(void *param)
{
    modem_driver_config_t *config = (modem_driver_config_t *)param;

    // -----------------------------------------------------------------------
    // Step 1: Wait for modem to boot
    //   TODO:
    //     - Delay 3 seconds (give sim_modem_task time to start + power-on)
    //     - printf() to console: "Driver: connecting to modem..."
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Step 2: Run the connection sequence
    //   TODO:
    //     a. Call modem_check_sim()
    //        - On success: printf("SIM OK")
    //        - On failure: printf("SIM check failed"), goto error
    //
    //     b. Call modem_register_network()
    //        - On success: printf("Registered on network")
    //        - On failure: printf("Registration failed"), goto error
    //
    //     c. Call modem_activate_pdp()
    //        - On success: printf("PDP context active")
    //        - On failure: printf("PDP activation failed"), goto error
    //
    //     d. Call modem_enter_data_mode()
    //        - On success: printf("PPP link up")
    //        - On failure: printf("Data mode failed"), goto error
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Step 3: Hand off to PPP (Phase 2)
    //   TODO (implement later):
    //     - After modem_enter_data_mode() returns 0, the UART now carries
    //       binary PPP frames. No more AT commands can be sent.
    //     - To bring up IP connectivity, use ESP-IDF's PPP netif:
    //         1. Create esp_netif with ESP_NETIF_DEFAULT_PPP()
    //         2. Create an esp_modem DTE on this UART
    //         3. Call esp_modem_set_mode(dce, ESP_MODEM_MODE_PPP)
    //         4. lwIP handles LCP/IPCP negotiation automatically
    //         5. Wait for the NETIF_PPP_STATUS event (IP_EVENT_PPP_GOT_IP)
    //         6. Once IP is assigned, standard socket APIs work:
    //            - getaddrinfo() for DNS
    //            - connect() / send() / recv() for TCP
    //            - sendto() / recvfrom() for UDP
    //     - Alternatively (simpler for testing against the sim):
    //         Use the UART directly and send raw PPP frames yourself.
    //         The sim_modem handles LCP/IPCP/ICMP/DNS/TCP at the byte level.
    //     - See ESP-IDF examples: examples/protocols/pppos_client
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    // Step 4: Done (or error)
    //   TODO:
    //     - On success: printf("=== All layers connected ===")
    //     - On error: printf("=== Connection failed at step X ===")
    //     - Sit in a delay loop (task must not return)
    // -----------------------------------------------------------------------

    ESP_LOGI(TAG, "modem_driver_task started");

    // TODO: implement the flow above
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
