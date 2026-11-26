/* Palm-only lifecycle/deadline adapter around the upstream CS1 packet engine.
 * No SLIP, PacketLink, service protocol or CPU/PIO firmware is implemented here. */
#include "NioLib.h"
#include "private.h"
#include "fn_internal.h"
#include "fn_platform.h"
#include "../fn_palmos_internal.h"

Err nl_hs_get(UInt16 card, UInt16 attr, void *value)
    __attribute__((__callseq__("move.w #6,-(%%sp); trap #15; dc.w 0xA349")));
uint16_t nl_original_exchange(const uint8_t *, uint16_t, uint8_t *, uint16_t, uint16_t *);

/* This is an ownership tombstone, NOT a saved network session/configuration.
 * It is a feature (one UInt32: the CS1 base, whose low byte is always zero,
 * with the posted sequence in that byte), so writing it is a table update,
 * not a database write: a preference here cost about 116 ms per exchange on
 * a Visor Prism. Features survive library unload and are cleared by a soft
 * reset, which the recovery API never assumed it survives. Publishing and
 * read-back must succeed BEFORE the doorbell is physically posted. */
#define PENDING_FTR 0x4e32
typedef struct { UInt32 base; UInt16 sequence; } Pending;
static UInt16 candidate_card, invalid, posted, ambiguous, last_result;
static UInt32 started, budget;

/* Reads: reply words between clock reads, and how often (ticks) a running
 * exchange re-checks that the module is still mapped. A register read (status
 * polls among them) reads the clock every time, so the deadline still ends bus
 * access within one poll. Checking the mapping in full on every word (seven
 * system calls) cost about 1.3 ms per word on a Visor Prism, and a read of a
 * moved module can only bring back a word the exchange then rejects.
 *
 * Writes: every one checks the mapping in full first, as before. A write
 * through a mapping that has changed could land on another card, and nothing
 * afterwards undoes it; request words are few next to reply words.
 *
 * Every check of the mapping, by either, is followed by the deadline, read
 * after its system calls: a check that runs past it stops the access. */
#define CLOCK_EVERY 32
static UInt16 mapped, accesses;
static UInt32 mapped_at, recheck;

static Boolean supported(void)
{
    static UInt16 known;
    UInt32 version;
    /* HsExtCommon.h: hsFtrCreator='hsEx', hsFtrIDVersion=0. Only a positive
     * answer is kept: the extensions cannot leave while NioLib is loaded. */
    if (known) return true;
    known = FtrGet('hsEx', 0, &version) == errNone && version != 0;
    return known;
}

Err nl_card_attr(UInt16 card, UInt16 attr, void *value)
{
    Err err;
    if (!supported()) return nioLibErrParam;
    err = nl_hs_get(card, attr, value);
    if (!err && attr == 4) candidate_card = card;
    return err;
}

static Boolean mapping_valid(void)
{
    UInt8 installed = 0;
    UInt32 base = 0, size = 0;
    UInt16 card = _fn_palmos_card ? _fn_palmos_card : candidate_card;
    mapped = 0;
    if (!_fn_palmos_cs1 || !card ||
        nl_card_attr(card, 1, &installed) || !installed ||
        nl_card_attr(card, 3, &base) || nl_card_attr(card, 4, &size) ||
        !size || base + size < base || base + size != _fn_palmos_cs1 ||
        (_fn_palmos_cs0 && base != _fn_palmos_cs0)) {
        invalid = 1;
        return false;
    }
    mapped = 1;
    mapped_at = TimGetTicks();
    return true;
}

/* The deadline at now, a time read after any system calls before it. */
static Boolean in_time(UInt32 now)
{
    if (now - started >= budget) { invalid = 1; return false; }
    return true;
}

/* Before a read: the deadline, and the mapping, checked in full when no check
 * has passed in this exchange, else once recheck ticks have passed. A reply
 * word reads the clock only every CLOCK_EVERY words; a register read reads it
 * every time. */
static Boolean read_ok(Boolean reply)
{
    UInt32 now;
    if (invalid) return false;
    if (reply && mapped && ++accesses < CLOCK_EVERY) return true;
    accesses = 0;
    now = TimGetTicks();
    if (!in_time(now)) return false;
    if (mapped && now - mapped_at < recheck) return true;
    return mapping_valid() && in_time(mapped_at);
}

/* Before a write: the mapping in full, then the deadline. */
static Boolean write_ok(void)
{
    return !invalid && mapping_valid() && in_time(mapped_at);
}

void nl_deadline(UInt16 milliseconds)
{
    UInt16 rate = SysTicksPerSecond();
    started = TimGetTicks();
    budget = ((UInt32)milliseconds * rate + 999) / 1000;
    if (!budget) budget = 1;
    recheck = rate / 4 ? rate / 4 : 1;
    invalid = 0; mapped = 0; accesses = 0;
    posted = ambiguous = 0; last_result = 0xffff;
}

