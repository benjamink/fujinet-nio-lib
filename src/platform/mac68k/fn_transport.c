/*
 * fn_transport.c - classic Macintosh (68k) transport over the floppy port
 *
 * The FujiNet Mac board presents its disks to the Mac as Directly Connected
 * Disks (HD20s) on the floppy port. Its floppy-port controller makes every
 * such unit FN_MAC_MAILBOX_BLOCKS blocks longer than its image; those blocks
 * are a mailbox: writing them sends one raw FujiBus packet, which the
 * controller forwards to fujinet-nio over its packet link, and reading them
 * returns the response. So the whole transport is ordinary driver-level
 * block I/O through the ROM's HD20 support in the .Sony driver: no INIT, no
 * driver, no hardware access.
 *
 * The mailbox contract (header, states, sequence numbers, sizes, timing) is
 * docs/mac68k/hd20-mailbox.md. tests/mac68k_transport_host_test.c runs this
 * file on the host against a fake drive queue and controller.
 *
 * Compiler: Retro68 (m68k-apple-macos-gcc), for the 68000: any Mac whose ROM
 * drives HD20s (512Ke, Plus, SE and later).
 */

#include <Files.h>
#include <Devices.h>
#include <Events.h>

#include <string.h>

#include "fujinet-nio.h"
#include "fn_platform.h"
#include "fn_internal.h"

#define FN_MAC_MAILBOX_BLOCKS 16
#define FN_MAC_BLOCK 512
#define FN_MAC_HEADER 16
#define FN_MAC_MAILBOX_BYTES (FN_MAC_MAILBOX_BLOCKS * FN_MAC_BLOCK)
#define FN_MAC_VERSION 1

/* The header's fields: "FNIO", then these offsets, big-endian. */
#define HDR_VERSION 4
#define HDR_STATE 5
#define HDR_SEQ 6     /* 16 bits */
#define HDR_LENGTH 8  /* 32 bits */

/* State 0 is a request the controller hasn't answered; the controller sets
 * 1 once the response for that sequence number is in the mailbox. The
 * mailbox has no error state: NIO's errors come back inside the FujiBus
 * response, and a request that gets no response times out. */
enum { MB_REQUEST = 0, MB_READY = 1 };

/* The ROM's .Sony driver, which runs the floppy drives and the HD20s. */
#define FN_MAC_SONY_REFNUM (-5)

/* How long to wait for a response, in ticks (60 a second): 35 s, as on Palm
 * OS, so a slow request through NIO, such as an HTTP fetch, can finish. A
 * request that times out may still run on the far side. */
#ifndef FN_MAC_TIMEOUT_TICKS
#define FN_MAC_TIMEOUT_TICKS (60UL * 35)
#endif

/* The head of the drive queue. DrvQHdr is a low-memory global at $308, and
 * GetDrvQHdr() is glue that Retro68 does not ship. A host test defines
 * FN_MAC_DRVQ_HEAD() to return a fake queue instead. */
#ifndef FN_MAC_DRVQ_HEAD
/* GCC flags any fixed-address access as out of bounds. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
static QElemPtr drive_queue_head(void)
{
    return ((QHdrPtr)0x0308)->qHead;
}
#pragma GCC diagnostic pop
#define FN_MAC_DRVQ_HEAD() drive_queue_head()
#endif

static struct {
    short drive;      /* drive number in the drive queue */
    short refnum;     /* its driver (.Sony) */
    long base;        /* first mailbox block */
    uint16_t seq;
    uint8_t ready;
} g_mac;

/* Word aligned: the driver moves words into it, and an odd address is an
 * address error on a 68000. */
static uint8_t g_blocks[FN_MAC_MAILBOX_BYTES] __attribute__((aligned(4)));

static OSErr block_io(short trap_write, long block, uint8_t *buf, long count)
{
    ParamBlockRec pb;

    memset(&pb, 0, sizeof(pb));
    pb.ioParam.ioRefNum = g_mac.refnum;
    pb.ioParam.ioVRefNum = g_mac.drive;
    pb.ioParam.ioBuffer = (Ptr)buf;
    pb.ioParam.ioReqCount = count * FN_MAC_BLOCK;
    pb.ioParam.ioPosMode = fsFromStart;
    pb.ioParam.ioPosOffset = block * FN_MAC_BLOCK;
    return trap_write ? PBWriteSync(&pb) : PBReadSync(&pb);
}

static long drive_blocks(const DrvQEl *q)
{
    /* qType 1: the size is 32 bits, dQDrvSz2 the high word. */
    if (q->qType == 1)
        return ((long)q->dQDrvSz2 << 16) | q->dQDrvSz;
    return q->dQDrvSz;
}

/* Only an HD20 unit can hold the mailbox, so only those are read. A SCSI
 * disk has its own driver. A floppy drive is .Sony's too, but holds 800,
 * 1440, 1600 or 2880 blocks, and a mailbox unit is its image plus 16, so
 * neither is spun up and sought. */
