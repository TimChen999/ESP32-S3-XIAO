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
    // TODO: LLM handles this
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
    // TODO: LLM handles this
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
    // 1. Set initial state to IDLE so the task knows no commands have been
    //    sent yet. The task will advance state through the AT sequence.
    config->state = MODEM_DRIVER_IDLE;

    // 2. Describe the UART's electrical parameters: baud rate, word format,
    //    and flow control. Must match what the modem on the other end expects.
    //    8N1 (8 data bits, no parity, 1 stop bit) is universal for AT modems.
    uart_config_t uart_cfg = {
        .baud_rate           = config->baud_rate,
        .data_bits           = UART_DATA_8_BITS,
        .parity              = UART_PARITY_DISABLE,
        .stop_bits           = UART_STOP_BITS_1,
        .flow_ctrl           = config->flow_control
                                   ? UART_HW_FLOWCTRL_CTS_RTS
                                   : UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    // 3. Apply the config to the UART peripheral registers. After this,
    //    the hardware knows the baud rate and framing but has no pin
    //    assignments or software buffers yet.
    uart_param_config(config->uart_num, &uart_cfg);

    // 4. Route the UART's TX, RX, RTS, and CTS signals to physical GPIO pins
    //    via the GPIO matrix. When flow control is off, rts_pin and cts_pin
    //    are -1, so those signals stay unconnected.
    uart_set_pin(config->uart_num,
                 config->tx_pin, config->rx_pin,
                 config->rts_pin, config->cts_pin);

    // 5. Install the UART driver: allocate RX and TX ring buffers (1024-byte)
    //    so uart_read_bytes() / uart_write_bytes() can be used by
    //    send_command_raw() and read_response().
    uart_driver_install(config->uart_num, 1024, 1024, 0, NULL, 0);

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

// Called 3 second after task start
int modem_check_sim(modem_driver_config_t *config)
{
    char resp[256];

    // First AT command should have longer timeout
    TickType_t at_timeout = pdMS_TO_TICKS(4000);

    //   Step 1 — Ping the modem with "AT"
    //     - Call modem_send_at(config, "AT", resp, sizeof(resp), at_timeout)
    //     - If response_contains(resp, "OK") → modem is alive
    //     - If timeout, retry up to 5 times with 1s delays (modem booting)
    //     - If still no response after retries, return -1
    for loop try 5 times
        call modem send with AT command to modem
        parse response, handle echo if needed (this should also be done in modem_send_at)
        if response is OK
            log successful initial ping
            success = true
            break loop
        if Timeout
            log timeout, continue
        if random cfun
            ignore, continue
        if garbled or other error (rx buffer flushed by sending function (modem_send_at)
            log error, continue 
        wait a second
    
    if success is not true
        return -1

    // Set timeouts for basic AT commands to 2 seconds (not including CGACT, ATD)
    TickType_t at_timeout = pdMS_TO_TICKS(2000);

    //   Step 2 — Disable echo with "ATE0"
    //     - Call modem_send_at(config, "ATE0", resp, sizeof(resp), at_timeout)
    //     - Expect "OK", if it keeps failing, continue with echo handling
    //     - After this succeeds, the modem stops echoing commands back.
    //       Subsequent responses will NOT be prefixed with the echoed command.
    //       read_response() handles both cases (with/without echo) because it
    //       scans the whole buffer for terminators regardless.
    echohandling = true (This could be a global so modem_send_at can see it)
    for loop try 3 times 
        call modem send with ate0 
        parse response
        if ok 
            echohandling = false
            log success and break
        if garbled or other error
            log error and continue 
        wait a second

    if echohandling is true (after 3 attempts) 
        log echo disable failed

    // TODO:
    //   Step 3 — Check SIM with "AT+CPIN?"
    //     - Call modem_send_at(config, "AT+CPIN?", resp, sizeof(resp), at_timeout)
    //     - Check response_contains(resp, "+CPIN: READY")
    //     - If ready: set state = MODEM_DRIVER_SIM_OK, return 0
    //     - If not: log error, set state = MODEM_DRIVER_ERROR, return -1
    //     - On real hardware, may get "+CPIN: SIM PIN" (needs PIN) or
    //       "+CME ERROR: 10" (no SIM inserted) — handle as fatal
    for loop try 3 times
        send AT+CPIN? 
        parse received data, handle echo if needed 
        if ready 
            log ready 
            return 0 
        if SIM PIN 
            (For the LLM: If a config file does not exist, create a config file with a field for sim pin)
            if pin unknown
                log pin unknown 
                return -1 
            send AT+CPIN="insert pin" 
            parse received data, handle echo if needed
            if success 
                log ready 
                return 0
        if SIM PUK 
            log error
            fatal
        if CME ERROR 10 
            log sim not inserted 
            fatal 
        if CME ERROR 13 
            log sim no work 
            fatal
        if garbled or other error
            log garbled, continue 
        wait a second

    (void)resp;
    log did not work after 3 tries
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
    implement as is, log RSSI and BER value

    // TODO:
    //   Step 2 — Poll registration status (with retries)
    //     - Send modem_send_at(config, "AT+CREG?", resp, sizeof(resp), at_timeout)
    //     - Check for "+CREG: 0,1" (registered home) or "+CREG: 0,5" (roaming)
    //     - If "+CREG: 0,2" (searching), wait 2s and retry (up to 10 times)
    //     - If "+CREG: 0,3" (denied), return -1 immediately
    send CREG? command
    parse creg command 
    for loop repeat 10 times
        if creg 01 or 05 
            log creg number and registration success 
            return 0 
        if creg 02 
            log waiting 
            wait 2 seconds 
            continue 
        if creg 03 
            log denied 
            fatal return -1
    log tried 10 times and failed 
    return -1

    // TODO:
    //   Step 3 — Query operator
    //     - Send "AT+COPS?"
    //     - Log the operator name from the response
    do instruction above

    // TODO:
    //   Step 4 — On success:
    //     - Set state = MODEM_DRIVER_REGISTERED
    //     - printf() to console: "Registered on network"
    //     - Return 0 
    do instruction above

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
    // Add comment: Here, define just says that there are 2 contexts, IP and Internet
    do step above, log error and fatal if it errors

    // TODO:
    //   Step 2 — Activate PDP context
    //     - Send modem_send_at(config, "AT+CGACT=1,1",
    //                          resp, sizeof(resp), pdp_timeout)
    //     - Expect "OK"
    //     - On success: set state = MODEM_DRIVER_PDP_ACTIVE, return 0
    //     - On failure: return -1
    //     - On real hardware, may get "+CME ERROR: 30" (no network) or
    //       "+CME ERROR: 33" (not subscribed) — handle as fatal 
    do step above, if -1, 30, or 33 handle as described, and log error
    if 148, retry and then if it fails again, log and return -1

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
    do as above 

    // TODO:
    //   Step 2 — On success:
    //     - Set state = MODEM_DRIVER_DATA_MODE
    //     - printf() to console: "PPP data mode established"
    //     - Return 0
    //     - After this, UART is no longer AT — it carries PPP frames 
    do as above 

    (void)resp;
    return -1;
}

void modem_driver_task(void *param)
{
    modem_driver_config_t *config = (modem_driver_config_t *)param;

    ESP_LOGI(TAG, "modem_driver_task started");

    // -----------------------------------------------------------------------
    // Step 1: Wait for modem to boot.
    // The sim_modem_task boots in 1.5s. We wait 3s to be safe — on real
    // hardware, modems can take 5-15s. modem_check_sim() retries handle
    // the case where we start too early.
    // -----------------------------------------------------------------------
    printf("Driver: waiting 3 seconds for modem to boot...\n");
    vTaskDelay(pdMS_TO_TICKS(3000));
    printf("Driver: connecting to modem...\n");

    // -----------------------------------------------------------------------
    // Step 2: Run the connection sequence.
    // Each function returns 0 on success, -1 on failure.
    // On failure, skip remaining steps and report which step failed.
    // -----------------------------------------------------------------------

    // Step 2a: Verify modem is alive and SIM is ready.
    // Sends: AT, ATE0, AT+CPIN? — expects OK and +CPIN: READY.
    if (modem_check_sim(config) != 0) {
        printf("=== Connection failed: SIM check ===\n");
        goto error;
    }
    printf("Driver: SIM OK\n");

    // Step 2b: Register on the cellular network.
    // Sends: AT+CSQ, AT+CREG?, AT+COPS? — waits for registration.
    if (modem_register_network(config) != 0) {
        printf("=== Connection failed: network registration ===\n");
        goto error;
    }
    printf("Driver: Registered on network\n");

    // Step 2c: Activate PDP context (data bearer).
    // Sends: AT+CGDCONT, AT+CGACT — establishes IP path through carrier.
    if (modem_activate_pdp(config) != 0) {
        printf("=== Connection failed: PDP activation ===\n");
        goto error;
    }
    printf("Driver: PDP context active\n");

    // Step 2d: Switch UART from AT text to PPP binary.
    // Sends: ATD*99# — expects CONNECT. After this, no more AT commands.
    if (modem_enter_data_mode(config) != 0) {
        printf("=== Connection failed: data mode ===\n");
        goto error;
    }
    printf("Driver: PPP link up\n");

    // -----------------------------------------------------------------------
    // Step 3: Hand off to PPP (Phase 2 — implement later).
    // After modem_enter_data_mode() returns 0, the UART now carries
    // binary PPP frames. No more AT commands can be sent.
    // To bring up IP connectivity, use ESP-IDF's PPP netif:
    //   1. Create esp_netif with ESP_NETIF_DEFAULT_PPP()
    //   2. Create an esp_modem DTE on this UART
    //   3. Call esp_modem_set_mode(dce, ESP_MODEM_MODE_PPP)
    //   4. lwIP handles LCP/IPCP negotiation automatically
    //   5. Wait for IP_EVENT_PPP_GOT_IP
    //   6. Standard socket APIs work (connect, send, recv)
    // Alternatively, send raw PPP frames to the sim_modem directly.
    // See ESP-IDF examples: examples/protocols/pppos_client
    // -----------------------------------------------------------------------

    printf("=== All AT layers connected — PPP Phase 2 not yet implemented ===\n");
    goto idle;

error:
    config->state = MODEM_DRIVER_ERROR;
    printf("Driver: stopped due to error (state=%d)\n", config->state);

idle:
    // Task must never return — sit in a delay loop.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