int nl_expired(UInt32 start, UInt32 wait)
{
    UInt32 now = TimGetTicks();
    return invalid || now - started >= budget || now - start > wait;
}

/* False when the marker cannot be known. Only a missing feature means none:
 * any other FtrGet error fails closed. */
static Boolean pending_get(Pending *p)
{
    UInt32 value;
    Err err;
    p->base = 0; p->sequence = 0;
    err = FtrGet(nioLibCreator, PENDING_FTR, &value);
    if (err == ftrErrNoSuchFeature) return true;
    if (err != errNone) return false;
    p->base = value & ~0xffUL;
    p->sequence = (UInt16)(value & 0xff);
    return !p->sequence || p->base;
}

static Boolean pending_set(UInt32 base, UInt16 seq)
{
    Pending check;
    if ((base & 0xff) || seq > 255) return false;
    if (FtrSet(nioLibCreator, PENDING_FTR, base | seq) != errNone) return false;
    return pending_get(&check) && check.base == base && check.sequence == seq;
}

UInt16 nl_cs1_pending(void)
{
    Pending p;
    return !pending_get(&p) || p.sequence != 0;
}
Err nl_cs1_reset(void)
{
    if (!pending_set(0, 0)) return nioLibErrGlobals;
    posted = ambiguous = 0;
    return errNone;
}

UInt16 nl_bus_read(UInt32 address)
{
    if (!read_ok(address - _fn_palmos_cs1 >= SBN_R_RESP)) return 0;
    return *(volatile UInt16 *)address;
}

void nl_bus_write(UInt32 address, UInt16 value)
{
    if (!write_ok()) return;
    if (address == _fn_palmos_cs1 + SBN_W(SBN_W_DOORBELL)) {
        /* The marker, then the mapping and the deadline again. */
        if (!pending_set(_fn_palmos_cs1, value)) { invalid = 1; return; }
        if (!write_ok()) {
            /* We know this doorbell was never posted. */
            pending_set(0, 0);
            return;
        }
        posted = 1;
    }
    *(volatile UInt16 *)address = value;
}

/* fn_palmos_read_stable, and the bus must not have refused the read (the value
 * is then not a register word). False means the register was not read. */
static Boolean stable_rd(UInt16 offset, UInt16 *value)
{
    return fn_palmos_read_stable(offset, value) && !invalid;
}

static Boolean settled(UInt16 seq)
{
    UInt16 first, ack, response, last;
    if (!stable_rd(SBN_R_STATUS, &first) || !stable_rd(SBN_R_ACK_SEQ, &ack) ||
        !stable_rd(SBN_R_RESP_SEQ, &response) || !stable_rd(SBN_R_STATUS, &last))
        return false;
    return !((first | last) & SBN_ST_BUSY) && ack == response && (!seq || response == seq);
}

static Boolean synchronize(void)
{
    Pending p;
    UInt16 seq;
    if (!mapping_valid() || !pending_get(&p)) return false;
    if (p.sequence) {
        if (p.base != _fn_palmos_cs1 || !settled(p.sequence) || !pending_set(0, 0))
            return false;
    }
    if (!settled(0) || !stable_rd(SBN_R_RESP_SEQ, &seq)) return false;
    _fn_palmos_seq = (UInt8)seq;
    return true;
}

UInt16 nl_cs1_probe(UInt16 *hostP, UInt16 *busyP)
{
    UInt16 status;
    *hostP = *busyP = 0;
    if (!supported()) return FN_ERR_NOT_FOUND;
    if (_fn_palmos_cs1 && !mapping_valid()) return FN_ERR_NOT_FOUND;
    if (!fn_palmos_open() || invalid) return FN_ERR_NOT_FOUND;
    if (!stable_rd(SBN_R_STATUS, &status)) return invalid ? FN_ERR_NOT_FOUND : FN_ERR_IO;
    *hostP = !!(status & SBN_ST_HOST);
    *busyP = !!(status & SBN_ST_BUSY);
    return FN_OK;
}

UInt16 nl_cs1_begin(void)
{
    UInt16 host, busy, status, result;
    result = nl_cs1_probe(&host, &busy);
    if (result) return result;
    if (!stable_rd(SBN_R_STATUS, &status)) return invalid ? FN_ERR_NOT_FOUND : FN_ERR_IO;
    if (status & SBN_ST_LOOPBACK) return FN_ERR_NOT_READY;
    if (!synchronize()) return FN_ERR_BUSY;
    return FN_OK; /* Module presence does not assert host/Internet availability. */
}

