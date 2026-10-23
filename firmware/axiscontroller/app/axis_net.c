#include "axis_net.h"

#include "axis_console.h"
#include "axis_registers.h"

#include "lwip/apps/mdns.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

#include <stdio.h>
#include <string.h>

#ifndef AXIS_NET_COUNTRY
#define AXIS_NET_COUNTRY CYW43_COUNTRY_UK
#endif

enum {
    REG_CONTROL_WORD = 0x0010,
    REG_STATUS_WORD = 0x0011,
    CONTROL_STOP = 1u << 5,
    STATUS_MOVING = 1u << 2,
    STATUS_HOMING = 1u << 3,
};

enum {
    AXIS_NET_RX_BUFFER_SIZE = 1024,
    AXIS_NET_TX_BUFFER_SIZE = 8192,
    AXIS_NET_LINE_BUFFER_SIZE = 256,
    AXIS_NET_DRAIN_CHUNK = 256,
    AXIS_NET_LINK_POLL_US = 250000,
    AXIS_NET_CONNECT_TIMEOUT_US = 20000000,
    AXIS_NET_RETRY_DELAY_US = 5000000,
    AXIS_NET_EVENT_CLIENT_CONNECTED = 1u << 0,
    AXIS_NET_EVENT_CLIENT_LOST = 1u << 1,
    AXIS_NET_SCAN_MAX = 24,
};

typedef struct {
    char ssid[AXIS_SETTINGS_SSID_MAX + 1];
    int16_t rssi;
    uint16_t channel;
    uint8_t auth_mode;
} AxisNetScanEntry;

static const char k_service_type[] = "_roberta-axis";

static bool g_available;
static AxisNetState g_state;
static AxisNetSettings g_active;
static AxisNetSettings g_staged;
static char g_hostname[AXIS_NET_HOSTNAME_MAX + 1];
static bool g_mdns_started;
static uint64_t g_state_since_us;
static uint64_t g_next_link_poll_us;
static int g_link_status;

// Shared with lwIP callbacks, which run from the cyw43 background IRQ. The
// main loop only touches these between cyw43_arch_lwip_begin/end.
static struct tcp_pcb *g_listen_pcb;
static struct tcp_pcb *g_client_pcb;
static uint32_t g_events;
static ip_addr_t g_client_addr;
static uint8_t g_rx_buffer[AXIS_NET_RX_BUFFER_SIZE];
static size_t g_rx_head;
static size_t g_rx_tail;

// Main-loop only.
static uint8_t g_tx_buffer[AXIS_NET_TX_BUFFER_SIZE];
static size_t g_tx_head;
static size_t g_tx_tail;
static char g_line_buffer[AXIS_NET_LINE_BUFFER_SIZE];
static size_t g_line_length;
static bool g_line_overflowed;
static bool g_last_was_cr;
static bool g_client_connected;
static bool g_watchdog_armed;
static uint64_t g_last_rx_us;
static uint32_t g_watchdog_ms = AXIS_NET_DEFAULT_WATCHDOG_MS;
static uint32_t g_watchdog_trips;
static uint64_t g_led_next_us;
static bool g_led_on;

// Filled from the cyw43 scan callback; read by the main loop once the scan
// is no longer active.
static AxisNetScanEntry g_scan[AXIS_NET_SCAN_MAX];
static size_t g_scan_count;
static bool g_scan_pending;

static struct netif *sta_netif(void)
{
    return &cyw43_state.netif[CYW43_ITF_STA];
}

static void set_state(AxisNetState state, uint64_t now_us)
{
    g_state = state;
    g_state_since_us = now_us;
}

static void build_hostname(void)
{
    if (g_active.name[0] != '\0') {
        snprintf(g_hostname, sizeof(g_hostname), "roberta-%s", g_active.name);
        return;
    }

    char board_id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));
    snprintf(g_hostname, sizeof(g_hostname), "roberta-axis-%s",
             &board_id[strlen(board_id) - 4]);
}

// lwIP callbacks ------------------------------------------------------------

// Returns ERR_ABRT when the pcb had to be aborted, which a recv callback must
// pass back to lwIP.
static err_t detach_client(struct tcp_pcb *pcb)
{
    tcp_arg(pcb, NULL);
    tcp_recv(pcb, NULL);
    tcp_err(pcb, NULL);
    if (tcp_close(pcb) != ERR_OK) {
        tcp_abort(pcb);
        return ERR_ABRT;
    }
    return ERR_OK;
}

