/* Host test for the classic Macintosh transport (src/platform/mac68k/fn_transport.c).
 *
 * The stub Files.h, Devices.h and Events.h stand in for the Toolbox. This file
 * plays the drive queue, the .Sony driver's HD20 units and floppy drives, a
 * SCSI disk, and the FujiNet board's controller behind the mailbox, following
 * docs/mac68k/hd20-mailbox.md. */

#include <Devices.h>
#include <Events.h>
#include <Files.h>

#include "fn_internal.h"
#include "fn_platform.h"
#include "fujinet-nio.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned failures;

#define CHECK(name, expression) do {                                      \
    if (!(expression)) {                                                  \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__);          \
        ++failures;                                                       \
    }                                                                     \
} while (0)

#define BLOCK 512
#define MAILBOX_BLOCKS 16
#define MAILBOX_BYTES (MAILBOX_BLOCKS * BLOCK)
#define HEADER 16
#define SONY (-5)
#define SCSI (-33)

/* ---- The drives ---------------------------------------------------------- */

typedef struct {
    DrvQEl q;
    long blocks;
    uint8_t tail[MAILBOX_BYTES];  /* the drive's last 16 blocks */
    unsigned reads;               /* block calls that reached it */
    unsigned writes;
} FakeDrive;

static FakeDrive g_drives[6];
static int g_drive_count;
static FakeDrive *g_fujinet;      /* the drive the controller answers on */

QElemPtr mac_test_drive_queue(void)
{
    return g_drive_count ? (QElemPtr)&g_drives[0].q : NULL;
}

static void put_header(uint8_t *at, uint8_t state, uint16_t seq, uint32_t length)
{
    memset(at, 0, HEADER);
    memcpy(at, "FNIO", 4);
    at[4] = 1;
    at[5] = state;
    at[6] = (uint8_t)(seq >> 8);
    at[7] = (uint8_t)seq;
    at[8] = (uint8_t)(length >> 24);
    at[9] = (uint8_t)(length >> 16);
    at[10] = (uint8_t)(length >> 8);
    at[11] = (uint8_t)length;
}

/* A drive at the end of the queue. With a mailbox, its last 16 blocks start
 * with the controller's header, last sequence number `seq`. */
static FakeDrive *add_drive(short refnum, long blocks, int mailbox, uint16_t seq)
{
    FakeDrive *d = &g_drives[g_drive_count];

    memset(d, 0, sizeof(*d));
    d->q.dQDrive = (short)(g_drive_count + 1);
    d->q.dQRefNum = refnum;
    d->q.qType = blocks > 0xFFFF ? 1 : 0;
    d->q.dQDrvSz = (unsigned short)(blocks & 0xFFFF);
    d->q.dQDrvSz2 = (unsigned short)(blocks >> 16);
    d->blocks = blocks;
    if (mailbox) put_header(d->tail, 1, seq, 0);
    if (g_drive_count > 0) g_drives[g_drive_count - 1].q.qLink = (QElemPtr)&d->q;
    ++g_drive_count;
    return d;
}

/* ---- The controller -------------------------------------------------------- */

enum { ANSWER, SILENT, STALE_FIRST, NO_MAGIC };

static struct {
    int mode;
    unsigned wait_polls;        /* polls that see the request unanswered first */
    unsigned stale_polls;       /* STALE_FIRST: polls that see an older response */
    uint8_t response[MAILBOX_BYTES];
    uint32_t response_length;   /* as the header gives it */
    uint8_t request[MAILBOX_BYTES];
    long request_bytes;         /* bytes of the last write */
    long request_block;         /* the block it started at */
    unsigned writes;            /* write calls to the mailbox */
    unsigned polls;             /* reads of the header block since the last write */
    unsigned rest_reads;        /* reads of the blocks after the header */
    unsigned fail_write;        /* fail this many writes */
    unsigned fail_poll;         /* fail the n-th poll (1-based) */
    unsigned fail_rest;         /* fail the read of the blocks after the header */
} g_ctl;

static uint16_t request_seq(void)
{
    return (uint16_t)((g_ctl.request[6] << 8) | g_ctl.request[7]);
}

/* What the controller has in the header block when the Mac polls it. Until
 * the first request it is the header the drive started with. */
