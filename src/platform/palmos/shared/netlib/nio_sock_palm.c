/* nio_sock.h on Palm OS NetLib. NetLib brings up whatever service the
 * Network preferences panel names (PPP over Bluetooth, a modem, a cradle),
 * so the first nio_sock_start() may show the system's connection progress.
 * A 68k app sees network byte order as its own, on OS 5 (PACE) too. */
#include <PalmOS.h>
#include <NetMgr.h>
#include "nio_sock.h"

/* Text constants the library audit (scripts/palmos_shared_image.py) checks byte for byte: objdump decodes them
 * as instructions, some through A5. */
static const char k_sock_netlib[] = "Net.lib";

/* DNS and connect can cross a slow dial-up style link. */
#define CONNECT_SECONDS 20
#define SEND_SECONDS 10

int nio_sock_start(NioNet *n)
{
    UInt16 ifErrs = 0;
    Err err;

    if (n->open) return 0;
    if (SysLibFind((Char *)k_sock_netlib, &n->ref) != errNone) return NIO_SOCK_ERROR;
    err = NetLibOpen(n->ref, &ifErrs);
    if (err != errNone && err != netErrAlreadyOpen) return NIO_SOCK_ERROR;
    if (ifErrs) {
        NetLibClose(n->ref, true);
        return NIO_SOCK_ERROR;
    }
    n->open = 1;
    return 0;
}

void nio_sock_stop(NioNet *n)
{
    /* Not immediate: NetLib keeps the link for its close-wait time, so the
     * next app does not have to dial again. */
    if (n->open) NetLibClose(n->ref, n->pad ? true : false);
    n->open = 0;
}

static Boolean resolve(NioNet *n, const char *host, NetIPAddr *addr)
{
    NetHostInfoBufPtr info;
    Err err;
    Boolean ok = false;

    *addr = NetLibAddrAToIN(n->ref, (Char *)host);
    if (*addr != (NetIPAddr)-1) return true;
    info = (NetHostInfoBufPtr)MemPtrNew(sizeof(NetHostInfoBufType));
    if (!info) return false;
    if (NetLibGetHostByName(n->ref, (Char *)host, info, CONNECT_SECONDS * SysTicksPerSecond(),
                            &err) && info->address[0]) {
        *addr = info->address[0];
        ok = true;
    }
    MemPtrFree(info);
    return ok;
}

int nio_sock_tcp_connect(NioNet *n, const char *host, uint16_t port, long *sock)
{
    NetSocketAddrINType to;
    NetSocketRef s;
    Err err;
    Int32 wait = CONNECT_SECONDS * SysTicksPerSecond();

    if (!n->open || !resolve(n, host, &to.addr)) return NIO_SOCK_ERROR;
    to.family = netSocketAddrINET;
    to.port = port;
    s = NetLibSocketOpen(n->ref, netSocketAddrINET, netSocketTypeStream, 0, wait, &err);
    if (s < 0) return NIO_SOCK_ERROR;
    if (NetLibSocketConnect(n->ref, s, (NetSocketAddrType *)&to, sizeof(to), wait, &err) < 0) {
        NetLibSocketClose(n->ref, s, wait, &err);
        return NIO_SOCK_ERROR;
    }
    *sock = s;
    return 0;
}

int nio_sock_send(NioNet *n, long sock, const uint8_t *data, uint16_t len)
{
    Err err;
    Int16 got = NetLibSend(n->ref, (NetSocketRef)sock, (void *)data, len, 0, 0, 0,
                         SEND_SECONDS * SysTicksPerSecond(), &err);

    return got <= 0 ? NIO_SOCK_ERROR : got;
}

int nio_sock_recv(NioNet *n, long sock, uint8_t *data, uint16_t max, uint16_t wait_ms)
{
    Err err;
    Int32 wait = ((Int32)wait_ms * SysTicksPerSecond() + 999) / 1000;
    Int16 got = NetLibReceive(n->ref, (NetSocketRef)sock, data, max, 0, 0, 0, wait, &err);

    if (got >= 0) return got;
    if (err == netErrTimeout || err == netErrWouldBlock) return NIO_SOCK_WOULDBLOCK;
    /* The peer went away: the reader sees it as the end of the stream. */
    if (err == netErrSocketClosedByRemote) return 0;
    return NIO_SOCK_ERROR;
}

void nio_sock_close(NioNet *n, long sock)
{
    Err err;

    NetLibSocketClose(n->ref, (NetSocketRef)sock, SysTicksPerSecond(), &err);
}

void nio_sock_local_ip(NioNet *n, char *out, uint16_t cap)
{
    UInt32 creator;
    UInt16 instance, index, len;
    NetIPAddr addr;
    Char text[20];

    out[0] = 0;
    if (!n->open || cap < 16) return;
    /* The first interface that is not loopback and has an address. */
    for (index = 0; NetLibIFGet(n->ref, index, &creator, &instance) == errNone; index++) {
        if (creator == netIFCreatorLoop) continue;
        addr = 0;
        len = sizeof(addr);
        if (NetLibIFSettingGet(n->ref, creator, instance, netIFSettingActualIPAddr, &addr,
                               &len) == errNone && addr) {
            NetLibAddrINToA(n->ref, addr, text);
            StrCopy(out, text);
            return;
        }
    }
}