static void on_client_error(void *arg, err_t err)
{
    (void)err;

    // lwIP has already freed the pcb.
    if (arg != NULL && arg == g_client_pcb) {
        g_client_pcb = NULL;
        g_events |= AXIS_NET_EVENT_CLIENT_LOST;
    }
}

static err_t on_client_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    (void)arg;

    if (p == NULL || err != ERR_OK) {
        if (p != NULL) {
            pbuf_free(p);
        }
        if (pcb == g_client_pcb) {
            g_client_pcb = NULL;
            g_events |= AXIS_NET_EVENT_CLIENT_LOST;
        }
        return detach_client(pcb);
    }

    // Bytes that do not fit are dropped; commands are short and drained often.
    for (const struct pbuf *q = p; q != NULL; q = q->next) {
        const uint8_t *bytes = q->payload;
        for (u16_t index = 0; index < q->len; index++) {
            const size_t next = (g_rx_head + 1u) % AXIS_NET_RX_BUFFER_SIZE;
            if (next == g_rx_tail) {
                break;
            }
            g_rx_buffer[g_rx_head] = bytes[index];
            g_rx_head = next;
        }
    }

    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg;

    if (err != ERR_OK || pcb == NULL) {
        return ERR_VAL;
    }

    // One controlling client at a time; the newest connection wins so an app
    // can reconnect after a dropped link without waiting for a timeout.
    if (g_client_pcb != NULL) {
        (void)detach_client(g_client_pcb);
        g_events |= AXIS_NET_EVENT_CLIENT_LOST;
    }

    g_client_pcb = pcb;
    g_client_addr = pcb->remote_ip;
    g_rx_head = 0;
    g_rx_tail = 0;
    g_events |= AXIS_NET_EVENT_CLIENT_CONNECTED;

    tcp_arg(pcb, pcb);
    tcp_recv(pcb, on_client_recv);
    tcp_err(pcb, on_client_error);
    tcp_nagle_disable(pcb);
    return ERR_OK;
}

static void srv_txt(struct mdns_service *service, void *userdata)
{
    char item[8 + AXIS_SETTINGS_NAME_MAX];
    (void)userdata;

    snprintf(item, sizeof(item), "name=%s", g_active.name);
    mdns_resp_add_service_txtitem(service, item, (u8_t)strlen(item));
    mdns_resp_add_service_txtitem(service, "proto=console1", 14);
}

// Link management -----------------------------------------------------------

static void start_listening(void)
{
    struct tcp_pcb *pcb;

    cyw43_arch_lwip_begin();
    pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (pcb != NULL && tcp_bind(pcb, IP_ANY_TYPE, AXIS_NET_TCP_PORT) == ERR_OK) {
        g_listen_pcb = tcp_listen_with_backlog(pcb, 1);
        if (g_listen_pcb != NULL) {
            tcp_accept(g_listen_pcb, on_accept);
        }
    } else if (pcb != NULL) {
        tcp_close(pcb);
    }
    cyw43_arch_lwip_end();
}

static void start_mdns_or_announce(void)
{
    cyw43_arch_lwip_begin();
    if (!g_mdns_started) {
        mdns_resp_init();
        mdns_resp_add_netif(sta_netif(), g_hostname);
        mdns_resp_add_service(sta_netif(), g_hostname, k_service_type, DNSSD_PROTO_TCP,
                              AXIS_NET_TCP_PORT, srv_txt, NULL);
        g_mdns_started = true;
    } else {
        mdns_resp_announce(sta_netif());
    }
    cyw43_arch_lwip_end();
}

static void begin_connect(uint64_t now_us)
{
    if (!axis_settings_has_credentials(&g_active)) {
        set_state(AXIS_NET_STATE_NO_CREDENTIALS, now_us);
        return;
    }

    const uint32_t auth = g_active.password[0] != '\0' ? CYW43_AUTH_WPA2_MIXED_PSK : CYW43_AUTH_OPEN;
    const char *password = g_active.password[0] != '\0' ? g_active.password : NULL;

    if (cyw43_arch_wifi_connect_async(g_active.ssid, password, auth) == 0) {
        set_state(AXIS_NET_STATE_CONNECTING, now_us);
    } else {
        set_state(AXIS_NET_STATE_RETRY_WAIT, now_us);
    }
}

static void restart_link(uint64_t now_us)
{
    cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    build_hostname();

    cyw43_arch_lwip_begin();
    netif_set_hostname(sta_netif(), g_hostname);
    if (g_mdns_started) {
        mdns_resp_rename_netif(sta_netif(), g_hostname);
        mdns_resp_rename_service(sta_netif(), 0, g_hostname);
    }
    cyw43_arch_lwip_end();

    begin_connect(now_us);
}

