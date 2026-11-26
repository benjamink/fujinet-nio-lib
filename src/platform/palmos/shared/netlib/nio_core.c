/* FujiBus packet in, packet out: checks the request, calls the device and
 * frames its reply with the status as the packet's one parameter. */
#include <string.h>
#include "nio.h"

void nio_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void nio_put32(uint8_t *p, uint32_t v)
{
    nio_put16(p, (uint16_t)v);
    nio_put16(p + 2, (uint16_t)(v >> 16));
}

/* FujiBus checksum: bytes summed with the carry folded in, skipping the
 * checksum field itself (offset 4). */
static uint8_t checksum(const uint8_t *p, uint16_t len)
{
    uint16_t sum = 0, i;

    for (i = 0; i < len; i++) {
        if (i == 4) continue;
        sum = (uint16_t)(sum + p[i]);
        sum = (uint16_t)((sum >> 8) + (sum & 0xFF));
    }
    return (uint8_t)sum;
}

void nio_init(NioState *nio)
{
    memset(nio, 0, sizeof(*nio));
    nio->next_handle = 1;
}

uint16_t nio_exchange(NioState *nio, const uint8_t *req, uint16_t req_len,
                      uint8_t *resp, uint16_t resp_cap)
{
    uint16_t out_len = 0, total;
    uint8_t status;
    const uint8_t *in;
    uint16_t in_len;

    if (req_len < NIO_HEADER || NIO_GET16(req + 2) != req_len || resp_cap < NIO_REPLY_HEAD)
        return 0;
    if (checksum(req, req_len) != req[4])
        return 0;
    /* Requests from fujinet-nio-lib carry no parameters, only a payload. */
    if (req[5] != 0) {
        status = NIO_ST_INVALID;
    } else {
        in = req + NIO_HEADER;
        in_len = (uint16_t)(req_len - NIO_HEADER);
        switch (req[0]) {
        case NIO_DEV_NETWORK:
            status = nio_network(nio, req[1], in, in_len, resp + NIO_REPLY_HEAD,
                                 (uint16_t)(resp_cap - NIO_REPLY_HEAD), &out_len);
            break;
        case NIO_DEV_FUJI:
            status = nio_fuji(nio, req[1], in, in_len, resp + NIO_REPLY_HEAD,
                              (uint16_t)(resp_cap - NIO_REPLY_HEAD), &out_len);
            break;
        case NIO_DEV_WIFI:
            status = nio_wifi(nio, req[1], in, in_len, resp + NIO_REPLY_HEAD,
                              (uint16_t)(resp_cap - NIO_REPLY_HEAD), &out_len);
            break;
        default:
            status = NIO_ST_NO_DEVICE;
            break;
        }
    }
    if (status != NIO_ST_OK) out_len = 0;
    total = (uint16_t)(NIO_REPLY_HEAD + out_len);
    resp[0] = req[0];
    resp[1] = req[1];
    nio_put16(resp + 2, total);
    resp[4] = 0;
    resp[5] = 1;              /* one 1-byte parameter: the status */
    resp[6] = status;
    resp[4] = checksum(resp, total);
    return total;
}
