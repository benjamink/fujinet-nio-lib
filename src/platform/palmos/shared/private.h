#ifndef NL_SHARED_PRIVATE_H
#define NL_SHARED_PRIVATE_H
#include <PalmOS.h>
#include "fujinet-nio.h"
#include "NioLib.h"

/* Shared-only hooks: the default static Palm build remains unchanged. */
UInt16 nl_bus_read(UInt32 address);
void nl_bus_write(UInt32 address, UInt16 value);
Err nl_card_attr(UInt16 card, UInt16 attr, void *value);
int nl_expired(UInt32 start, UInt32 wait);
void nl_deadline(UInt16 milliseconds);
UInt16 nl_transport_probe(UInt16 *hostP, UInt16 *busyP);
UInt16 nl_transport_begin(void);
UInt16 nl_transport_pending(void);
UInt16 nl_last_result(void);
void nl_clear_last_result(void);
Err nl_transport_reset(UInt16 transport);
Err nl_cs1_reset(void);
Err nl_serial_reset(void);
Err nl_usb_reset(void);
UInt16 nl_ambiguous(void);
Err nl_can_close(void);
Boolean nl_client_idle(void);
UInt16 nl_config_read(NioLibConfig *config);
Boolean nl_config_valid(const NioLibConfig *config);
Err nl_config_write(const NioLibConfig *config);
UInt16 nl_selected(void);
UInt16 nl_active(void);
void nl_transport_status(NioLibTransportStatus *status);
UInt16 nl_cs1_probe(UInt16 *, UInt16 *);
UInt16 nl_cs1_begin(void);
UInt16 nl_cs1_pending(void);
UInt16 nl_cs1_last_result(void);
UInt16 nl_cs1_ambiguous(void);
UInt16 nl_serial_open(const NioLibConfig *);
void nl_serial_close(void);
UInt16 nl_serial_ready(void);
UInt16 nl_serial_owned(void);
UInt16 nl_serial_pending(void);
UInt16 nl_serial_exchange(const UInt8 *, UInt16, UInt8 *, UInt16, UInt16 *);
UInt16 nl_usb_open(void);
void nl_usb_close(void);
UInt16 nl_usb_ready(void);
UInt16 nl_usb_pending(void);
UInt16 nl_usb_owned(void);
UInt16 nl_usb_available(void);
UInt16 nl_usb_exchange(const UInt8 *, UInt16, UInt8 *, UInt16, UInt16 *);
UInt16 nl_json_query(UInt16 handle, const Char *selector);
Boolean nl_payload_valid(const UInt8 *, const UInt8 *, UInt16, UInt16);

#ifdef FN_PALMOS_SHARED
#define FN_PALMOS_CARD_ATTR_GET nl_card_attr
#define FN_PALMOS_EXPIRED(start, wait) nl_expired((start), (wait))
#endif
#endif
