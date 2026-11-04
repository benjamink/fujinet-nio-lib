/* Host test for the Palm OS Springboard client: a fake module behind the CS1
 * bus macros plays the firmware, so the check word, resend, padding, cut-off
 * and error mapping run without a Palm. */
#include <PalmOS.h>
#include "sbnio.h"
#include "fn_platform.h"
#include "fn_internal.h"
#include "fn_raw.h"
#include "fujinet-nio.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef FN_PLATFORM_PALMOS
#error "fn_platform.h does not detect Palm OS"
#endif

static unsigned failures;

#define CHECK(name, expression) do {                                      \
    if (!(expression)) {                                                  \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__);          \
        ++failures;                                                       \
    }                                                                     \
} while (0)

#define CS0_BASE 0x28000000UL
#define CS0_SIZE 0x01000000UL
#define POISON   0xEE

/* The module: registers, the request as the firmware received it, the reply
 * it publishes, and switches to make it misbehave. */
static struct {
    int present;
    uint32_t cs1;
    uint16_t magic, version, status, echo;
    uint16_t resp_seq, resp_len, result, resp_check;
    uint16_t resp[SBN_RESP_MAX / 2];
    uint16_t req[SBN_REQ_MAX / 2 + 4];
    unsigned req_words;
    uint16_t begin_len, check;
    uint16_t log[16];
    unsigned log_words;
    uint8_t seqs[8];
    unsigned doorbells, resp_reads, writes, reads;
    uint8_t reply[SBN_RESP_MAX];
    unsigned reply_len;
    uint16_t forced_result;      /* published instead of OK for a good request */
    unsigned corrupt_requests;   /* doorbells that see a damaged first word */
    unsigned silent_doorbells;   /* doorbells that never get a reply */
    unsigned late_polls;         /* RESP_SEQ polls before the reply shows */
    unsigned bad_read_passes;    /* reply reads whose first word comes back wrong */
    unsigned pending_seq, polls_left;
    int bad_pass;
} fw;

static struct { uint8_t removable, installed; uint32_t base, size; } cards[4];
static unsigned hs_calls;
static uint32_t g_ticks;

UInt32 TimGetTicks(void) { return g_ticks++; }
UInt16 SysTicksPerSecond(void) { return 100; }

Err HsCardAttrGet(UInt16 cardNo, UInt16 attr, void *valueP)
{
    ++hs_calls;
    if (cardNo < 1 || cardNo > 3) return 1;
    switch (attr) {
    case 0: *(UInt8 *)valueP = cards[cardNo].removable; return 0;
    case 1: *(UInt8 *)valueP = cards[cardNo].installed; return 0;
    case 3: *(UInt32 *)valueP = cards[cardNo].base; return 0;
    case 4: *(UInt32 *)valueP = cards[cardNo].size; return 0;
    default: return 1;
    }
}

static uint16_t check_words(const uint16_t *w, unsigned n)
{
    uint16_t c = 0;
    unsigned i;

    for (i = 0; i < n; i++) c = SBN_CHECK_STEP(c, w[i]);
    return c;
}

static void publish(uint8_t seq, uint16_t result)
{
    unsigned i, words = (fw.reply_len + 1) / 2;

    memset(fw.resp, 0, sizeof fw.resp);
    for (i = 0; i < fw.reply_len; i++)
        fw.resp[i / 2] |= (uint16_t)(fw.reply[i] << ((i & 1) ? 0 : 8));
    fw.resp_len = result == SBN_RES_OK ? (uint16_t)fw.reply_len : 0;
    fw.resp_check = result == SBN_RES_OK ? check_words(fw.resp, words) : 0;
    fw.result = result;
    if (fw.late_polls) {
        fw.pending_seq = seq;
        fw.polls_left = fw.late_polls;
    } else {
        fw.resp_seq = seq;
    }
}

