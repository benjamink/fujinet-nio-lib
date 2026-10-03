#include <stdio.h>
#include <string.h>

#include "fn_protocol.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

/* FujiDevice GetInfo: reply is version, firmware (u8 length + text), build
 * profile (u8 length + text). The fake below stands in for fn_raw_call. */

static uint8_t last_device;
static uint8_t last_command;
static uint8_t last_payload[8];
static uint16_t last_payload_len;
static uint8_t reply_status;
static uint8_t reply_wire[256];
static uint16_t reply_len;

static void set_reply(const char *firmware, const char *profile)
{
    uint8_t n;
    reply_len = 0;
    reply_wire[reply_len++] = FN_FUJI_PROTOCOL_VERSION;
    n = (uint8_t)strlen(firmware);
    reply_wire[reply_len++] = n;
    memcpy(reply_wire + reply_len, firmware, n);
    reply_len = (uint16_t)(reply_len + n);
    n = (uint8_t)strlen(profile);
    reply_wire[reply_len++] = n;
    memcpy(reply_wire + reply_len, profile, n);
    reply_len = (uint16_t)(reply_len + n);
}

uint8_t fn_raw_call(uint8_t device,
                    uint8_t command,
                    const void *payload,
                    uint16_t payload_length,
                    void *reply,
                    uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    last_device = device;
    last_command = command;
    last_payload_len = payload_length;
    if (payload_length > sizeof(last_payload)) return FN_ERR_INVALID;
    memcpy(last_payload, payload, payload_length);
    response->status = reply_status;
    response->payload_length = 0;
    if (reply_status) return FN_OK;
    /* As the real fn_raw_call: a reply over capacity is an error. */
    if (reply_len > reply_capacity) return FN_ERR_IO;
    memcpy(reply, reply_wire, reply_len);
    response->payload_length = reply_len;
    return FN_OK;
}

static int test_get_info(void)
{
    fn_fuji_info_t info;
    char longest_firmware[FN_FUJI_MAX_FIRMWARE_VERSION + 1];
    char longest_profile[FN_FUJI_MAX_BUILD_PROFILE + 1];

    reply_status = 0;
    set_reply("0.1.1", "S3 + FujiBus over GPIO (e.g. RS232)");
    if (fn_fuji_get_info(&info) != FN_OK) return 1;
    if (last_device != FN_DEVICE_FUJI || last_command != FN_FUJI_CMD_GET_INFO) return 1;
    if (last_payload_len != 1 || last_payload[0] != FN_FUJI_PROTOCOL_VERSION) return 1;
    if (strcmp(info.firmware, "0.1.1") != 0) return 1;
    if (strcmp(info.profile, "S3 + FujiBus over GPIO (e.g. RS232)") != 0) return 1;

    /* The largest version-1 reply fits the call's buffer. */
    memset(longest_firmware, 'f', FN_FUJI_MAX_FIRMWARE_VERSION);
    longest_firmware[FN_FUJI_MAX_FIRMWARE_VERSION] = 0;
    memset(longest_profile, 'p', FN_FUJI_MAX_BUILD_PROFILE);
    longest_profile[FN_FUJI_MAX_BUILD_PROFILE] = 0;
    set_reply(longest_firmware, longest_profile);
    if (fn_fuji_get_info(&info) != FN_OK) return 1;
    if (strcmp(info.firmware, longest_firmware) != 0 || strcmp(info.profile, longest_profile) != 0) return 1;

    /* Firmware without the command answers Unsupported (wire status 8). */
    reply_status = 8;
    if (fn_fuji_get_info(&info) != FN_ERR_UNSUPPORTED) return 1;
    reply_status = 0;
    return 0;
}

static int test_malformed_replies(void)
{
    fn_fuji_info_t info;

    reply_status = 0;
    /* A string length past the end of the reply. */
    set_reply("0.1.1", "Generic");
    reply_wire[1] = 40;
    if (fn_fuji_get_info(&info) != FN_ERR_IO) return 1;
    if (info.firmware[0] || info.profile[0]) return 1;

    /* Missing profile. */
    set_reply("0.1.1", "Generic");
    reply_len = 7;
    if (fn_fuji_get_info(&info) != FN_ERR_IO) return 1;

    /* Trailing bytes: a version-1 reply has exactly these fields. */
    set_reply("0.1.1", "Generic");
    reply_wire[reply_len++] = 0xAA;
    if (fn_fuji_get_info(&info) != FN_ERR_IO) return 1;

    /* A reply version above what we asked for, or zero, is not one we read. */
    set_reply("0.1.1", "Generic");
    reply_wire[0] = 2;
    if (fn_fuji_get_info(&info) != FN_ERR_IO) return 1;
    reply_wire[0] = 0;
    if (fn_fuji_get_info(&info) != FN_ERR_IO) return 1;

    if (fn_fuji_get_info(0) != FN_ERR_INVALID) return 1;
    return 0;
}

int main(void)
{
    if (test_get_info()) { puts("fuji info wire test failed"); return 1; }
    if (test_malformed_replies()) { puts("fuji info malformed-reply test failed"); return 1; }
    puts("fuji wire tests passed");
    return 0;
}