static void controller_poll(FakeDrive *d)
{
    if (g_ctl.writes == 0)
        return;
    ++g_ctl.polls;
    if (g_ctl.mode == NO_MAGIC) {
        memcpy(d->tail, "XXXX", 4);
        return;
    }
    if (g_ctl.mode == SILENT || g_ctl.polls <= g_ctl.wait_polls)
        return;  /* still the request, state 0 */
    if (g_ctl.mode == STALE_FIRST && g_ctl.polls <= g_ctl.wait_polls + g_ctl.stale_polls) {
        put_header(d->tail, 1, (uint16_t)(request_seq() - 1), 3);
        return;
    }
    put_header(d->tail, 1, request_seq(), g_ctl.response_length);
    memcpy(d->tail + HEADER, g_ctl.response,
           g_ctl.response_length + HEADER <= MAILBOX_BYTES ? g_ctl.response_length : MAILBOX_BYTES - HEADER);
}

/* ---- The Toolbox ------------------------------------------------------------ */

static unsigned long g_ticks;
static unsigned long g_tick_step;
static unsigned g_bad_calls;     /* calls a real driver would refuse */

unsigned long TickCount(void)
{
    g_ticks += g_tick_step;
    return g_ticks;
}

static FakeDrive *drive_for(const ParamBlockRec *pb)
{
    int i;

    for (i = 0; i < g_drive_count; ++i)
        if (g_drives[i].q.dQDrive == pb->ioParam.ioVRefNum &&
            g_drives[i].q.dQRefNum == pb->ioParam.ioRefNum)
            return &g_drives[i];
    return NULL;
}

/* Common checks; the block range within the drive's last 16 blocks, or -1. */
static long tail_offset(FakeDrive *d, const ParamBlockRec *pb)
{
    long first = pb->ioParam.ioPosOffset / BLOCK;
    long count = pb->ioParam.ioReqCount / BLOCK;

    if (pb->ioParam.ioPosMode != fsFromStart || pb->ioParam.ioReqCount % BLOCK != 0 ||
        pb->ioParam.ioPosOffset % BLOCK != 0 || count < 1 ||
        ((uintptr_t)pb->ioParam.ioBuffer & 1) != 0) {  /* an address error on a 68000 */
        ++g_bad_calls;
        return -1;
    }
    if (first < d->blocks - MAILBOX_BLOCKS || first + count > d->blocks) {
        ++g_bad_calls;  /* the transport only ever touches the last 16 blocks */
        return -1;
    }
    return (first - (d->blocks - MAILBOX_BLOCKS)) * BLOCK;
}

OSErr PBReadSync(ParamBlockRec *pb)
{
    FakeDrive *d = drive_for(pb);
    long at;

    if (d == NULL) { ++g_bad_calls; return ioErr; }
    ++d->reads;
    at = tail_offset(d, pb);
    if (at < 0) return ioErr;
    if (d == g_fujinet && at == 0) {
        controller_poll(d);
        if (g_ctl.fail_poll && g_ctl.polls == g_ctl.fail_poll) return ioErr;
    } else if (d == g_fujinet) {
        ++g_ctl.rest_reads;
        if (g_ctl.fail_rest) return ioErr;
    }
    memcpy(pb->ioParam.ioBuffer, d->tail + at, (size_t)pb->ioParam.ioReqCount);
    return noErr;
}

OSErr PBWriteSync(ParamBlockRec *pb)
{
    FakeDrive *d = drive_for(pb);
    long at;

    if (d == NULL) { ++g_bad_calls; return ioErr; }
    ++d->writes;
    at = tail_offset(d, pb);
    if (at < 0) return ioErr;
    if (g_ctl.fail_write) { --g_ctl.fail_write; return ioErr; }
    memcpy(d->tail + at, pb->ioParam.ioBuffer, (size_t)pb->ioParam.ioReqCount);
    if (d == g_fujinet) {
        ++g_ctl.writes;
        memcpy(g_ctl.request, pb->ioParam.ioBuffer, (size_t)pb->ioParam.ioReqCount);
        g_ctl.request_bytes = pb->ioParam.ioReqCount;
        g_ctl.request_block = pb->ioParam.ioPosOffset / BLOCK;
        g_ctl.polls = 0;
        g_ctl.rest_reads = 0;
    }
    return noErr;
}

/* ---- Helpers ------------------------------------------------------------------ */

static uint8_t g_request[MAILBOX_BYTES];
static uint8_t g_response[MAILBOX_BYTES];

static void reset(void)
{
    fn_transport_close();
    memset(g_drives, 0, sizeof(g_drives));
    g_drive_count = 0;
    g_fujinet = NULL;
    memset(&g_ctl, 0, sizeof(g_ctl));
    g_ctl.mode = ANSWER;
    g_ticks = 1000;
    g_tick_step = 1;
    g_bad_calls = 0;
}