static void doorbell(uint8_t seq)
{
    uint16_t result = SBN_RES_OK;
    uint16_t c;

    if (fw.doorbells < sizeof fw.seqs) fw.seqs[fw.doorbells] = seq;
    ++fw.doorbells;
    if (fw.silent_doorbells) {
        --fw.silent_doorbells;
        return;
    }
    if (!fw.begin_len || fw.begin_len > SBN_REQ_MAX ||
        fw.req_words != (unsigned)(fw.begin_len + 1) / 2) {
        result = SBN_RES_BAD_LEN;
    } else {
        if (fw.corrupt_requests) {
            fw.req[0] ^= 0x0100;
            --fw.corrupt_requests;
        }
        c = check_words(fw.req, fw.req_words);
        if (c != fw.check) result = SBN_RES_BAD_CHECK;
        else if (fw.forced_result) result = fw.forced_result;
    }
    publish(seq, result);
}

uint16_t fn_palmos_test_read(uint32_t address)
{
    uint32_t offset = address - fw.cs1;
    unsigned index;

    ++fw.reads;
    if (!fw.present || address < fw.cs1 || offset >= 0x01000000UL) return 0xFFFF;
    offset &= 0xFFFF;
    if (offset >= SBN_R_RESP) {
        index = (offset - SBN_R_RESP) / 2;
        ++fw.resp_reads;
        if (index == 0) {
            fw.bad_pass = fw.bad_read_passes != 0;
            if (fw.bad_pass) --fw.bad_read_passes;
        }
        return (uint16_t)(fw.resp[index] ^ ((index == 0 && fw.bad_pass) ? 0x0100 : 0));
    }
    switch (offset) {
    case SBN_R_MAGIC: return fw.magic;
    case SBN_R_VERSION: return fw.version;
    case SBN_R_STATUS: return fw.status;
    case SBN_R_RESP_SEQ:
        if (fw.pending_seq) {
            if (fw.polls_left) {
                --fw.polls_left;
            } else {
                fw.resp_seq = (uint16_t)fw.pending_seq;
                fw.pending_seq = 0;
            }
        }
        return fw.resp_seq;
    case SBN_R_RESP_LEN: return fw.resp_len;
    case SBN_R_RESULT: return fw.result;
    case SBN_R_RESP_CHECK: return fw.resp_check;
    case SBN_R_ECHO: return fw.echo;
    default: return 0;
    }
}

void fn_palmos_test_write(uint32_t address, uint16_t value)
{
    uint32_t offset = address - fw.cs1;

    ++fw.writes;
    if (!fw.present || address < fw.cs1 || offset >= 0x01000000UL) return;
    switch (offset >> 16) {
    case SBN_W_DATA:
        if (fw.req_words < sizeof fw.req / sizeof fw.req[0]) fw.req[fw.req_words] = value;
        ++fw.req_words;
        break;
    case SBN_W_BEGIN: fw.begin_len = value; fw.req_words = 0; break;
    case SBN_W_CHECK: fw.check = value; break;
    case SBN_W_DOORBELL: doorbell((uint8_t)value); break;
    case SBN_W_LOG:
        if (fw.log_words < sizeof fw.log / sizeof fw.log[0]) fw.log[fw.log_words] = value;
        ++fw.log_words;
        break;
    case SBN_W_ECHO: fw.echo = value; break;
    default: break;
    }
}

static void reset(void)
{
    fn_shutdown();
    memset(&fw, 0, sizeof fw);
    fw.present = 1;
    fw.cs1 = (uint32_t)(CS0_BASE + CS0_SIZE);
    fw.magic = SBN_MAGIC_VALUE;
    fw.version = SBN_ABI_VERSION;
    memset(cards, 0, sizeof cards);
    /* Card 1 is the Visor's own slot; card 2 holds the module. */
    cards[1].removable = 0;
    cards[1].installed = 1;
    cards[2].removable = 1;
    cards[2].installed = 1;
    cards[2].base = (uint32_t)CS0_BASE;
    cards[2].size = (uint32_t)CS0_SIZE;
    hs_calls = 0;
    g_ticks = 0;
}

