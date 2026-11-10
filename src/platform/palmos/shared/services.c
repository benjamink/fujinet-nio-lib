#define NIOLIB_IMPLEMENTATION
#include "NioLib.h"
#include <EntryPoints.h>
#include "private.h"
#include "fn_internal.h"
#include "fn_platform.h"
#include "sbnio.h"

static UInt32 owner_token, owner_db;
static UInt16 owner_card, busy, uncertain_operation;

/* The outer guard is deliberately independent of A4 and works before Open. */
#define SERVICE(name, params, args) \
    static Err name##_a4 params SYSLIB_POINTER_ENTRYPOINT; \
    Err name params { \
        SysLibTblEntryPtr entry = SysLibTblEntry(refNum); \
        if (!entry || !entry->globalsP) return nioLibErrNotOpen; \
        return name##_a4 args; \
    } \
    static Err name##_a4 params

Boolean nl_client_idle(void) { return !busy && !owner_token; }
Err nl_can_close(void)
{
    if (!nl_client_idle()) return nioLibErrBusy;
    /* One bounded attempt per entry. Physical ownership and command ambiguity
     * are independent: neither backend clears its persistent pending marker. */
    if (nl_usb_owned()) nl_usb_close();
    if (nl_serial_owned()) nl_serial_close();
    return nl_usb_owned() || nl_serial_owned() ? nioLibErrBusy : errNone;
}

static Err enter(UInt32 token)
{
    UInt16 card;
    LocalID db;
    if (busy) return nioLibErrBusy;
    busy = 1;
    if (!token || token != owner_token || SysCurAppDatabase(&card, &db) ||
        card != owner_card || db != owner_db) {
        busy = 0;
        return nioLibErrOwner;
    }
    nl_deadline(35000);
    return errNone;
}

static void leave(void)
{
    if (nl_ambiguous()) uncertain_operation = 1;
    /* Common parsers use scratch pointers. Never retain an app buffer after
     * returning through a trap, even though the next common call replaces it. */
    _fn_transport_ctx.request = NULL; _fn_transport_ctx.response = NULL;
    _fn_parse_ctx.response = NULL; _fn_parse_ctx.data = NULL;
    busy = 0;
}

static UInt16 close_handle(UInt16 handle)
{
    UInt16 result = fn_close(handle);
    if (!result) {
        _fn_parse_ctx.response = _fn_resp_buf;
        _fn_parse_ctx.resp_len = _fn_transport_ctx.resp_len;
        result = fn_parse_response_header();
        if (!result) result = _fn_parse_ctx.status;
    }
    return result;
}

SERVICE(NioLibBeginClient,
        (UInt16 refNum, UInt32 *tokenP, UInt16 *capacityP, UInt16 *resultP),
        (refNum, tokenP, capacityP, resultP))
{
    UInt32 generation = 0;
    UInt16 card;
    LocalID db;
    UInt16 result;
    (void)refNum;
    if (!tokenP || !capacityP || !resultP) return nioLibErrParam;
    *tokenP = 0; *capacityP = FN_MAX_SESSIONS; *resultP = FN_ERR_INVALID;
    if (busy || owner_token) return nioLibErrBusy;
    if (nl_can_close()) return nioLibErrBusy;
    busy = 1;
    if (SysCurAppDatabase(&card, &db)) { busy = 0; return nioLibErrOwner; }
    FtrGet(nioLibCreator, 0x4e33, &generation);
    if (generation == 0xffffffffUL || FtrSet(nioLibCreator, 0x4e33, generation + 1)) {
        busy = 0; return nioLibErrOverflow;
    }
    nl_deadline(35000);
    result = fn_init();
    if (!result) result = nl_transport_begin();
    if (result) fn_shutdown();
    else {
        owner_card = card; owner_db = db; owner_token = generation + 1;
        uncertain_operation = 0;
        *tokenP = owner_token;
    }
    *resultP = result;
    leave();
    return errNone;
}