static uint8_t may_hold_mailbox(const DrvQEl *q, long blocks)
{
    if (q->dQRefNum != FN_MAC_SONY_REFNUM || blocks <= FN_MAC_MAILBOX_BLOCKS)
        return 0;
    return blocks != 800 && blocks != 1440 && blocks != 1600 && blocks != 2880;
}

static uint16_t header_seq(void)
{
    return (uint16_t)((g_blocks[HDR_SEQ] << 8) | g_blocks[HDR_SEQ + 1]);
}

uint8_t fn_transport_init(void)
{
    DrvQEl *q;

    if (g_mac.ready)
        return FN_OK;

    for (q = (DrvQEl *)FN_MAC_DRVQ_HEAD(); q != NULL; q = (DrvQEl *)q->qLink) {
        long blocks = drive_blocks(q);

        if (!may_hold_mailbox(q, blocks))
            continue;
        g_mac.drive = q->dQDrive;
        g_mac.refnum = q->dQRefNum;
        g_mac.base = blocks - FN_MAC_MAILBOX_BLOCKS;
        if (block_io(0, g_mac.base, g_blocks, 1) == noErr &&
            memcmp(g_blocks, "FNIO", 4) == 0) {
            /* Carry on from the controller's last sequence number, so a
             * response left from before can't match the next request. */
            g_mac.seq = header_seq();
            g_mac.ready = 1;
            return FN_OK;
        }
    }
    return FN_ERR_NOT_FOUND;
}

void fn_transport_close(void)
{
    g_mac.ready = 0;
}

uint8_t fn_transport_ready(void)
{
    return g_mac.ready;
}

uint8_t fn_transport_exchange(void)
{
    return fn_transport_exchange_buffers(_fn_transport_ctx.request,
                                         _fn_transport_ctx.req_len,
                                         _fn_transport_ctx.response,
                                         _fn_transport_ctx.resp_max,
                                         &_fn_transport_ctx.resp_len);
}

uint8_t fn_transport_exchange_buffers(const uint8_t *request,
                                      uint16_t request_length,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length)
{
    long nblocks;
    unsigned long start;
    uint32_t length;
    uint16_t seq;

    if (response_length == NULL || request == NULL || response == NULL)
        return FN_ERR_INVALID;
    *response_length = 0;
    if (!g_mac.ready)
        return FN_ERR_NOT_FOUND;
    if ((long)request_length + FN_MAC_HEADER > FN_MAC_MAILBOX_BYTES)
        return FN_ERR_INVALID;

    seq = ++g_mac.seq;
    memset(g_blocks, 0, FN_MAC_HEADER);
    memcpy(g_blocks, "FNIO", 4);
    g_blocks[HDR_VERSION] = FN_MAC_VERSION;
    g_blocks[HDR_STATE] = MB_REQUEST;
    g_blocks[HDR_SEQ] = (uint8_t)(seq >> 8);
    g_blocks[HDR_SEQ + 1] = (uint8_t)seq;
    g_blocks[HDR_LENGTH + 2] = (uint8_t)(request_length >> 8);
    g_blocks[HDR_LENGTH + 3] = (uint8_t)request_length;
    memcpy(g_blocks + FN_MAC_HEADER, request, request_length);
    nblocks = ((long)request_length + FN_MAC_HEADER + FN_MAC_BLOCK - 1) / FN_MAC_BLOCK;
    if (block_io(1, g_mac.base, g_blocks, nblocks) != noErr)
        return FN_ERR_IO;

    /* Poll the header block until the response for this sequence number is
     * up. A response to an earlier request, one that timed out, is not it. */
    start = TickCount();
    for (;;) {
        if (block_io(0, g_mac.base, g_blocks, 1) != noErr)
            return FN_ERR_IO;
        if (memcmp(g_blocks, "FNIO", 4) != 0)
            return FN_ERR_IO;
        if (g_blocks[HDR_STATE] == MB_READY && header_seq() == seq)
            break;
        if (TickCount() - start > FN_MAC_TIMEOUT_TICKS)
            return FN_ERR_TIMEOUT;
    }

    length = ((uint32_t)g_blocks[HDR_LENGTH] << 24) | ((uint32_t)g_blocks[HDR_LENGTH + 1] << 16) |
             ((uint32_t)g_blocks[HDR_LENGTH + 2] << 8) | g_blocks[HDR_LENGTH + 3];
    if (length > response_capacity || length + FN_MAC_HEADER > FN_MAC_MAILBOX_BYTES)
        return FN_ERR_INVALID;
    nblocks = ((long)length + FN_MAC_HEADER + FN_MAC_BLOCK - 1) / FN_MAC_BLOCK;
    if (nblocks > 1 &&
        block_io(0, g_mac.base + 1, g_blocks + FN_MAC_BLOCK, nblocks - 1) != noErr)
        return FN_ERR_IO;
    memcpy(response, g_blocks + FN_MAC_HEADER, (size_t)length);
    *response_length = (uint16_t)length;
    return FN_OK;
}

const char *fn_platform_name(void)
{
    return "mac68k";
}
