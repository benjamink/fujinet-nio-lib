/* One request/reply exchange over the CS1 link. */
#include <PalmOS.h>
#include "fujinet-nio.h"
#include "fn_palmos_internal.h"

/* NIO's own HTTP opens can take a while (DNS, TLS); the firmware gives up
 * after 30 s and publishes a timeout, so wait a little longer than that. */
#define REPLY_WAIT_SECONDS 35

/* A request the firmware found damaged is sent again; a reply that fails its
 * check is read again (it stays in place) before giving up. */
#define SEND_TRIES 3
#define READ_RETRIES 3

/* BEGIN, the bytes two per DATA word (first one high), then CHECK over them. */
static void send_request(const uint8_t *req, uint16_t len)
{
    fn_palmos_port_t port = FN_PALMOS_PORT(_fn_palmos_cs1 + SBN_W(SBN_W_DATA));
    uint16_t pairs = len >> 1, w, check = 0;

    FN_PALMOS_WR(SBN_W_BEGIN, len);
    while (pairs--) {
        w = (uint16_t)((uint16_t)req[0] << 8 | req[1]);
        req += 2;
        check = SBN_CHECK_STEP(check, w);
        FN_PALMOS_PUT(port, w);
    }
    if (len & 1) {
        w = (uint16_t)((uint16_t)req[0] << 8);
        check = SBN_CHECK_STEP(check, w);
        FN_PALMOS_PUT(port, w);
    }
    FN_PALMOS_WR(SBN_W_CHECK, check);
}

/* Read the reply words, keep at most cap bytes, check every word. Zero if
 * the length or the check did not read back right. */
static uint8_t read_reply(uint8_t *resp, uint16_t cap, uint16_t *resp_len)
{
    fn_palmos_port_t port = FN_PALMOS_PORT(_fn_palmos_cs1 + SBN_R_RESP);
    uint16_t n = FN_PALMOS_RD(SBN_R_RESP_LEN), want = FN_PALMOS_RD(SBN_R_RESP_CHECK);
    uint16_t keep, pairs, words, w, check = 0;

    if (n > SBN_RESP_MAX) return 0;
    keep = n < cap ? n : cap;
    pairs = keep >> 1;
    words = ((n + 1) >> 1) - pairs;
    while (pairs--) {
        w = FN_PALMOS_GET(port);
        check = SBN_CHECK_STEP(check, w);
        resp[0] = (uint8_t)(w >> 8);
        resp[1] = (uint8_t)w;
        resp += 2;
    }
    if (words) {
        /* The word with an odd last byte to keep, or the first one past cap. */
        w = FN_PALMOS_GET(port);
        check = SBN_CHECK_STEP(check, w);
        if (keep & 1) *resp = (uint8_t)(w >> 8);
        words--;
    }
    while (words--) {
        w = FN_PALMOS_GET(port);
        check = SBN_CHECK_STEP(check, w);
    }
    *resp_len = keep;
    return check == want;
}

uint16_t fn_palmos_exchange(const uint8_t *req, uint16_t len, uint8_t *resp, uint16_t cap,
                            uint16_t *resp_len)
{
    uint16_t tries, result = SBN_RES_BAD_CHECK;
    uint32_t start, wait = (uint32_t)REPLY_WAIT_SECONDS * SysTicksPerSecond();

    *resp_len = 0;
    if (!fn_palmos_open()) return FN_PALMOS_NO_ANSWER;
    if (!len || len > SBN_REQ_MAX) return SBN_RES_BAD_LEN;
    for (tries = 0; tries < SEND_TRIES && result == SBN_RES_BAD_CHECK; tries++) {
        if (++_fn_palmos_seq == 0) _fn_palmos_seq = 1;
        send_request(req, len);
        FN_PALMOS_WR(SBN_W_DOORBELL, _fn_palmos_seq);
        start = TimGetTicks();
        while (FN_PALMOS_RD(SBN_R_RESP_SEQ) != _fn_palmos_seq)
            if (TimGetTicks() - start > wait) return FN_PALMOS_NO_ANSWER;
        result = FN_PALMOS_RD(SBN_R_RESULT);
    }
    /* Only a good result has a reply to read. */
    if (result != SBN_RES_OK) return result;
    for (tries = 0; tries <= READ_RETRIES; tries++)
        if (read_reply(resp, cap, resp_len)) return result;
    *resp_len = 0;
    return FN_PALMOS_BAD_READ;
}
