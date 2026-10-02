#include <stdio.h>
#include <string.h>

#include "fn_protocol.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

/* Keep the test independent of platform-specific auth enum names. */
#define WPA2_TEST_AUTH 4

static uint8_t last_command;
static uint8_t last_payload[256];
static uint16_t last_payload_len;
static unsigned call_count;
static uint8_t adapter_status;
static uint8_t adapter_firmware_len_override;

static void put_string(uint8_t *out, uint16_t *at, const char *value)
{
    uint8_t len = (uint8_t)strlen(value);
    out[(*at)++] = len;
    memcpy(out + *at, value, len);
    *at = (uint16_t)(*at + len);
}

static void reset_call(void)
{
    memset(last_payload, 0, sizeof(last_payload));
    last_payload_len = 0;
    last_command = 0;
    call_count = 0;
}

uint8_t fn_raw_call(uint8_t device,
                    uint8_t command,
                    const void *payload,
                    uint16_t payload_length,
                    void *reply,
                    uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    uint8_t *out = (uint8_t *)reply;
    uint16_t at = 0;

    if (device != FN_DEVICE_WIFI || payload_length > sizeof(last_payload))
        return FN_ERR_INVALID;

    ++call_count;
    last_command = command;
    last_payload_len = payload_length;
    memcpy(last_payload, payload, payload_length);
    response->status = FN_OK;
    response->payload_length = 0;

    if (command == FN_WIFI_CMD_SET_CONFIG) {
        if (payload_length < 2 || last_payload[0] != FN_WIFI_PROTOCOL_VERSION)
            return FN_ERR_INVALID;
        return FN_OK;
    }

    if (command == FN_WIFI_CMD_GET_STATUS) {
        if (reply_capacity < 64) return FN_ERR_INVALID;
        out[at++] = FN_WIFI_PROTOCOL_VERSION;
        out[at++] = 2;       /* link state */
        out[at++] = 1;       /* configured enabled */
        out[at++] = 1;       /* BSSID valid */
        out[at++] = 1;       /* scan supported */
        out[at++] = (uint8_t)-61;
        out[at++] = 0x00; out[at++] = 0x11; out[at++] = 0x22;
        out[at++] = 0x33; out[at++] = 0x44; out[at++] = 0x55;
        put_string(out, &at, "192.0.2.10");
        put_string(out, &at, "255.255.255.0");
        put_string(out, &at, "192.0.2.1");
        put_string(out, &at, "192.0.2.53");
        out[at++] = 0xB6; out[at++] = 0x00;
        out[at++] = FN_WIFI_BACKEND_POSIX_SIMULATED;
        response->payload_length = at;
        return FN_OK;
    }

    if (command == FN_WIFI_CMD_GET_CONFIG) {
        if (reply_capacity < 32) return FN_ERR_INVALID;
        out[at++] = FN_WIFI_PROTOCOL_VERSION;
        out[at++] = 1;
        out[at++] = 1;
        put_string(out, &at, "test-network");
        put_string(out, &at, "00:11:22:33:44:55");
        response->payload_length = at;
        return FN_OK;
    }

    if (command == FN_WIFI_CMD_GET_ADAPTER_INFO) {
        if (payload_length != 1 || last_payload[0] != FN_WIFI_PROTOCOL_VERSION)
            return FN_ERR_INVALID;
        response->status = adapter_status;
        if (adapter_status) return FN_OK;
        if (reply_capacity < 9 + 6) return FN_ERR_INVALID;
        out[at++] = FN_WIFI_PROTOCOL_VERSION;
        out[at++] = 1;
        out[at++] = 0x24; out[at++] = 0x6F; out[at++] = 0x28;
        out[at++] = 0x01; out[at++] = 0x02; out[at++] = 0x03;
        put_string(out, &at, "0.1.1");
        if (adapter_firmware_len_override) out[8] = adapter_firmware_len_override;
        response->payload_length = at;
        return FN_OK;
    }

    if (command == FN_WIFI_CMD_SCAN) {
        uint8_t requested = last_payload[3];
        if (payload_length != 4 || last_payload[0] != FN_WIFI_PROTOCOL_VERSION)
            return FN_ERR_INVALID;
        if (reply_capacity < 3 + 1 + 9 + 9) return FN_ERR_INVALID;
        out[at++] = FN_WIFI_PROTOCOL_VERSION;
        out[at++] = requested < 2 ? 0 : 1;
        out[at++] = requested < 2 ? requested : 2;
        if (requested >= 1) {
            put_string(out, &at, "first-network");
            out[at++] = 0x10; out[at++] = 0x20; out[at++] = 0x30;
            out[at++] = 0x40; out[at++] = 0x50; out[at++] = 0x60;
            out[at++] = (uint8_t)-42; out[at++] = 6; out[at++] = 3;
        }
        if (requested >= 2) {
            put_string(out, &at, "second-network");
            out[at++] = 0xAA; out[at++] = 0xBB; out[at++] = 0xCC;
            out[at++] = 0xDD; out[at++] = 0xEE; out[at++] = 0xFF;
            out[at++] = (uint8_t)-70; out[at++] = 11; out[at++] =  WPA2_TEST_AUTH;
        }
        response->payload_length = at;
        return FN_OK;
    }

    return FN_ERR_INVALID;
}