// Watchdog ------------------------------------------------------------------

static void trip_watchdog(const char *reason)
{
    uint32_t status_word = 0;

    g_watchdog_armed = false;
    axis_registers_read(REG_STATUS_WORD, &status_word);
    if ((status_word & (STATUS_MOVING | STATUS_HOMING)) == 0) {
        return;
    }

    // Stop motion but keep the drive enabled so the joint holds position.
    axis_registers_bit_set(REG_CONTROL_WORD, CONTROL_STOP);
    g_watchdog_trips++;
    printf("net watchdog: %s, motion stopped\r\n", reason);
}

static void poll_watchdog(uint64_t now_us)
{
    if (!g_watchdog_armed || g_watchdog_ms == 0) {
        return;
    }

    if (now_us - g_last_rx_us > (uint64_t)g_watchdog_ms * 1000u) {
        trip_watchdog("client silent");
    }
}

// Line handling -------------------------------------------------------------

static void net_console_write(const char *text, size_t length)
{
    axis_net_write(text, length);
}

static void consume_byte(uint8_t ch)
{
    if (ch == '\n' && g_last_was_cr) {
        g_last_was_cr = false;
        return;
    }
    g_last_was_cr = ch == '\r';

    if (ch == '\r' || ch == '\n') {
        if (g_line_overflowed) {
            axis_net_write("line-too-long\r\naxis> ", 21);
        } else {
            g_line_buffer[g_line_length] = '\0';
            axis_console_handle_line(g_line_buffer, net_console_write);
        }
        g_line_length = 0;
        g_line_overflowed = false;
        return;
    }

    if (g_line_length + 1 < sizeof(g_line_buffer)) {
        g_line_buffer[g_line_length++] = (char)ch;
    } else {
        g_line_overflowed = true;
    }
}

static void drain_rx(uint64_t now_us)
{
    uint8_t chunk[AXIS_NET_DRAIN_CHUNK];

    while (true) {
        size_t count = 0;

        cyw43_arch_lwip_begin();
        while (count < sizeof(chunk) && g_rx_tail != g_rx_head) {
            chunk[count++] = g_rx_buffer[g_rx_tail];
            g_rx_tail = (g_rx_tail + 1u) % AXIS_NET_RX_BUFFER_SIZE;
        }
        cyw43_arch_lwip_end();

        if (count == 0) {
            return;
        }

        g_last_rx_us = now_us;
        g_watchdog_armed = true;
        for (size_t index = 0; index < count; index++) {
            consume_byte(chunk[index]);
        }
    }
}

static void flush_tx(void)
{
    cyw43_arch_lwip_begin();

    if (g_client_pcb == NULL) {
        g_tx_head = 0;
        g_tx_tail = 0;
        cyw43_arch_lwip_end();
        return;
    }

    bool wrote = false;
    while (g_tx_tail != g_tx_head) {
        const size_t contiguous = g_tx_head > g_tx_tail
            ? g_tx_head - g_tx_tail
            : AXIS_NET_TX_BUFFER_SIZE - g_tx_tail;
        const size_t space = tcp_sndbuf(g_client_pcb);
        const size_t length = contiguous < space ? contiguous : space;

        if (length == 0 ||
            tcp_write(g_client_pcb, &g_tx_buffer[g_tx_tail], (u16_t)length, TCP_WRITE_FLAG_COPY) != ERR_OK) {
            break;
        }

        g_tx_tail = (g_tx_tail + length) % AXIS_NET_TX_BUFFER_SIZE;
        wrote = true;
    }

    if (wrote) {
        tcp_output(g_client_pcb);
    }

    cyw43_arch_lwip_end();
}

static void handle_events(void)
{
    uint32_t events;
    ip_addr_t client_addr;

    cyw43_arch_lwip_begin();
    events = g_events;
    g_events = 0;
    client_addr = g_client_addr;
    cyw43_arch_lwip_end();

    if ((events & AXIS_NET_EVENT_CLIENT_LOST) != 0) {
        g_client_connected = false;
        if (g_watchdog_armed) {
            trip_watchdog("client disconnected");
        }
    }

    if ((events & AXIS_NET_EVENT_CLIENT_CONNECTED) != 0) {
        g_client_connected = true;
        g_watchdog_armed = false;
        g_line_length = 0;
        g_line_overflowed = false;
        g_last_was_cr = false;
        g_tx_head = 0;
        g_tx_tail = 0;
        printf("net client connected from %s\r\n", ipaddr_ntoa(&client_addr));
        axis_net_write("axiscontroller ready\r\naxis> ", 28);
    }
}

