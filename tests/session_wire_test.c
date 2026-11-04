#include <stdio.h>
#include <string.h>

#include "fn_protocol.h"
#include "fn_internal.h"
#include "fn_session.h"

typedef struct {
    uint8_t tx[128];
    uint16_t tx_length;
    uint8_t rx[128];
    uint16_t rx_length;
    uint16_t rx_offset;
    uint8_t opened;
    uint8_t flushed;
    uint8_t byte_write_calls;
    uint8_t bulk_write_calls;
} fake_serial_t;

static uint8_t fake_open(void *context)
{
    fake_serial_t *fake = (fake_serial_t *)context;
    fake->opened = 1;
    return FN_OK;
}

static void fake_close(void *context)
{
    ((fake_serial_t *)context)->opened = 0;
}

static uint8_t fake_write(void *context, uint8_t value, uint16_t timeout_ms)
{
    fake_serial_t *fake = (fake_serial_t *)context;
    (void)timeout_ms;
    if (fake->tx_length >= sizeof(fake->tx)) return FN_ERR_IO;
    fake->byte_write_calls++;
    fake->tx[fake->tx_length++] = value;
    return FN_OK;
}

static uint8_t fake_write_bytes(void *context, const uint8_t *data,
                                uint16_t length, uint16_t timeout_ms)
{
    fake_serial_t *fake = (fake_serial_t *)context;
    (void)timeout_ms;
    if ((uint32_t)fake->tx_length + length > sizeof(fake->tx)) return FN_ERR_IO;
    memcpy(&fake->tx[fake->tx_length], data, length);
    fake->tx_length = (uint16_t)(fake->tx_length + length);
    fake->bulk_write_calls++;
    return FN_OK;
}

static uint8_t fake_read(void *context, uint8_t *value, uint16_t timeout_ms)
{
    fake_serial_t *fake = (fake_serial_t *)context;
    (void)timeout_ms;
    if (fake->rx_offset >= fake->rx_length) return 0;
    *value = fake->rx[fake->rx_offset++];
    return 1;
}

static void fake_flush(void *context)
{
    ((fake_serial_t *)context)->flushed = 1;
}

static const fn_stream_channel_ops_t fake_ops = {
    fake_open, fake_close, fake_write, fake_read, fake_flush, NULL
};

static const fn_stream_channel_ops_t fake_bulk_ops = {
    fake_open, fake_close, fake_write, fake_read, fake_flush, fake_write_bytes
};

static void mini_fuji(uint8_t *buf, uint8_t device, uint8_t cmd, uint16_t total)
{
    buf[0] = device;
    buf[1] = cmd;
    buf[2] = (uint8_t)(total & 0xFF);
    buf[3] = (uint8_t)(total >> 8);
    buf[4] = 0;
    buf[5] = 0;
}

static int test_rs232_slip_session(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[64];
    uint8_t response[16];
    uint16_t response_length = 0;
    const uint8_t request[] = {0x01, 0xFC, 0xDB, 0xC0};
    uint8_t response_packet[FN_HEADER_SIZE];
    uint16_t encoded_length;

    mini_fuji(response_packet, 0x01, 0xFC, FN_HEADER_SIZE);

    encoded_length = fn_slip_encode(response_packet, sizeof(response_packet), fake.rx);
    fake.rx_length = encoded_length;
    if (fn_stream_session_init(&session, &fake_ops, &fake, wire, sizeof(wire)) != FN_OK ||
        fn_stream_session_open(&session) != FN_OK) return 1;
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  sizeof(response), &response_length, 100) != FN_OK) return 1;
    if (!fake.flushed || response_length != sizeof(response_packet) ||
        memcmp(response, response_packet, sizeof(response_packet)) != 0) return 1;
    if (fake.tx_length != 8 || fake.tx[0] != SLIP_END || fake.tx[7] != SLIP_END ||
        fake.tx[3] != SLIP_ESCAPE || fake.tx[4] != SLIP_ESC_ESC ||
        fake.tx[5] != SLIP_ESCAPE || fake.tx[6] != SLIP_ESC_END) return 1;
    if (fn_stream_session_capabilities(&session)->max_outstanding != 1 ||
        fn_stream_session_capabilities(&session)->packet_native != 0) return 1;
    fn_stream_session_close(&session);
    return fake.opened ? 1 : 0;
}

