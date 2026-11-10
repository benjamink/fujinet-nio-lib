#ifndef NIO_SERIAL_VDRV_COMPAT_H
#define NIO_SERIAL_VDRV_COMPAT_H
#ifdef NIO_PALMOS_SDK31
/* UsbMgr.h includes this for optional virtual-driver control macros only.
 * NioLib uses no virtual-driver controls. Supply no unverified declarations. */
#else
#include_next <SerialVdrv.h>
#endif
#endif
