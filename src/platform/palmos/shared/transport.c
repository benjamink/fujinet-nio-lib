/* Palm-only packet dispatcher: one core, one backend per foreground lifetime. */
#include "private.h"
#include "fn_internal.h"
#include "fn_platform.h"

#define CONFIG_PREF 0x4e34
static UInt16 active;
static UInt16 last_reply = 0xffff;
uint8_t nl_cs1_init(void);
void nl_cs1_close(void);
uint8_t nl_cs1_ready(void);
uint8_t nl_cs1_exchange_buffers(const uint8_t *, uint16_t, uint8_t *, uint16_t, uint16_t *);

static void defaults(NioLibConfig *c)
{
    MemSet(c, sizeof(*c), 0);
    c->size = sizeof(*c); c->version = nioConfigVersion;
    c->transport = nioTransportSpringboard; c->serialBaud = 115200UL;
}
Boolean nl_config_valid(const NioLibConfig *c)
{
    return c && c->size == sizeof(*c) && c->version == nioConfigVersion &&
        (c->transport == nioTransportSpringboard || c->transport == nioTransportSerial || c->transport == nioTransportUSB) &&
        !(c->serialOptions & ~nioSerialRTSCTS) &&
        (c->serialBaud == 9600UL || c->serialBaud == 19200UL ||
         c->serialBaud == 38400UL || c->serialBaud == 57600UL || c->serialBaud == 115200UL) &&
        !c->reserved[0] && !c->reserved[1] && !c->reserved[2];
}
UInt16 nl_config_read(NioLibConfig *c)
{
    UInt16 size = sizeof(*c);
    Int16 version;
    defaults(c);
    version = PrefGetAppPreferences(nioLibCreator, CONFIG_PREF, c, &size, true);
    if (version == noPreferenceFound) { defaults(c); return nioConfigDefaulted; }
    if (version != nioConfigVersion || size != sizeof(*c) || !nl_config_valid(c)) {
        defaults(c); return nioConfigInvalid;
    }
    return 0;
}
Err nl_config_write(const NioLibConfig *c)
{
    NioLibConfig check;
    if (!nl_config_valid(c)) return nioLibErrParam;
    PrefSetAppPreferences(nioLibCreator, CONFIG_PREF, nioConfigVersion, (void *)c, sizeof(*c), true);
    if (nl_config_read(&check) || MemCmp(c, &check, sizeof(*c))) return nioLibErrGlobals;
    return errNone;
}
UInt16 nl_selected(void)
{
    NioLibConfig c;
    nl_config_read(&c);
    return c.transport;
}
UInt16 nl_active(void) { return active; }
uint8_t fn_transport_init(void)
{
    NioLibConfig c;
    UInt16 result;
    if (active) return fn_transport_ready() ? FN_OK : FN_ERR_NOT_READY;
    /* Corrupt/unknown saved settings are inspectable as safe defaults, but
     * must not silently open a different backend on the user's behalf. */
    if (nl_config_read(&c) == nioConfigInvalid) return FN_ERR_INVALID;
    result = c.transport == nioTransportSpringboard ? nl_cs1_init() :
             c.transport == nioTransportSerial ? nl_serial_open(&c) : nl_usb_open();
    if (!result) active = c.transport;
    return result;
}
void fn_transport_close(void)
{
    if (active == nioTransportSpringboard) nl_cs1_close();
    if (active == nioTransportSerial) nl_serial_close();
    if (active == nioTransportUSB) nl_usb_close();
    active = 0;
}
uint8_t fn_transport_ready(void)
{
    if (active == nioTransportSpringboard) return nl_cs1_ready();
    if (active == nioTransportSerial) return nl_serial_ready();
    if (active == nioTransportUSB) return nl_usb_ready();
    return 0;
}
uint8_t fn_transport_exchange_buffers(const uint8_t *request, uint16_t length,
        uint8_t *response, uint16_t capacity, uint16_t *got)
{
    UInt16 result;
    last_reply = 0xffff;
    if (!got) return FN_ERR_INVALID;
    *got = 0;
    if (!request || !response || length < FN_HEADER_SIZE || length > 1024 || !capacity || capacity > 1024)
        return FN_ERR_INVALID;
    if (active == nioTransportSpringboard)
        return nl_cs1_exchange_buffers(request, length, response, capacity, got);
    if (active == nioTransportSerial || active == nioTransportUSB) {
        result = active == nioTransportSerial ?
            nl_serial_exchange(request, length, response, capacity, got) :
            nl_usb_exchange(request, length, response, capacity, got);
        last_reply = result;
        return result;
    }
    return FN_ERR_NOT_READY;
}
uint8_t fn_transport_exchange(void)
{
    return fn_transport_exchange_buffers(_fn_transport_ctx.request, _fn_transport_ctx.req_len,
        _fn_transport_ctx.response, _fn_transport_ctx.resp_max, &_fn_transport_ctx.resp_len);
}
UInt16 nl_transport_begin(void)
{
    return active == nioTransportSpringboard ? nl_cs1_begin() :
           (active == nioTransportSerial || active == nioTransportUSB) ? FN_OK : FN_ERR_NOT_READY;
}
UInt16 nl_transport_probe(UInt16 *host, UInt16 *busy)
{
    NioLibConfig c;
    *host = *busy = 0;
    if (nl_config_read(&c) == nioConfigInvalid) return FN_ERR_INVALID;
    if ((active ? active : c.transport) == nioTransportSpringboard) return nl_cs1_probe(host, busy);
    if ((active ? active : c.transport) == nioTransportUSB)
        return nl_usb_pending() ? FN_ERR_BUSY : nl_usb_available() ? FN_OK : FN_ERR_NOT_FOUND;
    /* Serial manager presence is not proof of a cable or an NIO peer. */
    return nl_serial_pending() ? FN_ERR_BUSY : FN_OK;
}
UInt16 nl_transport_pending(void)
{
    return active == nioTransportUSB ? nl_usb_pending() :
           active == nioTransportSerial ? nl_serial_pending() : nl_cs1_pending();
}
UInt16 nl_ambiguous(void)
{
    return active == nioTransportUSB ? nl_usb_pending() :
           active == nioTransportSerial ? nl_serial_pending() : nl_cs1_ambiguous();
}
UInt16 nl_last_result(void)
{
    return active != nioTransportSpringboard ? last_reply : nl_cs1_last_result();
}
void nl_clear_last_result(void) { last_reply = 0xffff; }
Err nl_transport_reset(UInt16 transport)
{
    if (active || nl_usb_owned() || nl_serial_owned()) return nioLibErrBusy;
    if (transport == nioTransportSpringboard) return nl_cs1_reset();
    if (transport == nioTransportSerial) return nl_serial_reset();
    if (transport == nioTransportUSB) return nl_usb_reset();
    return nioLibErrParam;
}
void nl_transport_status(NioLibTransportStatus *s)
{
    NioLibConfig c;
    s->configFlags = nl_config_read(&c);
    s->selected = c.transport; s->active = active;
    s->flags = nioStatusSupported | nioStatusAvailabilityUnknown;
    s->maxPacket = 1024; s->maxSessions = FN_MAX_SESSIONS;
    s->result = s->configFlags == nioConfigInvalid ? FN_ERR_INVALID : FN_OK;
    if (c.transport == nioTransportUSB && !nl_usb_available()) s->result = FN_ERR_NOT_FOUND;
    if (active) s->flags |= nioStatusActive;
    if (active && fn_transport_ready()) s->flags |= nioStatusAvailable;
    if ((c.transport == nioTransportSpringboard && nl_cs1_pending()) ||
        (c.transport == nioTransportSerial && nl_serial_pending()) ||
        (c.transport == nioTransportUSB && nl_usb_pending())) {
        s->flags |= nioStatusRecoveryRequired; s->result = FN_ERR_BUSY;
    }
}