/* The usual Mac: an internal floppy, a SCSI disk, then the FujiNet unit. */
static void usual_mac(uint16_t seq)
{
    add_drive(SONY, 1600, 0, 0);
    add_drive(SCSI, 80000, 0, 0);
    g_fujinet = add_drive(SONY, 81936, 1, seq);
}

static void fill(uint8_t *buf, size_t length, uint8_t salt)
{
    size_t i;

    for (i = 0; i < length; ++i) buf[i] = (uint8_t)(i * 7 + salt);
}

static void set_response(uint32_t length, uint8_t salt)
{
    fill(g_ctl.response, length <= sizeof(g_ctl.response) ? length : sizeof(g_ctl.response), salt);
    g_ctl.response_length = length;
}

static uint8_t exchange(uint16_t request_length, uint16_t capacity, uint16_t *got)
{
    fill(g_request, request_length, 0x11);
    memset(g_response, 0xEE, sizeof(g_response));
    return fn_transport_exchange_buffers(g_request, request_length, g_response, capacity, got);
}

/* ---- Tests -------------------------------------------------------------------- */

static void test_probe_reads_only_hd20_units(void)
{
    FakeDrive *floppy, *scsi, *plain;
    uint16_t got = 0;

    reset();
    /* The floppy and the SCSI disk even carry the header: they still mustn't be read. */
    floppy = add_drive(SONY, 1600, 1, 7);
    scsi = add_drive(SCSI, 2000, 1, 7);
    add_drive(SONY, 2880, 1, 7);
    plain = add_drive(SONY, 4000, 0, 0);        /* an HD20 with no mailbox */
    g_fujinet = add_drive(SONY, 81936, 1, 9);   /* past 65535 blocks: a 32-bit size */

    CHECK("probe: init finds the FujiNet unit", fn_transport_init() == FN_OK);
    CHECK("probe: ready", fn_transport_ready() == 1);
    CHECK("probe: the floppy is not read", floppy->reads == 0);
    CHECK("probe: the 1.4 MB floppy is not read", g_drives[2].reads == 0);
    CHECK("probe: the SCSI disk is not read", scsi->reads == 0);
    CHECK("probe: the other HD20 is read once", plain->reads == 1);
    CHECK("probe: the FujiNet unit is read once", g_fujinet->reads == 1);

    set_response(5, 1);
    CHECK("probe: the exchange goes to the FujiNet unit", exchange(10, 64, &got) == FN_OK);
    CHECK("probe: and nowhere else", plain->writes == 0 && g_ctl.writes == 1);
    CHECK("probe: the mailbox is the unit's last 16 blocks", g_ctl.request_block == 81936 - 16);
    CHECK("probe: no call a driver would refuse", g_bad_calls == 0);
}

static void test_no_mailbox_is_not_found(void)
{
    uint16_t got = 1;

    reset();
    add_drive(SONY, 1600, 0, 0);
    add_drive(SCSI, 2000, 1, 0);
    add_drive(SONY, 4000, 0, 0);
    add_drive(SONY, 10, 0, 0);  /* too small to hold one */
    CHECK("none: init", fn_transport_init() == FN_ERR_NOT_FOUND);
    CHECK("none: not ready", fn_transport_ready() == 0);
    CHECK("none: exchange", exchange(4, 64, &got) == FN_ERR_NOT_FOUND && got == 0);
    reset();
    CHECK("none: an empty drive queue", fn_transport_init() == FN_ERR_NOT_FOUND);
}

