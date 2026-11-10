/* Project compatibility umbrella; SDK contents remain external. */
#ifndef NIO_PALMOS_COMPAT_H
#define NIO_PALMOS_COMPAT_H
#ifdef NIO_PALMOS_SDK31
#define PILOT_PRECOMPILED_HEADERS_OFF
#include <Pilot.h>
typedef unsigned char UInt8;
typedef signed char Int8;
typedef VoidPtr MemPtr;
typedef VoidHand MemHandle;
typedef SWord Coord;
typedef Ptr NioPalmLaunchPtr;
#define errNone 0
#else
#include_next <PalmOS.h>
typedef MemPtr NioPalmLaunchPtr;
#endif
#endif
