/* Handspring-only raw packet backend. No serial alias, SLIP or HotSync. */
#include <UsbMgr.h>
#include "private.h"
#include "fn_internal.h"

#define USB_PREF 0x4e36
static Char usb_name[] = "USB Library";
static UInt16 usb_ref, usb_loaded, usb_opened, usb_fault;
static UInt8 usb_tx[1024];

/* A USB timeout is NOT proof that the peer retired the old physical session.
 * Persist only ambiguity, independently of serial and CS1 ownership markers.
 * There is intentionally no automatic reset/clear recovery operation. */
UInt16 nl_usb_pending(void)
{
    UInt16 marker = 0, size = sizeof(marker);
    Int16 v = PrefGetAppPreferences(nioLibCreator, USB_PREF, &marker, &size, false);
    return v != noPreferenceFound && (v != 1 || size != sizeof(marker) || marker);
}
static Boolean mark(UInt16 value)
{
    UInt16 check = ~value, size = sizeof(check);
    Int16 v;
    PrefSetAppPreferences(nioLibCreator, USB_PREF, 1, &value, sizeof(value), false);
    v = PrefGetAppPreferences(nioLibCreator, USB_PREF, &check, &size, false);
    return v == 1 && size == sizeof(check) && check == value;
}
UInt16 nl_usb_owned(void) { return usb_opened || usb_loaded; }
static Boolean handspring_usb(void)
{
    UInt32 version = 0;
    return FtrGet('hsEx', 0, &version) == errNone && version != 0;
}
UInt16 nl_usb_available(void)
{
    UInt16 ref;
    if (!handspring_usb()) return 0;
    return SysLibFind(usb_name, &ref) == errNone;
}
void nl_usb_close(void)
{
    if (usb_opened) {
        /* Driver owns no app pointer: queued data lives in usb_tx until Close.
         * Keep globals alive if Close fails; never free a live driver's data. */
        UsbLibSendFlush(usb_ref);
        if (UsbLibClose(usb_ref)) { usb_fault = 1; return; }
        usb_opened = 0;
    }
    if (usb_loaded) {
        if (SysLibTblEntry(usb_ref)->globalsP || SysLibRemove(usb_ref)) return;
        usb_loaded = 0;
    }
    MemSet(usb_tx, sizeof(usb_tx), 0);
}
UInt16 nl_usb_open(void)
{
    Err err;
    const UInt16 *table;
    if (!handspring_usb()) return FN_ERR_UNSUPPORTED;
    if (usb_opened || usb_loaded || nl_usb_pending()) return FN_ERR_BUSY;
    usb_fault = 0;
    err = SysLibFind(usb_name, &usb_ref);
    if (err) {
        err = SysLibLoad(sysFileTLibrary, 'HsUs', &usb_ref);
        if (err) return FN_ERR_NOT_FOUND;
        usb_loaded = 1;
    }
    /* Old ROMs may omit the counted Send/Receive traps. Check the dispatch
     * table's name offset before accessing the final required entry (+19).
     * This is a structural API guard, not a claim about arbitrary ROM stubs. */
    table = (const UInt16 *)SysLibTblEntry(usb_ref)->dispatchTblP;
    if (!table || table[0] < 2 * (sysLibTrapCustom - 0xA800 + 20)) {
        nl_usb_close(); return FN_ERR_UNSUPPORTED;
    }
    err = UsbLibOpen(usb_ref, usbLibPort2, 115200UL);
    if (err) { nl_usb_close(); return err == serErrAlreadyOpen ? FN_ERR_BUSY : FN_ERR_IO; }
    usb_opened = 1;
    return FN_OK;
}
UInt16 nl_usb_ready(void) { return usb_opened && !usb_fault && !nl_usb_pending(); }
static UInt16 receive_exact(UInt8 *buffer, UInt16 count, UInt32 start, UInt32 wait)
{
    UInt32 available, got;
    UInt16 used = 0, take;
    Err err;
    while (used < count) {
        if (nl_expired(start, wait)) return FN_ERR_TIMEOUT;
        available = 0;
        err = UsbLibReceiveCheck(usb_ref, &available);
        if (err) return FN_ERR_IO;
        if (!available) { SysTaskDelay(1); continue; }
        take = count - used;
        if (available < take) take = (UInt16)available;
        got = UsbLibReceive(usb_ref, buffer + used, take, 1, &err);
        if (got > take || (err && err != serErrTimeOut)) return FN_ERR_IO;
        used += (UInt16)got;
        if (!got) SysTaskDelay(1);
    }
    return FN_OK;
}
UInt16 nl_usb_exchange(const UInt8 *request, UInt16 length,
        UInt8 *response, UInt16 capacity, UInt16 *got)
{
    UInt16 result, used = 0, total;
    UInt32 start = TimGetTicks(), wait = 35UL * SysTicksPerSecond(), n, pending;
    Err err;
    *got = 0;
    if (!nl_usb_ready()) return FN_ERR_NOT_READY;
    if (length < FN_HEADER_SIZE || length > sizeof(usb_tx) || capacity < FN_HEADER_SIZE ||
        FN_READ_LE16(request, 2) != length ||
        fn_calc_packet_checksum(request, length) != request[4]) return FN_ERR_INVALID;
    _fn_parse_ctx.response = request; _fn_parse_ctx.resp_len = length;
    if (fn_parse_response_header()) return FN_ERR_INVALID;
    /* Unsolicited bytes cannot belong to a new synchronous exchange. */
    pending = 0;
    if (UsbLibReceiveCheck(usb_ref, &pending) || pending) { result = FN_ERR_IO; goto failed; }
    if (!mark(1)) return FN_ERR_IO;
    MemMove(usb_tx, request, length);
    while (used < length) {
        if (nl_expired(start, wait)) { result = FN_ERR_TIMEOUT; goto failed; }
        n = UsbLibSend(usb_ref, usb_tx + used, length - used, &err);
        if (err || !n || n > length - used) { result = FN_ERR_IO; goto failed; }
        used += (UInt16)n;
        do {
            if (nl_expired(start, wait)) { result = FN_ERR_TIMEOUT; goto failed; }
            if (UsbLibSendCheck(usb_ref, &pending)) { result = FN_ERR_IO; goto failed; }
            if (pending) SysTaskDelay(1);
        } while (pending);
    }
    result = receive_exact(response, FN_HEADER_SIZE, start, wait);
    if (result) goto failed;
    total = FN_READ_LE16(response, 2);
    if (total < FN_HEADER_SIZE || total > 1024 || total > capacity) { result = FN_ERR_INVALID; goto failed; }
    result = receive_exact(response + FN_HEADER_SIZE, total - FN_HEADER_SIZE, start, wait);
    if (result) goto failed;
    if (response[0] != request[0] || response[1] != request[1] ||
        fn_calc_packet_checksum(response, total) != response[4] ||
        !nl_payload_valid(request, response, total, capacity)) { result = FN_ERR_IO; goto failed; }
    /* There is no pipelining: excess bytes are unsolicited, never silently
     * discarded or accepted as the reply to a later request. */
    if (UsbLibReceiveCheck(usb_ref, &pending) || pending) { result = FN_ERR_IO; goto failed; }
    if (!mark(0)) { result = FN_ERR_IO; goto failed; }
    *got = total;
    return FN_OK;
failed:
    mark(1); usb_fault = 1;
    nl_usb_close();
    return result;
}