static void test_request_layout_and_a_two_block_response(void)
{
    uint16_t got = 0;
    uint8_t expected[700];

    reset();
    usual_mac(0x1234);
    CHECK("layout: init", fn_transport_init() == FN_OK);
    set_response(700, 3);
    CHECK("layout: exchange", exchange(600, sizeof(g_response), &got) == FN_OK);

    /* One write of header + packet, in whole blocks: 16 + 600 bytes is two. */
    CHECK("layout: one write", g_ctl.writes == 1);
    CHECK("layout: two blocks written", g_ctl.request_bytes == 2 * BLOCK);
    CHECK("layout: magic", memcmp(g_ctl.request, "FNIO", 4) == 0);
    CHECK("layout: version 1", g_ctl.request[4] == 1);
    CHECK("layout: state 0", g_ctl.request[5] == 0);
    CHECK("layout: next sequence number", request_seq() == 0x1235);
    CHECK("layout: length, 32 bits big-endian",
          g_ctl.request[8] == 0 && g_ctl.request[9] == 0 && g_ctl.request[10] == 0x02 && g_ctl.request[11] == 0x58);
    CHECK("layout: reserved bytes zero",
          g_ctl.request[12] == 0 && g_ctl.request[13] == 0 && g_ctl.request[14] == 0 && g_ctl.request[15] == 0);
    CHECK("layout: packet after the header", memcmp(g_ctl.request + HEADER, g_request, 600) == 0);

    /* The response: header block polled, then the rest read once. */
    fill(expected, sizeof(expected), 3);
    CHECK("layout: response length", got == 700);
    CHECK("layout: response bytes", memcmp(g_response, expected, 700) == 0);
    CHECK("layout: the rest read once", g_ctl.rest_reads == 1);
    CHECK("layout: no call a driver would refuse", g_bad_calls == 0);
}

static void test_a_one_block_response_needs_no_second_read(void)
{
    uint16_t got = 0;

    reset();
    usual_mac(1);
    fn_transport_init();
    set_response(BLOCK - HEADER, 5);
    CHECK("one block: exchange", exchange(4, sizeof(g_response), &got) == FN_OK);
    CHECK("one block: length", got == BLOCK - HEADER);
    CHECK("one block: no second read", g_ctl.rest_reads == 0);
}

static void test_sequence_numbers_follow_on_and_wrap(void)
{
    uint16_t got = 0;

    reset();
    usual_mac(0xFFFE);
    fn_transport_init();
    set_response(1, 0);
    exchange(1, 64, &got);
    CHECK("seq: carries on from the controller's", request_seq() == 0xFFFF);
    exchange(1, 64, &got);
    CHECK("seq: wraps to 0", request_seq() == 0x0000);
    exchange(1, 64, &got);
    CHECK("seq: then 1", request_seq() == 0x0001);
}

static void test_waits_while_the_request_is_unanswered(void)
{
    uint16_t got = 0;

    reset();
    usual_mac(1);
    fn_transport_init();
    g_ctl.wait_polls = 25;
    set_response(8, 9);
    CHECK("wait: exchange", exchange(4, 64, &got) == FN_OK && got == 8);
    CHECK("wait: polled until answered", g_ctl.polls == 26);
}

static void test_a_stale_response_is_ignored(void)
{
    uint16_t got = 0;
    uint8_t expected[40];

    reset();
    usual_mac(1);
    fn_transport_init();
    /* Left from an earlier request that timed out: state 1, the previous number. */
    g_ctl.mode = STALE_FIRST;
    g_ctl.wait_polls = 2;
    g_ctl.stale_polls = 5;
    set_response(40, 4);
    CHECK("stale: exchange", exchange(4, 64, &got) == FN_OK);
    fill(expected, sizeof(expected), 4);
    CHECK("stale: the matching response", got == 40 && memcmp(g_response, expected, 40) == 0);
    CHECK("stale: polled past the stale ones", g_ctl.polls == 8);
}

static void test_an_unanswered_request_times_out_after_35_seconds(void)
{
    uint16_t got = 1;
    unsigned long start;

    reset();
    usual_mac(1);
    fn_transport_init();
    g_ctl.mode = SILENT;
    g_tick_step = 60;  /* a second a poll */
    start = g_ticks;
    CHECK("timeout: FN_ERR_TIMEOUT", exchange(4, 64, &got) == FN_ERR_TIMEOUT);
    CHECK("timeout: nothing returned", got == 0);
    CHECK("timeout: after 35 s", g_ticks - start > 35 * 60 && g_ticks - start <= 37 * 60);

    /* The next request still works, with the next number. */
    g_ctl.mode = ANSWER;
    g_tick_step = 1;
    set_response(2, 0);
    CHECK("timeout: the next exchange", exchange(4, 64, &got) == FN_OK && got == 2);
    CHECK("timeout: with the next number", request_seq() == 3);
}