static int test_status(void)
{
    fn_wifi_status_t status;

    reset_call();
    if (fn_wifi_get_status(&status) != FN_OK || call_count != 1 ||
        last_command != FN_WIFI_CMD_GET_STATUS || last_payload_len != 1 ||
        last_payload[0] != FN_WIFI_PROTOCOL_VERSION)
        return 1;
    if (status.link_state != 2 || status.rssi != -61 ||
        status.bssid.bytes[0] != 0x00 || status.bssid.bytes[5] != 0x55 ||
        strcmp(status.ip, "192.0.2.10") != 0 ||
        strcmp(status.dns, "192.0.2.53") != 0 ||
        status.capabilities != 0x00B6 ||
        status.backend_kind != FN_WIFI_BACKEND_POSIX_SIMULATED)
        return 1;
    return 0;
}

static int test_config_and_set(void)
{
    fn_wifi_config_t config;
    fn_wifi_config_update_t update;
    static const uint8_t expected[] = {
        1, 0x3F, 1, 4, 'C', 'a', 'f', 'e',
        17, '0', '1', ':', '2', '3', ':', '4', '5', ':', '6', '7', ':', '8', '9', ':', 'a', 'b',
        6, 's', 'e', 'c', 'r', 'e', 't'
    };

    reset_call();
    if (fn_wifi_get_config(&config) != FN_OK ||
        strcmp(config.ssid, "test-network") != 0 ||
        strcmp(config.bssid, "00:11:22:33:44:55") != 0 || !config.password_present)
        return 1;

    update.fields = FN_WIFI_SET_ENABLED | FN_WIFI_SET_SSID |
                    FN_WIFI_SET_BSSID | FN_WIFI_SET_PASSWORD |
                    FN_WIFI_SET_PERSIST | FN_WIFI_SET_RECONNECT;
    update.enabled = 1;
    update.ssid = "Cafe";
    update.bssid = "01:23:45:67:89:ab";
    update.password = "secret";
    reset_call();
    if (fn_wifi_set_config(&update) != FN_OK ||
        last_command != FN_WIFI_CMD_SET_CONFIG ||
        last_payload_len != sizeof(expected) ||
        memcmp(last_payload, expected, sizeof(expected)) != 0)
        return 1;
    return 0;
}

static int test_scan_and_boundaries(void)
{
    fn_wifi_scan_record_t records[2];
    uint8_t response[3 + 1 + FN_WIFI_MAX_SSID + 9];
    uint8_t count = 0, more = 0;

    reset_call();
    if (fn_wifi_scan(0, 2, records, 2, &count, &more,
                     response, sizeof(response)) != FN_OK ||
        last_payload[3] != 1 || count != 1 || more != 0 ||
        strcmp(records[0].ssid, "first-network") != 0 ||
        records[0].rssi != -42 || records[0].channel != 6 ||
        records[0].bssid.bytes[5] != 0x60)
        return 1;

    if (fn_wifi_scan(0, 1, records, 2, &count, &more,
                     response, sizeof(response) - 1) != FN_ERR_INVALID)
        return 1;
    if (fn_wifi_scan(0, 1, records, 0, &count, &more,
                     response, sizeof(response)) != FN_ERR_INVALID)
        return 1;
    return 0;
}

static int test_adapter_info(void)
{
    fn_wifi_adapter_info_t info;

    reset_call();
    adapter_status = 0;
    adapter_firmware_len_override = 0;
    if (fn_wifi_get_adapter_info(&info) != FN_OK) return 1;
    if (last_command != FN_WIFI_CMD_GET_ADAPTER_INFO || last_payload_len != 1) return 1;
    if (!info.mac.valid || info.mac.bytes[0] != 0x24 || info.mac.bytes[5] != 0x03) return 1;
    if (strcmp(info.firmware, "0.1.1") != 0) return 1;

    /* Firmware without the command answers Unsupported (wire status 8). */
    adapter_status = 8;
    if (fn_wifi_get_adapter_info(&info) != FN_ERR_UNSUPPORTED) return 1;

    /* A length byte that disagrees with the payload is rejected. */
    adapter_status = 0;
    adapter_firmware_len_override = 9;
    if (fn_wifi_get_adapter_info(&info) != FN_ERR_IO) return 1;
    adapter_firmware_len_override = 0;

    if (fn_wifi_get_adapter_info(0) != FN_ERR_INVALID) return 1;
    return 0;
}

int main(void)
{
    if (test_status()) { puts("wifi status wire test failed"); return 1; }
    if (test_config_and_set()) { puts("wifi config wire test failed"); return 1; }
    if (test_scan_and_boundaries()) { puts("wifi scan wire test failed"); return 1; }
    if (test_adapter_info()) { puts("wifi adapter info wire test failed"); return 1; }
    puts("wifi wire tests passed");
    return 0;
}
