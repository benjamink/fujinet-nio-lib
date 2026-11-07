/*
 * fn_transport.c - Commodore 64 transport over the IEC serial bus
 *
 * One IEC device carries whole FujiBus packets through the KERNAL's own bus
 * routines. A request is LISTEN, the packet's bytes, UNLISTEN; the UNLISTEN ends
 * the packet, and the device runs it. The response is TALK, then bytes until the
 * length in the packet's own header. There is no SLIP, padding or segmentation,
 * and one request is outstanding at a time.
 *
 * The device holds the bus while a request runs, as a drive does while it seeks,
 * so a slow request is simply a slow first byte: ACPTR waits. A device that has
 * nothing to send ends the transfer at once.
 *
 * The contract is docs/iec/nio-channel.md in the fujinet-nio repository.
 */

#include <cbm.h>
#include <stdint.h>

#include "fn_internal.h"
#include "fn_platform.h"
#include "fujinet-nio.h"

/* The KERNAL's status byte. READST only returns it, and nothing on the bus path
 * clears it, so a request starts by clearing it. */
#ifndef C64_ST
#define C64_ST (*(volatile uint8_t *)0x90)
#endif

#define NIO_DEVICE 30
#define NIO_CHANNEL 0x60 /* SECOND/TKSA: "data" on channel 0 */

#define ST_READ_TIMEOUT 0x02
#define ST_EOI 0x40
#define ST_DEVICE_NOT_PRESENT 0x80

#define HEADER_LENGTH_END 4 /* bytes 2 and 3 of the header: the packet's length */
#define HEADER_BYTES 6

static uint8_t g_ready;

uint8_t fn_transport_init(void)
{
    g_ready = 0;
    C64_ST = 0;
    /* Addressing the channel with no data tells us the device is there and leaves
     * it idle: nothing was sent, so nothing runs. */
    cbm_k_listen(NIO_DEVICE);
    cbm_k_second(NIO_CHANNEL);
    cbm_k_unlsn();
    if (C64_ST & ST_DEVICE_NOT_PRESENT) {
        return FN_ERR_TRANSPORT;
    }
    g_ready = 1;
    return FN_OK;
}

void fn_transport_close(void)
{
    g_ready = 0;
}

uint8_t fn_transport_ready(void)
{
    return g_ready;
}

const char *fn_platform_name(void)
{
    return "c64";
}

static uint8_t send_request(void)
{
    const uint8_t *data;
    uint16_t remaining;

    data = _fn_transport_ctx.request;
    remaining = _fn_transport_ctx.req_len;

    cbm_k_listen(NIO_DEVICE);
    cbm_k_second(NIO_CHANNEL);
    if (C64_ST & ST_DEVICE_NOT_PRESENT) {
        cbm_k_unlsn();
        return FN_ERR_TRANSPORT;
    }
    while (remaining) {
        cbm_k_ciout(*data);
        ++data;
        --remaining;
    }
    cbm_k_unlsn();
    return (C64_ST & ST_DEVICE_NOT_PRESENT) ? FN_ERR_TRANSPORT : FN_OK;
}

static uint8_t read_response(void)
{
    uint8_t *data;
    uint16_t got;
    uint16_t want;
    uint8_t value;
    uint8_t status;

    data = _fn_transport_ctx.response;
    got = 0;
    want = HEADER_BYTES;

    for (;;) {
        value = cbm_k_acptr();
        status = C64_ST;
        /* A read timeout means ACPTR got no byte: the talker had nothing to send. */
        if (status & (ST_READ_TIMEOUT | ST_DEVICE_NOT_PRESENT)) {
            return FN_ERR_TRANSPORT;
        }
        data[got] = value;
        ++got;
        if (got == HEADER_LENGTH_END) {
            want = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
            if (want < HEADER_BYTES || want > _fn_transport_ctx.resp_max) {
                return FN_ERR_TRANSPORT;
            }
        }
        if (got >= want) {
            break;
        }
        if (status & ST_EOI) {
            return FN_ERR_TRANSPORT; /* the device ended it before the header's length */
        }
    }
    _fn_transport_ctx.resp_len = got;
    return FN_OK;
}

uint8_t fn_transport_exchange(void)
{
    uint8_t result;

    C64_ST = 0;
    _fn_transport_ctx.resp_len = 0;

    result = send_request();
    if (result != FN_OK) {
        return result;
    }

    cbm_k_talk(NIO_DEVICE);
    cbm_k_tksa(NIO_CHANNEL);
    result = (C64_ST & ST_DEVICE_NOT_PRESENT) ? FN_ERR_TRANSPORT : read_response();
    cbm_k_untlk();
    return result;
}