static void set_reply(const uint8_t *bytes, unsigned n)
{
    memcpy(fw.reply, bytes, n);
    fw.reply_len = n;
}

static uint16_t exchange(const uint8_t *req, uint16_t len, uint8_t *resp, uint16_t cap,
                         uint16_t *got)
{
    return fn_palmos_exchange(req, len, resp, cap, got);
}

static void test_platform_identity(void)
{
    CHECK("platform name", strcmp(fn_platform_name(), "palmos-springboard") == 0);
    CHECK("platform macro", strcmp(FN_PLATFORM_NAME, "palmos") == 0);
}

static void test_open_finds_module(void)
{
    uint32_t probes;

    reset();
    CHECK("not ready before init", fn_transport_ready() == 0);
    CHECK("init", fn_transport_init() == FN_OK);
    CHECK("ready", fn_transport_ready() == 1);
    CHECK("open returns cs1", fn_palmos_open() == fn_palmos_cs1());
    CHECK("cs1 is above the CS0 window", fn_palmos_cs1() == CS0_BASE + CS0_SIZE);
    CHECK("cs0 base", fn_palmos_cs0() == CS0_BASE);
    CHECK("card", fn_palmos_card() == 2);
    probes = hs_calls;
    CHECK("init again", fn_transport_init() == FN_OK);
    CHECK("no second probe", hs_calls == probes);
}

static void test_open_skips_other_cards(void)
{
    reset();
    /* Removable and installed, but nothing answers on its CS1. */
    cards[1].removable = 1;
    cards[1].base = 0x20000000UL;
    cards[1].size = CS0_SIZE;
    CHECK("init past a silent card", fn_transport_init() == FN_OK);
    CHECK("module card", fn_palmos_card() == 2);

    reset();
    cards[2].installed = 0;
    CHECK("not installed", fn_transport_init() == FN_ERR_NOT_FOUND);
    CHECK("not ready", fn_transport_ready() == 0);
}

static void test_open_rejects_module(void)
{
    reset();
    fw.magic = 0x1234;
    CHECK("wrong magic", fn_transport_init() == FN_ERR_NOT_FOUND);
    CHECK("no cs1 kept", fn_palmos_cs1() == 0);

    reset();
    fw.version = SBN_ABI_VERSION - 1;
    CHECK("wrong ABI version", fn_transport_init() == FN_ERR_NOT_FOUND);
    CHECK("not ready", fn_transport_ready() == 0);

    reset();
    fw.present = 0;
    CHECK("no module", fn_transport_init() == FN_ERR_NOT_FOUND);
}

static void test_close_and_reprobe(void)
{
    uint32_t probes;

    reset();
    CHECK("init", fn_init() == FN_OK);
    probes = hs_calls;
    CHECK("init again keeps link", fn_init() == FN_OK && hs_calls == probes);
    fn_shutdown();
    CHECK("shutdown closes link", fn_transport_ready() == 0);
    CHECK("shutdown clears cs1", fn_palmos_cs1() == 0);
    CHECK("re-init", fn_init() == FN_OK);
    CHECK("re-init probed again", hs_calls > probes);

    fn_shutdown();
    fw.present = 0;
    CHECK("module pulled", fn_init() == FN_ERR_NOT_FOUND);
    fw.present = 1;
    CHECK("module back", fn_init() == FN_OK);
}

static void test_exchange_even(void)
{
    static const uint8_t req[6] = {0x70, 0x01, 0x06, 0x00, 0xAB, 0xCD};
    static const uint8_t reply[4] = {1, 2, 3, 4};
    uint8_t resp[16];
    uint16_t got = 99;

    reset();
    fw.resp_seq = 41;
    set_reply(reply, sizeof reply);
    CHECK("init", fn_transport_init() == FN_OK);
    CHECK("exchange",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_OK);
    CHECK("reply length", got == 4);
    CHECK("reply bytes", memcmp(resp, reply, 4) == 0);
    CHECK("BEGIN length", fw.begin_len == 6);
    CHECK("three words", fw.req_words == 3);
    CHECK("bytes two per word, first high",
          fw.req[0] == 0x7001 && fw.req[1] == 0x0600 && fw.req[2] == 0xABCD);
    CHECK("check word", fw.check == check_words(fw.req, 3));
    CHECK("one doorbell", fw.doorbells == 1);
    CHECK("sequence follows the firmware's last reply", fw.seqs[0] == 42);
}

