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
//  │ "AT+CSQ"        │ Reply "+CSQ: 20,0\r\nOK"  (good signal)        │
//  │ "AT+CREG?"      │ If state >= SIM_READY:                         │
//  │                 │   Set state = REGISTERED                       │
//  │                 │   Reply "+CREG: 0,1\r\nOK"                     │
//  │ "AT+CEREG?"     │ Same as CREG — reply "+CEREG: 0,1\r\nOK"       │
//  │ "AT+COPS?"      │ Reply "+COPS: 0,0,\"Fake Cellular\"\r\nOK"     │
//  │ "AT+CGDCONT=…"  │ Reply "OK". (Store APN if you want to.)        │
//  │ "AT+CGACT=1,1"  │ If state >= REGISTERED:                        │
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

// ============================================================================
//  PPP HELPERS & HANDLERS
//
//  Phase 2 flow after ATD*99# enters DATA_MODE:
//    Step 1-2: Raw byte reading + HDLC frame collection (in handle_ppp_data_mode)
//    Step 3:   LCP negotiation    — handle_lcp_frame()
//    Step 4:   IPCP negotiation   — handle_ipcp_frame()
//    Step 5-6: IP packet routing  — handle_ipv4_packet()
//    Step 7:   +++ escape detect  — checked inline in handle_ppp_data_mode
// ============================================================================

// Simulated IP addresses assigned during IPCP
#define SIM_MODEM_IP    {10, 0, 0, 1}
#define SIM_PEER_IP     {10, 0, 0, 2}
#define SIM_DNS_IP      {10, 0, 0, 1}

// ---------------------------------------------------------------------------
//  send_ppp_frame
//
//  Wraps payload in 0x7E delimiters and writes to UART.
//  Reused by LCP, IPCP, and IPv4 response senders.
// ---------------------------------------------------------------------------
static void send_ppp_frame(sim_modem_config_t *config,
                           const unsigned char *frame, size_t len,
                           unsigned char ppp_flag)
{
    uart_write_bytes(config->uart_num, (const char *)&ppp_flag, 1);
    uart_write_bytes(config->uart_num, (const char *)frame, len);
    uart_write_bytes(config->uart_num, (const char *)&ppp_flag, 1);
}