static void test_sizes_are_bounded(void)
{
    uint16_t got = 1;
    unsigned writes;

    reset();
    usual_mac(1);
    fn_transport_init();

    set_response(1, 0);
    CHECK("size: the largest request fills the mailbox",
          exchange(MAILBOX_BYTES - HEADER, 64, &got) == FN_OK && g_ctl.request_bytes == MAILBOX_BYTES);
    writes = g_ctl.writes;
    CHECK("size: one byte more is refused", exchange(MAILBOX_BYTES - HEADER + 1, 64, &got) == FN_ERR_INVALID);
    CHECK("size: and never written", g_ctl.writes == writes);

    set_response(MAILBOX_BYTES - HEADER, 6);
    CHECK("size: the largest response",
          exchange(4, MAILBOX_BYTES - HEADER, &got) == FN_OK && got == MAILBOX_BYTES - HEADER);

    set_response(65, 0);
    CHECK("size: a response larger than the buffer", exchange(4, 64, &got) == FN_ERR_INVALID && got == 0);
    set_response(MAILBOX_BYTES - HEADER + 1, 0);
    CHECK("size: a length past the mailbox", exchange(4, 60000, &got) == FN_ERR_INVALID && got == 0);
    set_response(0x80000010UL, 0);
    CHECK("size: a huge 32-bit length", exchange(4, 60000, &got) == FN_ERR_INVALID);
}

static void test_io_failures(void)
{
    uint16_t got = 1;

    reset();
    usual_mac(1);
    fn_transport_init();
    set_response(700, 0);

    g_ctl.fail_write = 1;
    CHECK("io: the write fails", exchange(4, sizeof(g_response), &got) == FN_ERR_IO && got == 0);
    g_ctl.fail_poll = 1;
    CHECK("io: a poll fails", exchange(4, sizeof(g_response), &got) == FN_ERR_IO);
    g_ctl.fail_poll = 0;
    g_ctl.fail_rest = 1;
    CHECK("io: the rest of the response fails", exchange(4, sizeof(g_response), &got) == FN_ERR_IO);
    g_ctl.fail_rest = 0;
    g_ctl.mode = NO_MAGIC;
    CHECK("io: the header is gone", exchange(4, sizeof(g_response), &got) == FN_ERR_IO);
    g_ctl.mode = ANSWER;
    CHECK("io: and recovers", exchange(4, sizeof(g_response), &got) == FN_OK && got == 700);
}

static void test_arguments_and_the_context_call(void)
{
    uint16_t got = 1;

    reset();
    usual_mac(1);
    fn_transport_init();
    CHECK("args: no length pointer", fn_transport_exchange_buffers(g_request, 1, g_response, 8, NULL) == FN_ERR_INVALID);
    CHECK("args: no request", fn_transport_exchange_buffers(NULL, 1, g_response, 8, &got) == FN_ERR_INVALID);
    CHECK("args: no response", fn_transport_exchange_buffers(g_request, 1, NULL, 8, &got) == FN_ERR_INVALID);

    set_response(12, 2);
    fill(g_request, 20, 0x30);
    _fn_transport_ctx.request = g_request;
    _fn_transport_ctx.req_len = 20;
    _fn_transport_ctx.response = g_response;
    _fn_transport_ctx.resp_max = 64;
    _fn_transport_ctx.resp_len = 0;
    CHECK("ctx: fn_transport_exchange", fn_transport_exchange() == FN_OK);
    CHECK("ctx: response length", _fn_transport_ctx.resp_len == 12);
    CHECK("ctx: request sent", memcmp(g_ctl.request + HEADER, g_request, 20) == 0);
}

static void test_close_and_init_again(void)
{
    reset();
    usual_mac(1);
    CHECK("close: init", fn_transport_init() == FN_OK);
    CHECK("close: init again is a no-op", fn_transport_init() == FN_OK && g_fujinet->reads == 1);
    fn_transport_close();
    CHECK("close: not ready", fn_transport_ready() == 0);
    CHECK("close: init probes again", fn_transport_init() == FN_OK && g_fujinet->reads == 2);
    CHECK("platform name", strcmp(fn_platform_name(), "mac68k") == 0);
}

int main(void)
{
    test_probe_reads_only_hd20_units();
    test_no_mailbox_is_not_found();
    test_request_layout_and_a_two_block_response();
    test_a_one_block_response_needs_no_second_read();
    test_sequence_numbers_follow_on_and_wrap();
    test_waits_while_the_request_is_unanswered();
    test_a_stale_response_is_ignored();
    test_an_unanswered_request_times_out_after_35_seconds();
    test_sizes_are_bounded();
    test_io_failures();
    test_arguments_and_the_context_call();
    test_close_and_init_again();

    if (failures) {
        fprintf(stderr, "mac68k transport host test: %u failure(s)\n", failures);
        return 1;
    }
    printf("mac68k transport host test: OK\n");
    return 0;
}