static int test_bulk_write_slip_session(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[64];
    uint8_t response[16];
    uint16_t response_length = 0;
    const uint8_t request[] = {0x01, 0xFC, 0xDB, 0xC0};
    uint8_t response_packet[FN_HEADER_SIZE];

    mini_fuji(response_packet, 0x01, 0xFC, FN_HEADER_SIZE);

    fake.rx_length = fn_slip_encode(response_packet, sizeof(response_packet),
                                    fake.rx);
    if (fn_stream_session_init(&session, &fake_bulk_ops, &fake, wire,
                               sizeof(wire)) != FN_OK ||
        fn_stream_session_open(&session) != FN_OK) return 1;
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  sizeof(response), &response_length, 100) != FN_OK)
        return 1;
    if (fake.bulk_write_calls != 1 || fake.byte_write_calls != 0 ||
        fake.tx_length != 8 || fake.tx[0] != SLIP_END ||
        fake.tx[3] != SLIP_ESCAPE || fake.tx[4] != SLIP_ESC_ESC ||
        fake.tx[5] != SLIP_ESCAPE || fake.tx[6] != SLIP_ESC_END ||
        fake.tx[7] != SLIP_END || response_length != sizeof(response_packet) ||
        memcmp(response, response_packet, sizeof(response_packet)) != 0)
        return 1;
    return 0;
}

static int test_session_timeout_and_busy(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[32], response[8];
    uint16_t response_length = 0;

    if (fn_stream_session_init(&session, &fake_ops, &fake, wire, sizeof(wire)) != FN_OK ||
        fn_stream_session_open(&session) != FN_OK) return 1;
    if (fn_stream_session_request(&session, (const uint8_t *)"x", 1,
                                  response, sizeof(response), &response_length, 20) !=
        FN_ERR_TIMEOUT) return 1;
    fake.rx[0] = SLIP_END;
    memset(fake.rx + 1, 0x01, sizeof(fake.rx) - 1);
    fake.rx_length = sizeof(fake.rx);
    fake.rx_offset = 0;
    if (fn_stream_session_request(&session, (const uint8_t *)"x", 1,
                                  response, sizeof(response), &response_length, 20) !=
        FN_ERR_IO) return 1;
    session.busy = 1;
    if (fn_stream_session_request(&session, (const uint8_t *)"x", 1,
                                  response, sizeof(response), &response_length, 20) !=
        FN_ERR_BUSY) return 1;
    return 0;
}

static int test_lost_opening_end(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[64];
    uint8_t response[16];
    uint16_t response_length = 0;
    const uint8_t request[] = {0x01, 0xFC, 0x00};
    uint8_t response_packet[FN_HEADER_SIZE];
    uint8_t encoded[64];
    uint16_t encoded_length;

    mini_fuji(response_packet, 0x01, 0xFC, FN_HEADER_SIZE);

    encoded_length = fn_slip_encode(response_packet, sizeof(response_packet),
                                    encoded);
    /* Opening END dropped; payload and closing END remain. */
    memcpy(fake.rx, encoded + 1, (size_t)(encoded_length - 1U));
    fake.rx_length = (uint16_t)(encoded_length - 1U);
    if (fn_stream_session_init(&session, &fake_ops, &fake, wire, sizeof(wire)) !=
            FN_OK ||
        fn_stream_session_open(&session) != FN_OK)
        return 1;
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  sizeof(response), &response_length, 100) !=
        FN_OK)
        return 1;
    if (response_length != sizeof(response_packet) ||
        memcmp(response, response_packet, sizeof(response_packet)) != 0)
        return 1;
    return 0;
}