UInt16 nl_cs1_last_result(void) { return last_result; }
UInt16 nl_cs1_ambiguous(void) { return ambiguous; }

/* Ownership/bounds policy at the shared transport boundary. Reuse the common
 * decoders; reject inconsistent echoes BEFORE the common Read copies data to
 * an application buffer. This is not another NetworkService implementation. */
Boolean nl_payload_valid(const uint8_t *request, const uint8_t *response,
                              UInt16 length, UInt16 capacity)
{
    UInt16 offset, command = request[1];
    _fn_parse_ctx.response = response; _fn_parse_ctx.resp_len = length;
    _fn_parse_ctx.data = NULL; _fn_parse_ctx.data_max = capacity;
    if (fn_parse_response_header() != FN_OK) return false;
    if (_fn_parse_ctx.status != FN_OK) return true;
    offset = _fn_parse_ctx.data_offset;
    if (request[0] == FN_DEVICE_CLOCK && command == FN_CMD_CLOCK_GET_TZ) {
        return _fn_parse_ctx.data_len >= 2 && response[offset] == FN_CLOCK_VERSION &&
            response[offset + 1] < FN_MAX_TIMEZONE_LEN &&
            response[offset + 1] <= _fn_parse_ctx.data_len - 2;
    }
    if (request[0] != FN_DEVICE_NETWORK) return true;
    if (command != FN_CMD_CLOSE &&
        (!_fn_parse_ctx.data_len || response[offset] != FN_PROTOCOL_VERSION)) return false;
    if (command == FN_CMD_OPEN) {
        return fn_parse_open_response() == FN_OK && _fn_parse_ctx.handle != FN_INVALID_HANDLE &&
            fn_find_session(_fn_parse_ctx.handle) < 0;
    }
    if (command == FN_CMD_READ) {
        return fn_parse_read_response() == FN_OK &&
            _fn_parse_ctx.handle == FN_READ_LE16(request, 7) &&
            _fn_parse_ctx.offset_echo == FN_READ_LE32(request, 9);
    }
    if (command == FN_CMD_INFO) {
        return _fn_parse_ctx.data_len >= 16 && fn_parse_info_response() == FN_OK &&
            _fn_parse_ctx.handle == FN_READ_LE16(request, 7);
    }
    if (command == 0x07) { /* existing NetworkService TranslateConfigure */
        return _fn_parse_ctx.data_len >= 10 &&
            (response[offset + 1] & 1) && FN_READ_LE16(response, offset + 2) == 0 &&
            FN_READ_LE16(response, offset + 4) == FN_READ_LE16(request, 7);
    }
    if (command == FN_CMD_WRITE) {
        /* Common Write decodes its own accepted-byte metadata inline. */
        return _fn_parse_ctx.data_len >= 12 &&
            FN_READ_LE16(response, offset + 4) == FN_READ_LE16(request, 7) &&
            FN_READ_LE32(response, offset + 6) == FN_READ_LE32(request, 9);
    }
    return true;
}

uint16_t fn_palmos_exchange(const uint8_t *request, uint16_t length,
                            uint8_t *response, uint16_t capacity, uint16_t *got)
{
    UInt16 result, held;
    posted = ambiguous = 0;
    *got = 0;
    if (!synchronize() || nl_expired(started, budget)) {
        last_result = FN_PALMOS_NO_ANSWER;
        return last_result;
    }
    result = nl_original_exchange(request, length, response, capacity, got);
    last_result = result;
    /* The upstream reader checks every response word, even beyond capacity.
     * Reject truncation/mismatched packets before a common service uses them. */
    if (result == SBN_RES_OK && (*got < FN_HEADER_SIZE ||
        !stable_rd(SBN_R_RESP_LEN, &held) || held != *got ||
        response[0] != request[0] || response[1] != request[1] ||
        FN_READ_LE16(response, 2) != *got ||
        fn_calc_packet_checksum(response, *got) != response[FN_CHECKSUM_OFFSET] ||
        !nl_payload_valid(request, response, *got, capacity))) {
        result = last_result = FN_PALMOS_BAD_READ;
        *got = 0;
    }
    /* Older ABI-4 firmware can report NO_HOST after a disconnect even if a
     * request was sent; conservatively treat that outcome as ambiguous too. */
    ambiguous = posted && result != SBN_RES_OK &&
        result != SBN_RES_BAD_LEN && result != SBN_RES_BAD_CHECK && result != SBN_RES_REFUSED;
    /* Completion may race the local deadline. Discard a finalized old reply,
     * never accept it as a new request's answer. Unresolved markers persist. */
    if (!invalid && settled(_fn_palmos_seq)) pending_set(0, 0);
    return result;
}
