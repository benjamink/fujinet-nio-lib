/* Network transport (4): the NIO device runs inside NioLib, on the Palm, and reaches the Internet through the Palm's
 * own network library (NetLib): PPP over Bluetooth to a gateway, Wi-Fi, a modem or a cradle, whichever service the
 * Network preferences select. No FujiNet hardware is involved: each request packet goes to the NIO core in
 * netlib/ (NetworkDevice tcp:// and http://, FujiDevice info, WiFiDevice status), which answers it as a FujiNet
 * would. The core's state (about 6 KB) is allocated for one foreground client session only, so the library's global
 * data stays small whichever transport is chosen. */
#include "private.h"
#include <NetMgr.h>
#include "fn_internal.h"
#include "netlib/nio.h"

/* Text constants the library audit (scripts/palmos_shared_image.py) checks byte for byte: objdump decodes them
 * as instructions, some through A5. */
static const char k_netlib[] = "Net.lib";

static NioState *nl_nio;
#define nl_on (nl_nio != NULL)
/* The last session's counters, kept when its state is freed, so NIO Config can show what a run did after it ends. */
static struct { UInt16 connects, failures; UInt32 bytesIn, bytesOut; } nl_last;

/* NetLib is present: no proof of a configured service, a link or the Internet. */
UInt16 nl_netlib_available(void)
{
    UInt16 ref;
    return SysLibFind((Char *)k_netlib, &ref) == errNone;
}

UInt16 nl_netlib_open(void)
{
    NioState *nio;
    /* The state is allocated at BeginClient (fn_transport_init) and freed at EndClient (fn_shutdown ->
     * fn_transport_close). It is system-owned (owner 0) because the library's globals are, and an app's own chunks
     * are freed when it exits. A fresh open never inherits an earlier state. */
    if (nl_on) nl_netlib_close();
    if (!nl_netlib_available()) return FN_ERR_NOT_FOUND;
    nio = MemPtrNew(sizeof(NioState));
    if (!nio) return FN_ERR_IO;
    MemPtrSetOwner(nio, 0);             /* the library's, not the app's: freed at close */
    nio_init(nio);
    nl_nio = nio;
    return FN_OK;
}

void nl_netlib_close(void)
{
    /* Closes every socket; NetLib keeps the link for its idle time, so the next app need not dial again. */
    if (!nl_on) return;
    nl_last.connects = nl_nio->net.connects;
    nl_last.failures = nl_nio->net.failures;
    nl_last.bytesIn = nl_nio->net.bytesIn;
    nl_last.bytesOut = nl_nio->net.bytesOut;
    nio_shutdown(nl_nio);
    MemPtrFree(nl_nio);
    nl_nio = NULL;
}

UInt16 nl_netlib_ready(void) { return nl_on; }

UInt16 nl_netlib_exchange(const UInt8 *request, UInt16 length, UInt8 *response, UInt16 capacity, UInt16 *got)
{
    *got = 0;
    if (!nl_on) return FN_ERR_NOT_READY;
    *got = nio_exchange(nl_nio, request, length, response, capacity);
    return *got ? FN_OK : FN_ERR_INVALID;
}

static void nl_address(UInt16 ref, NioLibNetLibStatus *s)
{
    NioNet probe;
    MemSet(&probe, sizeof(probe), 0);
    probe.ref = ref;
    probe.open = 1;
    nio_sock_local_ip(&probe, s->ip, sizeof(s->ip));
}

/* The link and the counters, for NIO Config. During a client session they are the session's; otherwise the counters
 * are the last session's and the link is NetLib's: up while anyone has NetLib open, or while it is in its close-wait
 * (closed by everyone, the link kept for its idle time, as after NIO Config's Connect or a finished session). The
 * close-wait check takes NetLib for a moment, which never dials, and lets it go again, which starts its idle time
 * over: a link stays up while NIO Config keeps showing it. */
void nl_netlib_status(NioLibNetLibStatus *s)
{
    UInt16 ref = 0, count = 0;
    s->flags = 0;
    s->sessions = 0;
    s->connects = nl_last.connects;
    s->failures = nl_last.failures;
    s->bytesIn = nl_last.bytesIn;
    s->bytesOut = nl_last.bytesOut;
    s->ip[0] = 0;
    if (!nl_netlib_available()) return;
    s->flags |= nioNetLibPresent;
    if (nl_on) {
        s->flags |= nioNetLibSession;
        if (nl_nio->net_up) s->flags |= nioNetLibUp;
        s->sessions = nio_sessions_open(nl_nio);
        s->connects = nl_nio->net.connects;
        s->failures = nl_nio->net.failures;
        s->bytesIn = nl_nio->net.bytesIn;
        s->bytesOut = nl_nio->net.bytesOut;
        if (nl_nio->net_up) nio_sock_local_ip(&nl_nio->net, s->ip, sizeof(s->ip));
        return;
    }
    if (SysLibFind((Char *)k_netlib, &ref) || NetLibOpenCount(ref, &count)) return;
    if (count) {
        s->flags |= nioNetLibUp;
        nl_address(ref, s);
    } else if (NetLibOpenIfCloseWait(ref) == errNone) {
        s->flags |= nioNetLibUp;
        nl_address(ref, s);
        NetLibClose(ref, false);
    }
}

/* NIO Config's Hang up: the link goes now, not after NetLib's idle time. A link in its close-wait is taken and closed
 * at once; one that something still has open (a client session, another program) is left alone: nioLibErrBusy. */
Err nl_netlib_hangup(void)
{
    UInt16 ref = 0, count = 0;
    if (nl_on) return nioLibErrBusy;
    if (SysLibFind((Char *)k_netlib, &ref) || NetLibOpenCount(ref, &count)) return errNone;
    if (count) return nioLibErrBusy;
    if (NetLibOpenIfCloseWait(ref) == errNone) NetLibClose(ref, true);
    return errNone;
}