// ---------------------------------------------------------------------------
//  Internet checksum (RFC 1071) — used for IP, ICMP, TCP, UDP headers.
// ---------------------------------------------------------------------------
static unsigned short ip_checksum(const unsigned char *data, size_t len)
{
    unsigned long sum = 0;
    while (len > 1) {
        sum += ((unsigned short)data[0] << 8) | data[1];
        data += 2;
        len -= 2;
    }
    if (len == 1) {
        sum += (unsigned short)data[0] << 8;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (unsigned short)(~sum);
}

// ---------------------------------------------------------------------------
//  handle_lcp_frame  (Step 3)
//  After ATD*99# enters DATA_MODE, LCP is the first PPP control protocol.
//  Must complete before IPCP can assign IP addresses.
// ---------------------------------------------------------------------------
static void handle_lcp_frame(sim_modem_config_t *config,
                             const unsigned char *frame_buf,
                             size_t frame_len,
                             unsigned char ppp_flag)
{
    ESP_LOGI(TAG, "PPP frame complete: LCP (len=%u)", (unsigned int)frame_len);

    // Step 3: LCP negotiation (minimal implementation).
    // Parse LCP Config-Request from the driver and send:
    //   1) LCP Config-Ack (echo options back with code=2)
    //   2) Our own LCP Config-Request (MRU=1500)
    if (frame_len >= 8) {
        const unsigned char *lcp = &frame_buf[4];
        unsigned char lcp_code = lcp[0];
        unsigned char lcp_id = lcp[1];
        unsigned short lcp_len = ((unsigned short)lcp[2] << 8) | lcp[3];

        if (lcp_code == 0x01 && lcp_len >= 4 && (4U + lcp_len) <= frame_len) {
            unsigned char ack_frame[512];
            size_t ack_len = 4U + lcp_len;

            // Build LCP Config-Ack by echoing requester options.
            ack_frame[0] = 0xFF;
            ack_frame[1] = 0x03;
            ack_frame[2] = 0xC0;
            ack_frame[3] = 0x21;
            memcpy(&ack_frame[4], lcp, lcp_len);
            ack_frame[4] = 0x02; // Config-Ack

            send_ppp_frame(config, ack_frame, ack_len, ppp_flag);

            ESP_LOGI(TAG, "LCP Config-Ack sent (id=%u, len=%u)",
                     (unsigned int)lcp_id, (unsigned int)lcp_len);

            // Send our own LCP Config-Request (MRU option = 1500 / 0x05DC).
            static unsigned char local_lcp_id = 0x40;
            unsigned char req_frame[] = {
                0xFF, 0x03, 0xC0, 0x21,
                0x01, local_lcp_id++, 0x00, 0x08,
                0x01, 0x04, 0x05, 0xDC
            };

            send_ppp_frame(config, req_frame, sizeof(req_frame), ppp_flag);

            ESP_LOGI(TAG, "LCP Config-Request sent (id=%u, MRU=1500)",
                     (unsigned int)req_frame[5]);
        } else {
            ESP_LOGW(TAG, "LCP frame received but not a valid Config-Request");
        }
    } else {
        ESP_LOGW(TAG, "LCP frame too short for negotiation");
    }
}

// ---------------------------------------------------------------------------
//  handle_ipcp_frame  (Step 4)
//  After LCP opens, IPCP assigns IP addresses. This is the PPP equivalent
//  of DHCP — the modem tells the driver what IP to use.
// ---------------------------------------------------------------------------
static void handle_ipcp_frame(sim_modem_config_t *config,
                              const unsigned char *frame_buf,
                              size_t frame_len,
                              unsigned char ppp_flag)
{
    ESP_LOGI(TAG, "PPP frame complete: IPCP (len=%u)", (unsigned int)frame_len);

    if (frame_len < 8) {
        ESP_LOGW(TAG, "IPCP frame too short");
        return;
    }

    const unsigned char *ipcp = &frame_buf[4];
    unsigned char code = ipcp[0];
    unsigned char id   = ipcp[1];
    unsigned short ipcp_len = ((unsigned short)ipcp[2] << 8) | ipcp[3];

    if (code == 0x01 && ipcp_len >= 4 && (4U + ipcp_len) <= frame_len) {
        // IPCP Config-Request from driver.
        // Scan options for IP address (option type 0x03, length 6).
        const unsigned char peer_ip[] = SIM_PEER_IP;
        const unsigned char *opt = &ipcp[4];
        size_t opt_remaining = ipcp_len - 4;
        int ip_option_ok = 0;

        while (opt_remaining >= 2) {
            unsigned char opt_type = opt[0];
            unsigned char opt_len  = opt[1];
            if (opt_len < 2 || opt_len > opt_remaining) break;

            if (opt_type == 0x03 && opt_len == 6) {
                // IP-Address option: check if peer is requesting the IP we want to assign.
                if (memcmp(&opt[2], peer_ip, 4) == 0) {
                    ip_option_ok = 1;
                }
            }
            opt += opt_len;
            opt_remaining -= opt_len;
        }

        if (ip_option_ok) {
            // Peer requested the correct IP — send Config-Ack.
            unsigned char ack[512];
            size_t ack_len = 4U + ipcp_len;
            ack[0] = 0xFF; ack[1] = 0x03;
            ack[2] = 0x80; ack[3] = 0x21;
            memcpy(&ack[4], ipcp, ipcp_len);
            ack[4] = 0x02; // Config-Ack

            send_ppp_frame(config, ack, ack_len, ppp_flag);
            ESP_LOGI(TAG, "IPCP Config-Ack sent (peer IP 10.0.0.2 accepted)");
        } else {
            // Peer requested wrong/zero IP — send Config-Nak with the IP we want to assign.
            unsigned char nak[] = {
                0xFF, 0x03, 0x80, 0x21,
                0x03, id, 0x00, 0x0A,
                0x03, 0x06, peer_ip[0], peer_ip[1], peer_ip[2], peer_ip[3]
            };

            send_ppp_frame(config, nak, sizeof(nak), ppp_flag);
            ESP_LOGI(TAG, "IPCP Config-Nak sent (suggesting 10.0.0.2)");
        }

        // Send our own IPCP Config-Request with modem's IP and DNS.
        const unsigned char modem_ip[] = SIM_MODEM_IP;
        const unsigned char dns_ip[]   = SIM_DNS_IP;
        static unsigned char local_ipcp_id = 0x60;
        unsigned char req[] = {
            0xFF, 0x03, 0x80, 0x21,
            0x01, local_ipcp_id++, 0x00, 0x16,
            0x03, 0x06, modem_ip[0], modem_ip[1], modem_ip[2], modem_ip[3],
            0x81, 0x06, dns_ip[0],   dns_ip[1],   dns_ip[2],   dns_ip[3],
            0x83, 0x06, dns_ip[0],   dns_ip[1],   dns_ip[2],   dns_ip[3]
        };

        send_ppp_frame(config, req, sizeof(req), ppp_flag);
        ESP_LOGI(TAG, "IPCP Config-Request sent (modem=10.0.0.1, DNS=10.0.0.1)");

    } else if (code == 0x02) {
        // IPCP Config-Ack from driver — peer accepted our IP config.
        ESP_LOGI(TAG, "IPCP Config-Ack received — IP layer is up");
    } else {
        ESP_LOGW(TAG, "IPCP frame: unhandled code=0x%02X", code);
    }
}

// ---------------------------------------------------------------------------
//  handle_ipv4_packet  (Steps 5 & 6)
//  After IPCP opens, real IP packets arrive wrapped in PPP frames.
//  Extract IP header, dispatch by protocol: ICMP, UDP (DNS), TCP.
// ---------------------------------------------------------------------------
static void handle_ipv4_packet(sim_modem_config_t *config,
                               const unsigned char *frame_buf,
                               size_t frame_len,
                               unsigned char ppp_flag)
{
    // IP packet starts at frame_buf[4] (after PPP addr/ctrl/proto).
    const unsigned char *ip = &frame_buf[4];
    size_t ip_len = frame_len - 4;

    if (ip_len < 20) {
        ESP_LOGW(TAG, "IPv4 packet too short (%u bytes)", (unsigned int)ip_len);
        return;
    }

    unsigned char ip_hdr_len = (ip[0] & 0x0F) * 4;
    unsigned short total_len = ((unsigned short)ip[2] << 8) | ip[3];
    unsigned char protocol   = ip[9];
    // src IP = ip[12..15], dst IP = ip[16..19]

    ESP_LOGI(TAG, "IPv4 packet: proto=%u, len=%u, src=%u.%u.%u.%u, dst=%u.%u.%u.%u",
             protocol, total_len,
             ip[12], ip[13], ip[14], ip[15],
             ip[16], ip[17], ip[18], ip[19]);

    if (protocol == 1 && ip_len >= (size_t)(ip_hdr_len + 8)) {
        // Step 6a: ICMP — respond to echo request (ping).
        const unsigned char *icmp = &ip[ip_hdr_len];
        if (icmp[0] == 8) {
            // Build ICMP Echo Reply: swap src/dst IP, set type=0, fix checksums.
            unsigned char reply[512];
            size_t reply_ip_len = total_len;
            if (reply_ip_len > sizeof(reply) - 4) return;

            // PPP header for IPv4
            reply[0] = 0xFF; reply[1] = 0x03;
            reply[2] = 0x00; reply[3] = 0x21;

            memcpy(&reply[4], ip, reply_ip_len);
            unsigned char *rip = &reply[4];

            // Swap src and dst IP
            unsigned char tmp[4];
            memcpy(tmp, &rip[12], 4);
            memcpy(&rip[12], &rip[16], 4);
            memcpy(&rip[16], tmp, 4);

            // Zero IP checksum, recompute
            rip[10] = 0; rip[11] = 0;
            unsigned short ip_cksum = ip_checksum(rip, ip_hdr_len);
            rip[10] = (ip_cksum >> 8) & 0xFF;
            rip[11] = ip_cksum & 0xFF;

            // ICMP: set type=0 (echo reply), recompute checksum
            unsigned char *ricmp = &rip[ip_hdr_len];
            ricmp[0] = 0;  // type = Echo Reply
            ricmp[2] = 0; ricmp[3] = 0;  // zero checksum
            size_t icmp_len = reply_ip_len - ip_hdr_len;
            unsigned short icmp_cksum = ip_checksum(ricmp, icmp_len);
            ricmp[2] = (icmp_cksum >> 8) & 0xFF;
            ricmp[3] = icmp_cksum & 0xFF;

            send_ppp_frame(config, reply, 4 + reply_ip_len, ppp_flag);
            ESP_LOGI(TAG, "ICMP Echo Reply sent");
        }

    } else if (protocol == 17 && ip_len >= (size_t)(ip_hdr_len + 8)) {
        // Step 6b: UDP — check for DNS queries (dst port 53).
        const unsigned char *udp = &ip[ip_hdr_len];
        unsigned short dst_port = ((unsigned short)udp[2] << 8) | udp[3];

        if (dst_port == 53 && ip_len >= (size_t)(ip_hdr_len + 8 + 12)) {
            // Minimal DNS response: return 10.0.0.1 for any A-record query.
            const unsigned char *dns = &udp[8];
            unsigned short txn_id = ((unsigned short)dns[0] << 8) | dns[1];

            // Build a canned DNS response with one A record.
            const unsigned char peer_ip[] = SIM_MODEM_IP;
            unsigned char dns_resp[] = {
                (unsigned char)(txn_id >> 8), (unsigned char)(txn_id & 0xFF),
                0x81, 0x80,         // Flags: standard response, no error
                0x00, 0x01,         // Questions: 1
                0x00, 0x01,         // Answers: 1
                0x00, 0x00, 0x00, 0x00  // Authority/Additional: 0
            };

            // For simplicity, echo the query section and append a pointer answer.
            // Real DNS would copy the question + add answer RR. This is a stub.
            // TODO: copy actual question bytes and construct proper answer RR.

            ESP_LOGI(TAG, "DNS query received (txn=0x%04X) — stub response (not yet complete)",
                     txn_id);
            (void)dns_resp;
            (void)peer_ip;
        }

    } else if (protocol == 6 && ip_len >= (size_t)(ip_hdr_len + 20)) {
        // Step 6c: TCP — detect SYN and perform 3-way handshake stub.
        const unsigned char *tcp = &ip[ip_hdr_len];
        unsigned char tcp_flags = tcp[13];

        unsigned short src_port = ((unsigned short)tcp[0] << 8) | tcp[1];
        unsigned short dst_port = ((unsigned short)tcp[2] << 8) | tcp[3];
        unsigned long  seq_num  = ((unsigned long)tcp[4] << 24) | ((unsigned long)tcp[5] << 16)
                                | ((unsigned long)tcp[6] << 8)  | tcp[7];

        if (tcp_flags == 0x02) {
            // SYN received — send SYN-ACK.
            ESP_LOGI(TAG, "TCP SYN received: %u.%u.%u.%u:%u → :%u seq=%lu",
                     ip[12], ip[13], ip[14], ip[15],
                     src_port, dst_port, seq_num);

            // TODO: build SYN-ACK packet:
            //   - Swap src/dst IP and ports
            //   - Set flags = SYN+ACK (0x12)
            //   - ack_num = seq_num + 1
            //   - Pick our own seq_num (e.g. 5000)
            //   - Compute IP + TCP checksums
            //   - Wrap in PPP frame and send
            ESP_LOGI(TAG, "TCP SYN-ACK — stub (not yet complete)");

        } else if (tcp_flags & 0x01) {
            // FIN received.
            ESP_LOGI(TAG, "TCP FIN received from port %u", src_port);
            // TODO: send FIN-ACK back
        } else {
            ESP_LOGI(TAG, "TCP segment: flags=0x%02X src_port=%u dst_port=%u",
                     tcp_flags, src_port, dst_port);
        }

    } else {
        ESP_LOGI(TAG, "IPv4 protocol %u — not handled", protocol);
    }
}

static void handle_ppp_data_mode(sim_modem_config_t *config)
{
    // We keep this handler byte-oriented (not line-oriented) because PPP is
    // binary and may contain arbitrary bytes.
    static const unsigned char PPP_FLAG = 0x7E;
    unsigned char byte = 0;
    unsigned char frame_buf[512];
    size_t frame_len = 0;

    // Step 7: Track consecutive '+' bytes for escape detection.
    // Real modems require: 1s silence, "+++", 1s silence → back to AT mode.
    // We simplify: 3 consecutive '+' bytes with no other data in between.
    int plus_count = 0;

    ESP_LOGI(TAG, "Entered PPP data mode — collecting HDLC frames");

    while (config->state == SIM_MODEM_STATE_DATA_MODE) {
        // Step 1: Read raw bytes from UART (PPP is binary, not line-based).
        int got = uart_read_bytes(config->uart_num, &byte, 1, pdMS_TO_TICKS(200));
        if (got <= 0) {
            // Idle timeout: no bytes right now.
            // Step 7: if we saw 3 '+' and then silence, escape to AT mode.
            if (plus_count >= 3) {
                ESP_LOGI(TAG, "+++ escape detected — returning to AT command mode");
                config->state = SIM_MODEM_STATE_PDP_ACTIVE;
                return;
            }
            continue;
        }

        // Step 7: track '+' bytes for escape sequence.
        if (byte == '+') {
            plus_count++;
            if (plus_count < 3) continue;
            // Don't act on 3rd '+' yet — wait for silence (timeout above).
            continue;
        } else {
            plus_count = 0;
        }

        // Step 2: PPP frame delimiter 0x7E found (HDLC flag bytes).
        if (byte == PPP_FLAG) {
            // Consecutive flags are legal fill between frames.
            if (frame_len == 0) {
                continue;
            }

            // Frame boundary reached — we now have one complete PPP frame.
            // PPP header: Address(0xFF), Control(0x03), Protocol(2B).
            if (frame_len >= 4 && frame_buf[0] == 0xFF && frame_buf[1] == 0x03) {
                // Protocol field = frame_buf[2:4] (big-endian).
                // Determines which handler processes the payload.
                unsigned short protocol = ((unsigned short)frame_buf[2] << 8) | frame_buf[3];

                if (protocol == 0xC021) {
                    // Step 3: LCP — link parameter negotiation (must complete first).
                    handle_lcp_frame(config, frame_buf, frame_len, PPP_FLAG);

                } else if (protocol == 0x8021) {
                    // Step 4: IPCP — IP address assignment (after LCP opens).
                    handle_ipcp_frame(config, frame_buf, frame_len, PPP_FLAG);

                } else if (protocol == 0x0021) {
                    // Steps 5 & 6: IPv4 — real IP packets (after IPCP opens).
                    handle_ipv4_packet(config, frame_buf, frame_len, PPP_FLAG);

                } else {
                    ESP_LOGI(TAG, "PPP frame: unhandled proto=0x%04X (len=%u)",
                             protocol, (unsigned int)frame_len);
                }
            } else {
                ESP_LOGW(TAG, "PPP frame too short/unknown header (len=%u)",
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