static void test_exchange_odd_request(void)
{
    static const uint8_t req[5] = {1, 2, 3, 4, 5};
    static const uint8_t reply[1] = {9};
    uint8_t resp[4];
    uint16_t got = 0;

    reset();
    set_reply(reply, sizeof reply);
    CHECK("init", fn_transport_init() == FN_OK);
    CHECK("exchange",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_OK);
    CHECK("BEGIN counts bytes", fw.begin_len == 5);
    CHECK("odd byte takes a whole word", fw.req_words == 3);
    CHECK("odd byte padded with 0", fw.req[2] == 0x0500);
    CHECK("check covers the padded word", fw.check == check_words(fw.req, 3));
    CHECK("reply", got == 1 && resp[0] == 9);
}

static void test_reply_lengths(void)
{
    uint8_t reply[SBN_RESP_MAX];
    uint8_t req[2] = {1, 2};
    uint8_t resp[SBN_RESP_MAX + 2];
    uint16_t got;
    unsigned i;

    for (i = 0; i < sizeof reply; i++) reply[i] = (uint8_t)(i * 7 + 3);

    reset();
    CHECK("init", fn_transport_init() == FN_OK);

    set_reply(reply, 7);
    memset(resp, POISON, sizeof resp);
    CHECK("odd reply", exchange(req, 2, resp, 16, &got) == SBN_RES_OK);
    CHECK("odd reply length", got == 7);
    CHECK("odd reply bytes", memcmp(resp, reply, 7) == 0);
    CHECK("odd reply stops at its last byte", resp[7] == POISON);

    set_reply(reply, 8);
    memset(resp, POISON, sizeof resp);
    CHECK("cut to even cap", exchange(req, 2, resp, 4, &got) == SBN_RES_OK);
    CHECK("even cap length", got == 4);
    CHECK("even cap bytes", memcmp(resp, reply, 4) == 0 && resp[4] == POISON);

    memset(resp, POISON, sizeof resp);
    CHECK("cut to odd cap", exchange(req, 2, resp, 5, &got) == SBN_RES_OK);
    CHECK("odd cap length", got == 5);
    CHECK("odd cap bytes", memcmp(resp, reply, 5) == 0 && resp[5] == POISON);

    memset(resp, POISON, sizeof resp);
    CHECK("cap 0", exchange(req, 2, resp, 0, &got) == SBN_RES_OK);
    CHECK("cap 0 keeps nothing", got == 0 && resp[0] == POISON);

    set_reply(reply, 0);
    CHECK("empty reply", exchange(req, 2, resp, 8, &got) == SBN_RES_OK && got == 0);

    set_reply(reply, SBN_RESP_MAX);
    memset(resp, POISON, sizeof resp);
    fw.resp_reads = 0;
    CHECK("largest reply", exchange(req, 2, resp, SBN_RESP_MAX, &got) == SBN_RES_OK);
    CHECK("largest reply bytes", got == SBN_RESP_MAX && memcmp(resp, reply, SBN_RESP_MAX) == 0);
    CHECK("whole window read once", fw.resp_reads == SBN_RESP_MAX / 2);
}

