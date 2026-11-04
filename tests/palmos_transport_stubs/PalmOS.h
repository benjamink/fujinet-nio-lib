/* The few PalmOS.h names the Springboard client uses, for a 64-bit host. */
#ifndef PALMOS_STUB_H
#define PALMOS_STUB_H

#include <stdint.h>

typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef uint16_t Err;

UInt32 TimGetTicks(void);
UInt16 SysTicksPerSecond(void);
Err HsCardAttrGet(UInt16 cardNo, UInt16 attr, void *valueP);

#endif
