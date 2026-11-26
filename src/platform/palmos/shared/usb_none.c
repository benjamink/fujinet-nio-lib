/* A NioLib built without the Handspring SDK (no UsbMgr.h): no USB backend. Selecting USB fails as not found, and
 * the library does not report nioLibCapUSB, so NIO Config does not offer it. */
#include "private.h"

const UInt16 nl_usb_none = 1;           /* marks this build (tests) */

UInt16 nl_usb_pending(void) { return 0; }
UInt16 nl_usb_owned(void) { return 0; }
Err nl_usb_reset(void) { return errNone; }
UInt16 nl_usb_available(void) { return 0; }
void nl_usb_close(void) { }
UInt16 nl_usb_open(void) { return FN_ERR_NOT_FOUND; }
UInt16 nl_usb_ready(void) { return 0; }
UInt16 nl_usb_exchange(const UInt8 *request, UInt16 length, UInt8 *response, UInt16 capacity, UInt16 *got)
{
    (void)request; (void)length; (void)response; (void)capacity;
    *got = 0;
    return FN_ERR_NOT_FOUND;
}