static void test_request_lengths(void)
{
    static uint8_t big[SBN_REQ_MAX + 1];
    uint8_t resp[8];
    uint16_t got = 99;
    unsigned writes;

    reset();
    CHECK("init", fn_transport_init() == FN_OK);
    writes = fw.writes;
    CHECK("empty request", exchange(big, 0, resp, sizeof resp, &got) == SBN_RES_BAD_LEN);
    CHECK("empty request len", got == 0);
    CHECK("too long", exchange(big, SBN_REQ_MAX + 1, resp, sizeof resp, &got) == SBN_RES_BAD_LEN);
    CHECK("nothing sent", fw.writes == writes);
    CHECK("transport maps length error",
          fn_transport_exchange_buffers(big, 0, resp, sizeof resp, &got) == FN_ERR_INVALID);

    set_reply(big, 1);
    CHECK("largest request",
          exchange(big, SBN_REQ_MAX, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("largest request words", fw.begin_len == SBN_REQ_MAX && fw.req_words == SBN_REQ_MAX / 2);
}

static void test_sequence(void)
{
    static const uint8_t req[2] = {1, 2};
    uint8_t resp[4];
    uint16_t got;

    reset();
    fw.resp_seq = 254;
    CHECK("init", fn_transport_init() == FN_OK);
    CHECK("first", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("second", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("third", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("wraps past 0", fw.seqs[0] == 255 && fw.seqs[1] == 1 && fw.seqs[2] == 2);

    reset();
    fw.resp_seq = 255;
    CHECK("init at 255", fn_transport_init() == FN_OK);
    CHECK("exchange", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("0 is never used", fw.seqs[0] == 1);
}

static void test_resend_on_bad_check(void)
{
    static const uint8_t req[4] = {1, 2, 3, 4};
    static const uint8_t reply[2] = {7, 8};
    uint8_t resp[4];
    uint16_t got = 0;

    reset();
    set_reply(reply, sizeof reply);
    CHECK("init", fn_transport_init() == FN_OK);

    fw.corrupt_requests = 1;
    CHECK("damaged once",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_OK);
    CHECK("sent twice", fw.doorbells == 2);
    CHECK("new sequence for the resend", fw.seqs[1] == (uint8_t)(fw.seqs[0] + 1));
    CHECK("reply after resend", got == 2 && resp[0] == 7 && resp[1] == 8);

    fw.doorbells = 0;
    fw.corrupt_requests = 2;
    CHECK("damaged twice",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_OK);
    CHECK("sent three times", fw.doorbells == 3);

    fw.doorbells = 0;
    fw.resp_reads = 0;
    fw.corrupt_requests = 3;
    got = 99;
    CHECK("damaged every time",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_ERR_IO);
    CHECK("gives up after three", fw.doorbells == 3);
    CHECK("no reply read", fw.resp_reads == 0 && got == 0);
}

static void test_reread_on_bad_reply_check(void)
{
    static const uint8_t req[2] = {1, 2};
    static const uint8_t reply[6] = {1, 2, 3, 4, 5, 6};
    uint8_t resp[8];
    uint16_t got = 0;

    reset();
    set_reply(reply, sizeof reply);
    CHECK("init", fn_transport_init() == FN_OK);

    fw.bad_read_passes = 3;
    CHECK("read back on the fourth try",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_OK);
    CHECK("four passes", fw.resp_reads == 4 * 3);
    CHECK("one request", fw.doorbells == 1);
    CHECK("good bytes", got == 6 && memcmp(resp, reply, 6) == 0);

    fw.resp_reads = 0;
    fw.doorbells = 0;
    fw.bad_read_passes = 4;
    got = 99;
    CHECK("never reads back",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_ERR_IO);
    CHECK("gives up after four passes", fw.resp_reads == 4 * 3);
    CHECK("no length reported", got == 0);
    CHECK("not sent again", fw.doorbells == 1);
    fw.bad_read_passes = 4;
    CHECK("raw code", exchange(req, 2, resp, sizeof resp, &got) == FN_PALMOS_BAD_READ);
}

static void check_mapping(uint16_t fw_result, uint8_t expected, const char *name)
{
    static const uint8_t req[2] = {1, 2};
    static const uint8_t reply[2] = {5, 6};
    uint8_t resp[4];
    uint16_t got = 99;

    fw.doorbells = 0;
    fw.resp_reads = 0;
    fw.forced_result = fw_result;
    set_reply(reply, sizeof reply);
    CHECK(name, fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) ==
                    expected);
    CHECK("no resend", fw.doorbells == 1);
    CHECK("reply left alone", fw.resp_reads == 0);
    CHECK("no length", got == 0);
}

static void test_result_mapping(void)
{
    reset();
    CHECK("init", fn_transport_init() == FN_OK);
    check_mapping(SBN_RES_NO_HOST, FN_ERR_TRANSPORT, "NIO not running");
    check_mapping(SBN_RES_TIMEOUT, FN_ERR_TIMEOUT, "NIO silent");
    check_mapping(SBN_RES_UNANSWERED, FN_ERR_TIMEOUT, "NIO took it, no answer");
    check_mapping(SBN_RES_REFUSED, FN_ERR_IO, "link refused it");
    check_mapping(SBN_RES_BAD_LEN, FN_ERR_INVALID, "bad length");
    check_mapping(SBN_RES_TOO_BIG, FN_ERR_IO, "reply too big");
    check_mapping(SBN_RES_ABORTED, FN_ERR_ABORTED, "aborted");
    check_mapping(77, FN_ERR_IO, "unknown result");
}

static void test_no_answer(void)
{
    static const uint8_t req[2] = {1, 2};
    static const uint8_t reply[1] = {3};
    uint8_t resp[4];
    uint16_t got = 99;
    uint32_t spent;

    reset();
    fw.resp_seq = 5;
    set_reply(reply, sizeof reply);
    CHECK("init", fn_transport_init() == FN_OK);

    fw.silent_doorbells = 1;
    g_ticks = 0xFFFFFF00UL;
    CHECK("raw code", exchange(req, 2, resp, sizeof resp, &got) == FN_PALMOS_NO_ANSWER);
    spent = (uint32_t)(g_ticks - 0xFFFFFF00UL);
    CHECK("waited 35 s of ticks, across a tick wrap", spent >= 3500 && spent < 3600);
    CHECK("sent once", fw.doorbells == 1);
    CHECK("stale reply not taken", fw.resp_seq == 5 && fw.seqs[0] == 6);

    fw.silent_doorbells = 1;
    got = 99;
    CHECK("transport timeout",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) == FN_ERR_TIMEOUT);
    CHECK("no length", got == 0);

    CHECK("next exchange works", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("sequence moved on", fw.seqs[2] == 8);

    fw.late_polls = 500;
    CHECK("late reply", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK && got == 1);
}

static void test_not_open(void)
{
    static const uint8_t req[2] = {1, 2};
    uint8_t resp[4];
    uint16_t got = 99;
    unsigned traffic;

    reset();
    traffic = fw.reads + fw.writes;
    CHECK("transport: not found",
          fn_transport_exchange_buffers(req, sizeof req, resp, sizeof resp, &got) ==
              FN_ERR_NOT_FOUND);
    CHECK("length cleared", got == 0);
    CHECK("no bus traffic", fw.reads + fw.writes == traffic);

    CHECK("read of a closed link", fn_palmos_read(SBN_R_MAGIC) == 0);
    fn_palmos_write(SBN_W_ECHO, 0x1234);
    fn_palmos_log("x");
    CHECK("closed link stays quiet", fw.reads + fw.writes == traffic);

    CHECK("raw exchange looks for the module", exchange(req, 2, resp, sizeof resp, &got) == SBN_RES_OK);
    CHECK("and opened it", fn_palmos_cs1() != 0);

    fn_palmos_close();
    fw.present = 0;
    CHECK("no module: no answer",
          exchange(req, 2, resp, sizeof resp, &got) == FN_PALMOS_NO_ANSWER && got == 0);
}

static void test_invalid_arguments(void)
{
    static const uint8_t req[2] = {1, 2};
    uint8_t resp[4];
    uint16_t got = 99;
    unsigned doorbells;

    reset();
    CHECK("init", fn_transport_init() == FN_OK);
    doorbells = fw.doorbells;
    CHECK("null length",
          fn_transport_exchange_buffers(req, 2, resp, sizeof resp, NULL) == FN_ERR_INVALID);
    CHECK("null request",
          fn_transport_exchange_buffers(NULL, 2, resp, sizeof resp, &got) == FN_ERR_INVALID);
    CHECK("null request clears length", got == 0);
    got = 99;
    CHECK("null response",
          fn_transport_exchange_buffers(req, 2, NULL, sizeof resp, &got) == FN_ERR_INVALID);
    CHECK("null response clears length", got == 0);
    CHECK("nothing sent", fw.doorbells == doorbells);
}

static void test_registers_and_log(void)
{
    reset();
    CHECK("init", fn_transport_init() == FN_OK);
    CHECK("read magic", fn_palmos_read(SBN_R_MAGIC) == SBN_MAGIC_VALUE);
    fn_palmos_write(SBN_W_ECHO, 0xBEEF);
    CHECK("write echo", fn_palmos_read(SBN_R_ECHO) == 0xBEEF);

    fn_palmos_log("");
    CHECK("empty line", fw.log_words == 1 && fw.log[0] == 0x0A00);
    fw.log_words = 0;
    fn_palmos_log("A");
    CHECK("one character", fw.log_words == 1 && fw.log[0] == 0x410A);
    fw.log_words = 0;
    fn_palmos_log("AB");
    CHECK("two characters", fw.log_words == 2 && fw.log[0] == 0x4142 && fw.log[1] == 0x0A00);
    fw.log_words = 0;
    fn_palmos_log("ABC");
    CHECK("three characters", fw.log_words == 2 && fw.log[0] == 0x4142 && fw.log[1] == 0x430A);
}

/* The same path an application takes: fn_init, then a library call. */
static void test_through_the_library(void)
{
    uint8_t request = 0x24;
    uint8_t reply = 0;
    uint8_t answer[FN_HEADER_SIZE + 1];
    fn_raw_response_t response;

    reset();
    fn_build_header(answer, FN_DEVICE_FILE, 4, (uint16_t)(FN_HEADER_SIZE + 1));
    answer[FN_HEADER_SIZE] = 0x42;
    answer[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(answer, (uint16_t)sizeof answer);
    set_reply(answer, sizeof answer);

    CHECK("fn_init", fn_init() == FN_OK);
    CHECK("fn_raw_call",
          fn_raw_call(FN_DEVICE_FILE, 4, &request, 1, &reply, 1, &response) == FN_OK);
    CHECK("payload", response.payload_length == 1 && reply == 0x42);
    CHECK("seven-byte packet went out padded",
          fw.begin_len == FN_HEADER_SIZE + 1 && fw.req_words == (FN_HEADER_SIZE + 2) / 2);
    CHECK("device and command first", fw.req[0] == (uint16_t)(FN_DEVICE_FILE << 8 | 4));
    CHECK("payload last, low byte padded", fw.req[3] == 0x2400);
    CHECK("one exchange", fw.doorbells == 1);

    fw.silent_doorbells = 1;
    CHECK("fn_raw_call times out",
          fn_raw_call(FN_DEVICE_FILE, 4, &request, 1, &reply, 1, &response) == FN_ERR_TIMEOUT);
    CHECK("and is not replayed", fw.doorbells == 2);
}

int main(void)
{
    test_platform_identity();
    test_open_finds_module();
    test_open_skips_other_cards();
    test_open_rejects_module();
    test_close_and_reprobe();
    test_exchange_even();
    test_exchange_odd_request();
    test_reply_lengths();
    test_request_lengths();
    test_sequence();
    test_resend_on_bad_check();
    test_reread_on_bad_reply_check();
    test_result_mapping();
    test_no_answer();
    test_not_open();
    test_invalid_arguments();
    test_registers_and_log();
    test_through_the_library();

    if (failures != 0) {
        fprintf(stderr, "%u checks failed\n", failures);
        return 1;
    }
    return 0;
}