static void poll_link(uint64_t now_us)
{
    if (now_us < g_next_link_poll_us) {
        return;
    }
    g_next_link_poll_us = now_us + AXIS_NET_LINK_POLL_US;

    g_link_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);

    switch (g_state) {
    case AXIS_NET_STATE_CONNECTING:
        if (g_link_status == CYW43_LINK_UP) {
            set_state(AXIS_NET_STATE_CONNECTED, now_us);
            start_mdns_or_announce();
            printf("net connected %s as %s.local port %d\r\n",
                   ip4addr_ntoa(netif_ip4_addr(sta_netif())), g_hostname, AXIS_NET_TCP_PORT);
        } else if (g_link_status < 0 || now_us - g_state_since_us > AXIS_NET_CONNECT_TIMEOUT_US) {
            printf("net connect failed (link %d), retrying\r\n", g_link_status);
            cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
            set_state(AXIS_NET_STATE_RETRY_WAIT, now_us);
        }
        break;

    case AXIS_NET_STATE_CONNECTED:
        if (g_link_status != CYW43_LINK_UP) {
            printf("net link lost (link %d)\r\n", g_link_status);
            if (g_watchdog_armed) {
                trip_watchdog("wifi link lost");
            }
            cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
            set_state(AXIS_NET_STATE_RETRY_WAIT, now_us);
        }
        break;

    case AXIS_NET_STATE_RETRY_WAIT:
        if (!g_scan_pending && now_us - g_state_since_us > AXIS_NET_RETRY_DELAY_US) {
            begin_connect(now_us);
        }
        break;

    case AXIS_NET_STATE_UNAVAILABLE:
    case AXIS_NET_STATE_NO_CREDENTIALS:
        break;
    }
}

// Status LED: off without credentials, fast blink while joining, short
// heartbeat when connected, solid while a client is attached.
static void poll_led(uint64_t now_us)
{
    bool on;

    if (now_us < g_led_next_us) {
        return;
    }
    g_led_next_us = now_us + 50000u;

    const uint64_t phase_ms = (now_us / 1000u) % 1000u;
    switch (g_state) {
    case AXIS_NET_STATE_CONNECTED:
        on = g_client_connected || phase_ms < 100u;
        break;
    case AXIS_NET_STATE_CONNECTING:
    case AXIS_NET_STATE_RETRY_WAIT:
        on = (phase_ms / 125u) % 2u == 0u;
        break;
    default:
        on = false;
        break;
    }

    if (on != g_led_on) {
        g_led_on = on;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
    }
}

// Scan ----------------------------------------------------------------------

static int on_scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
    (void)env;

    if (result == NULL || result->ssid_len == 0) {
        return 0;
    }

    // One entry per SSID, keeping the strongest access point.
    for (size_t index = 0; index < g_scan_count; index++) {
        AxisNetScanEntry *entry = &g_scan[index];
        if (strlen(entry->ssid) == result->ssid_len &&
            memcmp(entry->ssid, result->ssid, result->ssid_len) == 0) {
            if (result->rssi > entry->rssi) {
                entry->rssi = result->rssi;
                entry->channel = result->channel;
                entry->auth_mode = result->auth_mode;
            }
            return 0;
        }
    }

    if (g_scan_count < AXIS_NET_SCAN_MAX) {
        AxisNetScanEntry *entry = &g_scan[g_scan_count++];
        const size_t length = result->ssid_len < AXIS_SETTINGS_SSID_MAX ? result->ssid_len : AXIS_SETTINGS_SSID_MAX;
        memcpy(entry->ssid, result->ssid, length);
        entry->ssid[length] = '\0';
        entry->rssi = result->rssi;
        entry->channel = result->channel;
        entry->auth_mode = result->auth_mode;
    }
    return 0;
}

static void poll_scan(void)
{
    if (!g_scan_pending || cyw43_wifi_scan_active(&cyw43_state)) {
        return;
    }

    g_scan_pending = false;
    printf("net scan: %u networks (2.4 GHz only)\r\n", (unsigned)g_scan_count);
    for (size_t index = 0; index < g_scan_count; index++) {
        const AxisNetScanEntry *entry = &g_scan[index];
        printf("  ch %2u  %4d dBm  %-6s  \"%s\"\r\n",
               entry->channel,
               entry->rssi,
               entry->auth_mode == 0 ? "open" : "secure",
               entry->ssid);
    }
    printf("axis> ");
}

