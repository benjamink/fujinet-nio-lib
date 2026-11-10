#ifndef NIO_SERIAL_OLD_COMPAT_H
#define NIO_SERIAL_OLD_COMPAT_H
#ifdef NIO_PALMOS_SDK31
#include <PalmOS.h>
#include <SerialMgr.h>
#else
#include_next <SerialMgrOld.h>
#endif
#endif
