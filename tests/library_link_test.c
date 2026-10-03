#include <stdint.h>

#include "fn_raw.h"
#include "fn_session.h"
#include "fujinet-nio.h"

static uint8_t channel_open(void *context)
{
    (void)context;
    return FN_OK;
}

static void channel_close(void *context)
{
    (void)context;
}

static uint8_t channel_write(void *context, uint8_t value, uint16_t timeout_ms)
{
    (void)context;
    (void)value;
    (void)timeout_ms;
    return FN_OK;
}

static uint8_t channel_read(void *context, uint8_t *value, uint16_t timeout_ms)
{
    (void)context;
    (void)value;
    (void)timeout_ms;
    return 0;
}

static void channel_flush(void *context)
{
    (void)context;
}

/* Supply the raw transport seam so the DiskDevice API can be exercised
 * without requiring a physical channel in this host-side link test. */
uint8_t fn_raw_call(uint8_t device, uint8_t command,
                    const void *payload, uint16_t payload_length,
                    void *reply, uint16_t reply_capacity,
                    fn_raw_response_t *response)
{
    uint8_t *out = (uint8_t *)reply;

    (void)device;
    (void)payload;
    (void)payload_length;
    if (!response) return FN_ERR_INVALID;
    if (command == 0x0E) {
        if (reply_capacity < 5) return FN_ERR_INVALID;
        out[0] = FN_DISK_PROTOCOL_VERSION;
        out[1] = out[2] = out[3] = 0;
        out[4] = 1;
        response->status = FN_OK;
        response->payload_length = 5;
        return FN_OK;
    }
    if (reply_capacity < 12) return FN_ERR_INVALID;
    out[0] = FN_DISK_PROTOCOL_VERSION;
    out[1] = FN_DISK_FLAG_MOUNTED | FN_DISK_FLAG_READONLY;
    out[2] = 0;
    out[3] = 0;
    out[4] = 1;
    out[5] = FN_DISK_TYPE_RAW;
    out[6] = 0;
    out[7] = 2;
    out[8] = 0xE0;
    out[9] = 0x06;
    out[10] = 0;
    out[11] = 0;
    response->status = FN_OK;
    response->payload_length = 12;
    return FN_OK;
}

int main(void)
{
    static const fn_stream_channel_ops_t channel_ops = {
        channel_open, channel_close, channel_write, channel_read,
        channel_flush, NULL
    };
    fn_stream_session_t session;
    fn_disk_info_t info;
    uint8_t wire[64];

    if (fn_stream_session_init(&session, &channel_ops, 0, wire,
                               sizeof(wire)) != FN_OK) return 1;
    if (fn_disk_mount(1, "test:disk.adf", 1, FN_DISK_TYPE_AUTO, 0,
                      &info) != FN_OK) return 1;
    if (info.slot != 1 || info.sector_size != 512 ||
        info.sector_count != 1760) return 1;
    if (fn_disk_flush(1) != FN_OK) return 1;
    (void)&fn_disk_write_sector_context;
    (void)&fn_disk_flush_context;
    (void)&fn_disk_unmount_context;
    (void)&fn_disk_clear_changed_context;
    /* Adapter details: link from the archive, not just the wire tests. */
    (void)&fn_fuji_get_info;
    (void)&fn_wifi_get_adapter_info;
    return 0;
}