static int test_short_fujibus_is_io(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[64];
    uint8_t response[16];
    uint16_t response_length = 0;
    uint8_t full[8];
    const uint8_t request[] = {0x01, 0xFC, 0x00};

    mini_fuji(full, 0xFE, 0x02, 8);
    full[6] = 0x11;
    full[7] = 0x22;
    /* Length field says 8, but the SLIP body is only the 6-byte header. */
    fake.rx_length = fn_slip_encode(full, FN_HEADER_SIZE, fake.rx);
    if (fn_stream_session_init(&session, &fake_ops, &fake, wire, sizeof(wire)) !=
            FN_OK ||
        fn_stream_session_open(&session) != FN_OK)
        return 1;
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  sizeof(response), &response_length, 100) !=
        FN_ERR_IO)
        return 1;
    if (session.last_decoded_length != FN_HEADER_SIZE)
        return 1;
    if (session.last_raw_length < (uint16_t)(FN_HEADER_SIZE + 2U))
        return 1;
    return 0;
}

static int test_oversized_frame_is_io(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[128];
    uint8_t response[48];
    uint16_t response_length = 0;
    uint8_t big[40];
    const uint8_t request[] = {0x01, 0xFC, 0x00};
    uint16_t i;

    /* A whole, valid frame, but bigger than the caller's 16-byte buffer */
    memset(big, 0x5A, sizeof(big));
    mini_fuji(big, 0xFE, 0x02, sizeof(big));
    fake.rx_length = fn_slip_encode(big, sizeof(big), fake.rx);
    memset(response, 0xA5, sizeof(response));
    if (fn_stream_session_init(&session, &fake_ops, &fake, wire, sizeof(wire)) !=
            FN_OK ||
        fn_stream_session_open(&session) != FN_OK)
        return 1;
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  16, &response_length, 100) != FN_ERR_IO)
        return 1;
    for (i = 16; i < sizeof(response); ++i)
        if (response[i] != 0xA5)
            return 1;
    return 0;
}

static int test_frame_exactly_filling_the_buffer(void)
{
    fake_serial_t fake = {0};
    fn_stream_session_t session;
    uint8_t wire[128];
    uint8_t response[24];
    uint16_t response_length = 0;
    uint8_t packet[16];
    const uint8_t request[] = {0x01, 0xFC, 0x00};
    uint16_t i;

    /* END and ESC bytes in the body: each takes two on the wire but one
     * decoded, and it is the decoded size that must fit */
    memset(packet, 0x5A, sizeof(packet));
    mini_fuji(packet, 0xFE, 0x02, sizeof(packet));
    packet[8] = SLIP_END;
    packet[9] = SLIP_ESCAPE;
    fake.rx_length = fn_slip_encode(packet, sizeof(packet), fake.rx);
    memset(response, 0xA5, sizeof(response));
    if (fn_stream_session_init(&session, &fake_ops, &fake, wire, sizeof(wire)) !=
            FN_OK ||
        fn_stream_session_open(&session) != FN_OK)
        return 1;
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  sizeof(packet), &response_length, 100) != FN_OK ||
        response_length != sizeof(packet) ||
        memcmp(response, packet, sizeof(packet)) != 0 ||
        response[sizeof(packet)] != 0xA5)
        return 1;

    /* One byte less room and the same frame is refused, the last byte of the
     * full-size reply untouched */
    fake.rx_offset = 0;
    memset(response, 0xA5, sizeof(response));
    if (fn_stream_session_request(&session, request, sizeof(request), response,
                                  sizeof(packet) - 1, &response_length, 100) !=
        FN_ERR_IO)
        return 1;
    for (i = sizeof(packet) - 1; i < sizeof(response); ++i)
        if (response[i] != 0xA5)
            return 1;
    return 0;
}

int main(void)
{
    if (test_rs232_slip_session() || test_bulk_write_slip_session() ||
        test_session_timeout_and_busy() || test_lost_opening_end() ||
        test_short_fujibus_is_io() || test_oversized_frame_is_io() ||
        test_frame_exactly_filling_the_buffer()) {
        puts("session wire tests failed");
        return 1;
    }
    puts("session wire tests passed");
    return 0;
}
