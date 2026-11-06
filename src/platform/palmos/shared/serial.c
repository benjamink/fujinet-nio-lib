/* Palm shared-only adaptation of fn_channel_serial.c. The upstream static
 * channel is untouched. Common fn_stream_session_t and SLIP are reused. */
#include <PalmOS.h>
#include <SerialMgrOld.h>
#include "private.h"
#include "fn_session.h"
#include "fn_slip.h"
#include "fn_internal.h"

#define SERIAL_PREF 0x4e35
#define SERIAL_MAGIC 0x4e535031UL
#define WIRE_SIZE 2050
static Char serial_name[] = "Serial Library";
static UInt16 port, opened, old_manager, io_fault, rx_installed, tx_owned, shared_ref;
static UInt8 tx_byte;
static UInt32 tx_start;
static UInt8 rx_buffer[WIRE_SIZE], wire[WIRE_SIZE];
static fn_stream_session_t session;

static UInt32 ticks(UInt16 ms)
{
    UInt32 n = ((UInt32)ms * SysTicksPerSecond() + 999) / 1000;
    return n ? n : 1;
}
/* Unlike CS1, raw serial has no sequence/epoch in the protocol. A possibly
 * executed packet cannot safely be matched after timeout, close or unload.
 * This unsaved marker blocks reuse until an external link/peer reset and an
 * explicit developer recovery. No automatic state-changing replay. */
UInt16 nl_serial_pending(void)
{
    UInt32 marker = 0;
    UInt16 size = sizeof(marker);
    Int16 version = PrefGetAppPreferences(nioLibCreator, SERIAL_PREF, &marker, &size, false);
    return version != noPreferenceFound &&
        (version != 1 || size != sizeof(marker) || marker != 0);
}
static Boolean marker_set(UInt32 value)
{
    UInt32 check = ~value;
    UInt16 size = sizeof(check);
    Int16 version;
    PrefSetAppPreferences(nioLibCreator, SERIAL_PREF, 1, &value, sizeof(value), false);
    version = PrefGetAppPreferences(nioLibCreator, SERIAL_PREF, &check, &size, false);
    return version == 1 && size == sizeof(check) && check == value;
}
static void clear_error(void)
{
    if (old_manager) SerClearErr(port); else SrmClearErr(port);
}
static uint8_t channel_open(void *context) { (void)context; return opened ? FN_OK : FN_ERR_NOT_READY; }
static void channel_close(void *context) { (void)context; }
static void channel_flush(void *context)
{
    (void)context;
    if (old_manager) SerReceiveFlush(port, 0); else SrmReceiveFlush(port, 0);
}
static void abort_tx(void)
{
    Err err = old_manager ? SerSendFlush(port) : SrmSendFlush(port);
    if (!err) tx_owned = 0;
}
static uint8_t write_byte(void *context, uint8_t value, uint16_t timeout)
{
    Err err;
    UInt32 n, remaining = 1, start = TimGetTicks();
    (void)context;
    if (!opened || tx_owned || nl_expired(tx_start, ticks(5000))) return FN_ERR_TIMEOUT;
    /* Driver may retain the send pointer even on an error. Never expose the
     * callback's stack byte, or reuse this byte before Check/Flush/Close. */
    tx_byte = value; tx_owned = 1;
    n = old_manager ? SerSend(port, &tx_byte, 1, &err) : SrmSend(port, &tx_byte, 1, &err);
    if (n != 1 || err) { abort_tx(); clear_error(); return FN_ERR_IO; }
    /* No unbounded SendWait. CTS timeout configured to one tick. */
    while (remaining) {
        err = old_manager ? SerSendCheck(port, &remaining) : SrmSendCheck(port, &remaining);
        if (err) { abort_tx(); clear_error(); return FN_ERR_IO; }
        if (!remaining) break;
        if (nl_expired(start, ticks(timeout)) || nl_expired(tx_start, ticks(5000)))
            { abort_tx(); return FN_ERR_TIMEOUT; }
        SysTaskDelay(1);
    }
    tx_owned = 0;
    return FN_OK;
}
static uint8_t read_byte(void *context, uint8_t *value, uint16_t timeout)
{
    Err err;
    UInt32 n;
    (void)context;
    /* Guard inside every poll, including continuous garbage input. The
     * upstream session's elapsed counter alone only counts empty polls. */
    if (!opened || io_fault || nl_expired(tx_start, ticks(35000))) return 0;
    n = old_manager ? SerReceive(port, value, 1, ticks(timeout), &err) :
                      SrmReceive(port, value, 1, ticks(timeout), &err);
    if (err && err != serErrTimeOut) { io_fault = 1; clear_error(); return 0; }
    return n == 1;
}
static const fn_stream_channel_ops_t ops = {
    channel_open, channel_close, write_byte, read_byte, channel_flush, NULL
};