// Public API ----------------------------------------------------------------

void axis_net_init(void)
{
    const uint64_t now_us = time_us_64();

    axis_settings_load(&g_active);
    g_staged = g_active;
    build_hostname();

    if (cyw43_arch_init_with_country(AXIS_NET_COUNTRY) != 0) {
        g_available = false;
        set_state(AXIS_NET_STATE_UNAVAILABLE, now_us);
        printf("net unavailable: cyw43 init failed\r\n");
        return;
    }

    g_available = true;
    cyw43_arch_enable_sta_mode();
    // The default power-save mode adds 100-300 ms of receive latency.
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);

    cyw43_arch_lwip_begin();
    netif_set_hostname(sta_netif(), g_hostname);
    cyw43_arch_lwip_end();

    start_listening();
    begin_connect(now_us);
}

void axis_net_poll(uint64_t now_us)
{
    if (!g_available) {
        return;
    }

    poll_link(now_us);
    handle_events();
    drain_rx(now_us);
    poll_watchdog(now_us);
    flush_tx();
    poll_led(now_us);
    poll_scan();
}

void axis_net_get_status(AxisNetStatus *status)
{
    memset(status, 0, sizeof(*status));
    status->state = g_state;
    status->link_status = g_link_status;
    status->client_connected = g_client_connected;
    status->watchdog_ms = g_watchdog_ms;
    status->watchdog_trips = g_watchdog_trips;
    snprintf(status->hostname, sizeof(status->hostname), "%s", g_hostname);
    snprintf(status->ssid, sizeof(status->ssid), "%s", g_active.ssid);

    if (!g_available) {
        return;
    }

    cyw43_arch_lwip_begin();
    if (g_state == AXIS_NET_STATE_CONNECTED) {
        snprintf(status->ip, sizeof(status->ip), "%s", ip4addr_ntoa(netif_ip4_addr(sta_netif())));
    }
    if (g_client_pcb != NULL) {
        snprintf(status->client_ip, sizeof(status->client_ip), "%s", ipaddr_ntoa(&g_client_addr));
    }
    cyw43_arch_lwip_end();
}

bool axis_net_read_rssi(int32_t *rssi)
{
    return g_available && g_state == AXIS_NET_STATE_CONNECTED &&
           cyw43_wifi_get_rssi(&cyw43_state, rssi) == 0;
}

const char *axis_net_state_to_string(AxisNetState state)
{
    switch (state) {
    case AXIS_NET_STATE_UNAVAILABLE:
        return "unavailable";
    case AXIS_NET_STATE_NO_CREDENTIALS:
        return "no-credentials";
    case AXIS_NET_STATE_CONNECTING:
        return "connecting";
    case AXIS_NET_STATE_CONNECTED:
        return "connected";
    case AXIS_NET_STATE_RETRY_WAIT:
        return "retry-wait";
    }

    return "unknown";
}

AxisNetSettings *axis_net_staged_settings(void)
{
    return &g_staged;
}

bool axis_net_apply_settings(bool persist)
{
    if (persist && !axis_settings_save(&g_staged)) {
        return false;
    }

    g_active = g_staged;
    if (g_available) {
        restart_link(time_us_64());
    }
    return true;
}

bool axis_net_forget_settings(void)
{
    if (!axis_settings_erase()) {
        return false;
    }

    memset(&g_staged, 0, sizeof(g_staged));
    g_active = g_staged;
    if (g_available) {
        restart_link(time_us_64());
    }
    return true;
}

bool axis_net_start_scan(void)
{
    cyw43_wifi_scan_options_t options;

    if (!g_available || g_scan_pending) {
        return false;
    }

    memset(&options, 0, sizeof(options));
    g_scan_count = 0;
    g_scan_pending = cyw43_wifi_scan(&cyw43_state, &options, NULL, on_scan_result) == 0;
    return g_scan_pending;
}

void axis_net_set_watchdog_ms(uint32_t timeout_ms)
{
    g_watchdog_ms = timeout_ms;
}

void axis_net_write(const char *text, size_t length)
{
    for (size_t index = 0; index < length; index++) {
        const size_t next = (g_tx_head + 1u) % AXIS_NET_TX_BUFFER_SIZE;
        if (next == g_tx_tail) {
            return;
        }
        g_tx_buffer[g_tx_head] = (uint8_t)text[index];
        g_tx_head = next;
    }
}