SERVICE(NioLibEndClient,
        (UInt16 refNum, UInt32 token, NioLibCleanup *c, UInt16 *resultP),
        (refNum, token, c, resultP))
{
    Err err;
    UInt16 i, result;
    (void)refNum;
    if (!c || c->size != sizeof(*c) || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    c->attempted = c->confirmed = c->uncertain = c->reserved = 0;
    *resultP = FN_OK;
    /* This total deadline is consulted INSIDE upstream response polling and
     * every guarded MMIO access. It is not a timer between blocking calls. */
    nl_deadline(2000);
    for (i = 0; i < FN_MAX_SESSIONS; ++i) {
        if (_fn_sessions[i].active) {
            ++c->attempted;
            result = close_handle(_fn_sessions[i].handle);
            if (!result) ++c->confirmed;
            else { ++c->uncertain; *resultP = result; }
        }
    }
    if (c->uncertain || nl_transport_pending() || uncertain_operation || nl_ambiguous()) {
        c->ambiguousOperation = 1;
        if (!*resultP) *resultP = FN_ERR_TIMEOUT;
    } else c->ambiguousOperation = 0;
    fn_shutdown();
    if (!*resultP && (nl_usb_owned() || nl_serial_owned())) *resultP = FN_ERR_BUSY;
    owner_token = owner_db = 0; owner_card = 0;
    leave();
    return errNone;
}

SERVICE(NioLibProbe,
        (UInt16 refNum, NioLibAvailability *a, UInt16 *resultP),
        (refNum, a, resultP))
{
    UInt16 card;
    LocalID db;
    (void)refNum;
    if (!a || a->size != sizeof(*a) || !resultP) return nioLibErrParam;
    if (busy) return nioLibErrBusy;
    if (owner_token && (SysCurAppDatabase(&card, &db) || card != owner_card || db != owner_db))
        return nioLibErrOwner;
    busy = 1;
    nl_deadline(2000);
    *resultP = nl_transport_probe(&a->hostPresent, &a->linkBusy);
    a->modulePresent = (nl_active() ? nl_active() : nl_selected()) == nioTransportSpringboard && *resultP == FN_OK;
    a->maxSessions = FN_MAX_SESSIONS; a->reserved = 0;
    leave();
    return errNone;
}

SERVICE(NioLibNetOpen,
        (UInt16 refNum, UInt32 token, const NioLibOpenArgs *a, UInt16 *handleP, UInt16 *resultP),
        (refNum, token, a, handleP, resultP))
{
    Err err;
    (void)refNum;
    if (!a || a->size != sizeof(*a) || !a->url || !handleP || !resultP ||
        a->method > 255 || a->flags > 255 || a->translationType > 255 ||
        a->translationFlags > 255 || a->translated > 1) return nioLibErrParam;
    *handleP = FN_INVALID_HANDLE;
    err = enter(token); if (err) return err;
    nl_clear_last_result();
    if (a->translated)
        *resultP = fn_open_translated(handleP, (uint8_t)a->method, a->url, (uint8_t)a->flags,
                    (uint8_t)a->translationType, (uint8_t)a->translationFlags, a->selector);
    else *resultP = fn_open(handleP, (uint8_t)a->method, a->url, (uint8_t)a->flags);
    /* A syntactically invalid Open response may hide a remote allocation. */
    if (*resultP && (nl_ambiguous() || nl_transport_pending() ||
        (nl_last_result() == SBN_RES_OK && _fn_parse_ctx.status == FN_OK)))
        uncertain_operation = 1;
    leave(); return errNone;
}

SERVICE(NioLibNetInfo,
        (UInt16 refNum, UInt32 token, UInt16 handle, UInt16 *httpP, UInt32 *lengthP,
         UInt16 *flagsP, UInt16 *resultP),
        (refNum, token, handle, httpP, lengthP, flagsP, resultP))
{
    Err err;
    uint8_t flags = 0;
    (void)refNum;
    if (!httpP || !lengthP || !flagsP || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = fn_info(handle, httpP, lengthP, &flags);
    *flagsP = flags;
    leave(); return errNone;
}

SERVICE(NioLibNetRead,
        (UInt16 refNum, UInt32 token, UInt16 handle, UInt32 offset, UInt8 *buffer,
         UInt16 capacity, UInt16 *readP, UInt16 *flagsP, UInt16 *resultP),
        (refNum, token, handle, offset, buffer, capacity, readP, flagsP, resultP))
{
    Err err;
    uint8_t flags = 0;
    (void)refNum;
    if (!buffer || !readP || !flagsP || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = fn_read(handle, offset, buffer, capacity, readP, &flags);
    *flagsP = flags;
    leave(); return errNone;
}

SERVICE(NioLibNetWrite,
        (UInt16 refNum, UInt32 token, UInt16 handle, UInt32 offset, const UInt8 *buffer,
         UInt16 length, UInt16 *writtenP, UInt16 *resultP),
        (refNum, token, handle, offset, buffer, length, writtenP, resultP))
{
    Err err;
    (void)refNum;
    if ((!buffer && length) || !writtenP || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = fn_write(handle, offset, buffer, length, writtenP);
    leave(); return errNone;
}

SERVICE(NioLibNetClose,
        (UInt16 refNum, UInt32 token, UInt16 handle, UInt16 *resultP),
        (refNum, token, handle, resultP))
{
    Err err;
    (void)refNum;
    if (!resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = fn_find_session(handle) < 0 ? FN_ERR_NOT_FOUND : close_handle(handle);
    if (*resultP && *resultP != FN_ERR_NOT_FOUND) uncertain_operation = 1;
    leave(); return errNone;
}

SERVICE(NioLibNetReady,
        (UInt16 refNum, UInt32 token, NioLibReadyEntry *entries, UInt16 capacity,
         UInt16 *countP, UInt16 *resultP),
        (refNum, token, entries, capacity, countP, resultP))
{
    Err err;
    fn_ready_entry_t ready[FN_MAX_SESSIONS];
    uint8_t count = 0, i;
    (void)refNum;
    if ((!entries && capacity) || !countP || !resultP) return nioLibErrParam;
    *countP = 0;
    err = enter(token); if (err) return err;
    *resultP = fn_ready(ready, FN_MAX_SESSIONS, &count);
    if (!*resultP) for (i = 0; i < count && *countP < capacity; ++i) {
        if (fn_find_session(ready[i].handle) >= 0) {
            entries[*countP].handle = ready[i].handle;
            entries[*countP].flags = ready[i].flags;
            entries[*countP].available = ready[i].available;
            ++*countP;
        }
    }
    leave(); return errNone;
}

SERVICE(NioLibClockGet,
        (UInt16 refNum, UInt32 token, UInt8 *timeLE8, UInt16 *resultP),
        (refNum, token, timeLE8, resultP))
{
    Err err;
    FN_TIME_T time;
    UInt16 i;
    (void)refNum;
    if (!timeLE8 || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = fn_clock_get(&time);
    if (!*resultP) for (i = 0; i < 8; ++i) { timeLE8[i] = (UInt8)time; time >>= 8; }
    leave(); return errNone;
}

SERVICE(NioLibClockTimezone,
        (UInt16 refNum, UInt32 token, Char *buffer, UInt16 capacity, UInt16 *resultP),
        (refNum, token, buffer, capacity, resultP))
{
    Err err;
    Char zone[FN_MAX_TIMEZONE_LEN];
    UInt16 length;
    (void)refNum;
    if (!buffer || !capacity || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = fn_clock_get_timezone(zone);
    if (!*resultP) {
        length = StrLen(zone);
        if (_fn_parse_ctx.data_len < 2 ||
            fn_clock_response_byte(_fn_parse_ctx.data_offset + 1) > _fn_parse_ctx.data_len - 2 ||
            length >= capacity) *resultP = FN_ERR_INVALID;
        else StrCopy(buffer, zone);
    }
    leave(); return errNone;
}

SERVICE(NioLibGetConfig,
        (UInt16 refNum, NioLibConfig *c, UInt16 *flagsP), (refNum, c, flagsP))
{
    (void)refNum;
    if (!c || c->size != sizeof(*c) || !flagsP) return nioLibErrParam;
    if (busy) return nioLibErrBusy;
    *flagsP = nl_config_read(c);
    return errNone;
}
SERVICE(NioLibSetConfig,
        (UInt16 refNum, const NioLibConfig *c), (refNum, c))
{
    (void)refNum;
    if (busy || owner_token || nl_active()) return nioLibErrBusy;
    return nl_config_write(c);
}
SERVICE(NioLibGetTransportStatus,
        (UInt16 refNum, NioLibTransportStatus *s), (refNum, s))
{
    (void)refNum;
    if (!s || s->size != sizeof(*s)) return nioLibErrParam;
    if (busy) return nioLibErrBusy;
    nl_transport_status(s);
    return errNone;
}

SERVICE(NioLibNetJsonQuery,
        (UInt16 refNum, UInt32 token, UInt16 handle, const Char *selector, UInt16 *resultP),
        (refNum, token, handle, selector, resultP))
{
    Err err;
    (void)refNum;
    if (!selector || !resultP) return nioLibErrParam;
    err = enter(token); if (err) return err;
    *resultP = nl_json_query(handle, selector);
    leave(); return errNone;
}

SERVICE(NioLibResetTransport,
        (UInt16 refNum, UInt16 transport, UInt16 confirmation),
        (refNum, transport, confirmation))
{
    Err result;
    (void)refNum;
    if (confirmation != nioRecoveryPeerReset ||
        (transport != nioTransportSpringboard && transport != nioTransportSerial &&
         transport != nioTransportUSB)) return nioLibErrParam;
    if (!nl_client_idle() || nl_active()) return nioLibErrBusy;
    if (nl_can_close()) return nioLibErrBusy;
    busy = 1;
    result = nl_transport_reset(transport);
    if (!result) uncertain_operation = 0;
    busy = 0;
    return result;
}