UInt16 nl_serial_open(const NioLibConfig *c)
{
    UInt32 feature = 0, flags = srmSettingsFlagBitsPerChar8;
    UInt16 length;
    Int32 cts = 1;
    Err err;
    SerSettingsType settings;
    if (nl_serial_owned()) return FN_ERR_BUSY;
    if (nl_serial_pending()) return FN_ERR_BUSY;
    old_manager = FtrGet(sysFileCSerialMgr, sysFtrNewSerialPresent, &feature) || !feature;
    if (old_manager) {
        /* No USB Library alias or forced UART takeover. AN-09 keyboard
         * release is intentionally deferred pending serial hardware checks. */
        err = SysLibFind(serial_name, &port);
        if (err) return FN_ERR_NOT_FOUND;
        err = SerOpen(port, 0, c->serialBaud);
        if (err == serErrAlreadyOpen) {
            /* AlreadyOpen increments the legacy library reference, but does
             * not grant us buffer/port ownership. Retain only that reference
             * if balancing it fails; never Flush someone else's connection. */
            err = SerClose(port);
            shared_ref = err != errNone && err != serErrStillOpen;
            return FN_ERR_BUSY;
        }
    } else err = SrmOpen(serPortCradlePort, c->serialBaud, &port);
    if (err) return err == serErrAlreadyOpen ? FN_ERR_BUSY : FN_ERR_NOT_FOUND;
    opened = 1;
    if (c->serialOptions & nioSerialRTSCTS) flags |= srmSettingsFlagRTSAutoM | srmSettingsFlagCTSAutoM;
    if (old_manager) {
        settings.baudRate = c->serialBaud; settings.flags = flags; settings.ctsTimeout = cts;
        err = SerSetSettings(port, &settings);
    } else {
        length = sizeof(flags); err = SrmControl(port, srmCtlSetFlags, &flags, &length);
        if (!err) { length = sizeof(cts); err = SrmControl(port, srmCtlSetCtsTimeout, &cts, &length); }
    }
    if (!err) err = old_manager ? SerSetReceiveBuffer(port, rx_buffer, sizeof(rx_buffer)) :
                                SrmSetReceiveBuffer(port, rx_buffer, sizeof(rx_buffer));
    if (!err) rx_installed = 1;
    if (!err) err = fn_stream_session_init(&session, &ops, NULL, wire, sizeof(wire));
    if (!err) err = fn_stream_session_open(&session);
    if (err) { nl_serial_close(); return FN_ERR_IO; }
    channel_flush(NULL);
    return FN_OK;
}
UInt16 nl_serial_owned(void) { return opened || rx_installed || tx_owned || shared_ref; }
void nl_serial_close(void)
{
    Err err;
    if (shared_ref) {
        err = SerClose(port);
        /* StillOpen releases OUR reference; somebody else's reference remains.
         * Retrying Close here would consume that other owner's count. */
        if (!err || err == serErrStillOpen) { shared_ref = 0; port = 0; }
        return;
    }
    if (!nl_serial_owned()) return;
    io_fault = 1;
    fn_stream_session_close(&session);
    if (opened || tx_owned) abort_tx();
    if (rx_installed) {
        err = old_manager ? SerSetReceiveBuffer(port, NULL, 0) : SrmSetReceiveBuffer(port, NULL, 0);
        if (!err) rx_installed = 0;
    }
    if (opened) {
        err = old_manager ? SerClose(port) : SrmClose(port);
        if (!err) {
            /* Successful physical shutdown releases all our driver pointers. */
            opened = rx_installed = tx_owned = 0;
        } else if (old_manager && err == serErrStillOpen) {
            /* Our Open was consumed, but the port remains live. Preserve any
             * undetached buffer/byte and never Close another owner's ref. */
            opened = 0;
        } else return;
    }
    if (nl_serial_owned()) return;
    port = io_fault = 0;
    MemSet(&session, sizeof(session), 0);
    MemSet(wire, sizeof(wire), 0); MemSet(rx_buffer, sizeof(rx_buffer), 0);
    tx_byte = 0;
}
UInt16 nl_serial_ready(void) { return opened && !io_fault && !nl_serial_pending(); }
static Boolean strict_frame(void)
{
    UInt16 i, n = session.last_raw_length;
    if (n < 2 || wire[n - 1] != SLIP_END) return false;
    i = wire[0] == SLIP_END ? 1 : 0;
    for (; i < n - 1; ++i) {
        if (wire[i] == SLIP_END) return false;
        if (wire[i] == SLIP_ESCAPE) {
            ++i;
            if (i >= n - 1 || (wire[i] != SLIP_ESC_END && wire[i] != SLIP_ESC_ESC)) return false;
        }
    }
    return true;
}
UInt16 nl_serial_exchange(const UInt8 *request, UInt16 length,
        UInt8 *response, UInt16 capacity, UInt16 *got)
{
    UInt16 result;
    *got = 0;
    if (!opened) return FN_ERR_NOT_READY;
    if (!request || length < FN_HEADER_SIZE || length > 1024 || !response || !capacity || capacity > 1024)
        return FN_ERR_INVALID;
    if (nl_serial_pending()) return FN_ERR_BUSY;
    if (!marker_set(SERIAL_MAGIC)) return FN_ERR_IO;
    tx_start = TimGetTicks(); io_fault = 0;
    result = fn_stream_session_request(&session, request, length, response, capacity, got, 35000);
    if (!result && (io_fault || !strict_frame() || *got < FN_HEADER_SIZE ||
        response[0] != request[0] || response[1] != request[1] ||
        fn_calc_packet_checksum(response, *got) != response[FN_CHECKSUM_OFFSET] ||
        !nl_payload_valid(request, response, *got, capacity))) result = FN_ERR_IO;
    if (!result && !marker_set(0)) result = FN_ERR_IO;
    if (result) *got = 0;
    return result;
}
