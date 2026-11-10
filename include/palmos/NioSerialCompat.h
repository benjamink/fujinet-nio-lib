#ifndef NIO_SERIAL_COMPAT_H
#define NIO_SERIAL_COMPAT_H
#include <PalmOS.h>
#include <SerialMgrOld.h>
#ifdef NIO_PALMOS_SDK31
/* Narrow declarations for the optional OS 3.3+ SerialDispatch ABI. Callers
 * MUST feature-test sysFtrNewSerialPresent before executing any Srm trap.
 * Values/signatures audited against SDK 3.5 SerialMgr.h; no newer SDK needed
 * for the baseline build. The legacy Ser path remains native SDK 3.1. */
#define sysFtrNewSerialPresent 1
#define sysFileCSerialMgr 'smgr'
#define serPortCradlePort 0x8000
#define srmSettingsFlagBitsPerChar8 0xc0UL
#define srmSettingsFlagRTSAutoM 0x10UL
#define srmSettingsFlagCTSAutoM 0x20UL
#define srmCtlSetFlags 3
#define srmCtlSetCtsTimeout 5
#define NIO_SERIAL_TRAP(selector) __attribute__((__callseq__("moveq #" #selector ",%%d2; trap #15; dc.w 0xA367")))
Err SrmOpen(UInt32, UInt32, UInt16 *) NIO_SERIAL_TRAP(1);
Err SrmClose(UInt16) NIO_SERIAL_TRAP(3);
Err SrmClearErr(UInt16) NIO_SERIAL_TRAP(9);
Err SrmControl(UInt16, UInt16, void *, UInt16 *) NIO_SERIAL_TRAP(10);
UInt32 SrmSend(UInt16, void *, UInt32, Err *) NIO_SERIAL_TRAP(11);
Err SrmSendWait(UInt16) NIO_SERIAL_TRAP(12);
Err SrmSendCheck(UInt16, UInt32 *) NIO_SERIAL_TRAP(13);
Err SrmSendFlush(UInt16) NIO_SERIAL_TRAP(14);
UInt32 SrmReceive(UInt16, void *, UInt32, Int32, Err *) NIO_SERIAL_TRAP(15);
Err SrmReceiveFlush(UInt16, Int32) NIO_SERIAL_TRAP(18);
Err SrmSetReceiveBuffer(UInt16, void *, UInt16) NIO_SERIAL_TRAP(19);
#undef NIO_SERIAL_TRAP
#endif
#endif
