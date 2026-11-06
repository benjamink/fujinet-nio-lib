/* Palm OS system-library ABI 1.4; original 1.0 traps stay unchanged. */
#ifndef NIO_PALM_LIBRARY_H
#define NIO_PALM_LIBRARY_H
#include <PalmOS.h>

#define nioLibName "NioLib"
#define nioLibCreator 'NiL1'
#define nioTestCreator 'NiT1'
#define nioLibType 'libr'
#define nioLibAbiMajor 1
#define nioLibAbiMinor 4
#define nioLibCapA4Globals 0x00000001UL
#define nioLibCapNetwork   0x00000002UL
#define nioLibCapClock     0x00000004UL
#define nioLibCapClient    0x00000008UL
#define nioLibCapTransports 0x00000010UL
#define nioLibCapUSB 0x00000020UL
#define nioLibCapJSONQuery 0x00000040UL
#define nioLibCapTestOnly  0x80000000UL
/* Legacy bit/name retained: it now identifies the foundation diagnostics. */
#define nioLibCapTestGlobals nioLibCapTestOnly
#define nioLibErrParam       (appErrorClass | 1)
#define nioLibErrNotOpen     (appErrorClass | 2)
#define nioLibErrOverflow    (appErrorClass | 3)
#define nioLibErrBadImage    (appErrorClass | 4)
#define nioLibErrGlobals     (appErrorClass | 5)
#define nioLibErrBusy        (appErrorClass | 6)
#define nioLibErrOwner       (appErrorClass | 7)
#define nioLibTrapVersion    (sysLibTrapCustom + 0)
#define nioLibTrapCaps       (sysLibTrapCustom + 1)
#define nioLibTrapTest       (sysLibTrapCustom + 2)
#define nioLibTrapBegin      (sysLibTrapCustom + 3)
#define nioLibTrapEnd        (sysLibTrapCustom + 4)
#define nioLibTrapProbe      (sysLibTrapCustom + 5)
#define nioLibTrapNetOpen    (sysLibTrapCustom + 6)
#define nioLibTrapNetInfo    (sysLibTrapCustom + 7)
#define nioLibTrapNetRead    (sysLibTrapCustom + 8)
#define nioLibTrapNetWrite   (sysLibTrapCustom + 9)
#define nioLibTrapNetClose   (sysLibTrapCustom + 10)
#define nioLibTrapNetReady   (sysLibTrapCustom + 11)
#define nioLibTrapClockGet   (sysLibTrapCustom + 12)
#define nioLibTrapClockZone  (sysLibTrapCustom + 13)

/* Saved preference NiL1/0x4e34, version 1. Palm native big-endian layout.
 * No pointers; size 24 on Palm OS. Reserved fields must be zero. */
#define nioTransportSpringboard 1
#define nioTransportSerial 2
#define nioTransportUSB 3
#define nioSerialRTSCTS 1
#define nioConfigVersion 1
#define nioConfigDefaulted 1
#define nioConfigInvalid 2
#define nioStatusSupported 1
#define nioStatusAvailable 2
#define nioStatusActive 4
#define nioStatusRecoveryRequired 8
#define nioStatusAvailabilityUnknown 16
#define nioStatusUSBUnsupported 32
typedef struct {
    UInt16 size, version, transport, serialOptions;
    UInt32 serialBaud;
    UInt32 reserved[3];
} NioLibConfig;
typedef struct {
    UInt16 size, selected, active, flags, maxPacket, maxSessions;
    UInt16 configFlags, result;
} NioLibTransportStatus;
#define nioLibTrapGetConfig (sysLibTrapCustom + 14)
#define nioLibTrapSetConfig (sysLibTrapCustom + 15)
#define nioLibTrapTransportStatus (sysLibTrapCustom + 16)
#define nioLibTrapNetJsonQuery (sysLibTrapCustom + 17)

typedef struct {
    UInt16 size;
    UInt16 abiMajor;
    UInt16 abiMinor;
    UInt16 implementation;
} NioLibVersion;

typedef struct {
    UInt16 size;
    UInt16 openCount;
    UInt32 initialized;
    UInt32 independent;
    UInt32 before;
    UInt32 after;
    UInt32 pointedValue;
    UInt32 functionValue;
    UInt16 dataPointerOK;
    UInt16 distinctOK;
} NioLibGlobalsResult;

/* ABI structures use only Palm fixed-width scalars and caller-owned pointers.
 * They are not FujiBus packet layouts or internal NIO session structures. */
typedef struct {
    UInt16 size, method, flags, translationType, translationFlags, translated;
    const Char *url;
    const Char *selector;
} NioLibOpenArgs;
typedef struct { UInt16 handle, flags, available; } NioLibReadyEntry;
typedef struct {
    UInt16 size, modulePresent, hostPresent, linkBusy, maxSessions, reserved;
} NioLibAvailability;
typedef struct {
    UInt16 size, attempted, confirmed, uncertain, ambiguousOperation, reserved;
} NioLibCleanup;

#ifdef NIOLIB_IMPLEMENTATION
#define NIO_LIB_TRAP(n)
#else
#define NIO_LIB_TRAP(n) SYS_TRAP(n)
#endif
Err NioLibOpen(UInt16 refNum) NIO_LIB_TRAP(sysLibTrapOpen);
/* remainingP is required; only zero means it is safe for the caller to remove.
 * A successful Close consumes one app Open. Nonzero may also represent an
 * idle library-owned hold for driver resources awaiting confirmed teardown. */
Err NioLibClose(UInt16 refNum, UInt16 *remainingP) NIO_LIB_TRAP(sysLibTrapClose);
Err NioLibSleep(UInt16 refNum) NIO_LIB_TRAP(sysLibTrapSleep);
Err NioLibWake(UInt16 refNum) NIO_LIB_TRAP(sysLibTrapWake);
/* These two queries work before Open and allocate nothing. */
Err NioLibGetVersion(UInt16 refNum, NioLibVersion *versionP) NIO_LIB_TRAP(nioLibTrapVersion);
Err NioLibGetCapabilities(UInt16 refNum, UInt32 *capsP) NIO_LIB_TRAP(nioLibTrapCaps);
Err NioLibTestGlobals(UInt16 refNum, UInt32 delta, NioLibGlobalsResult *resultP)
    NIO_LIB_TRAP(nioLibTrapTest);
/* resultP is an FN_* service/transport result, distinct from Palm Err. */
Err NioLibBeginClient(UInt16 refNum, UInt32 *tokenP, UInt16 *capacityP, UInt16 *resultP)
    NIO_LIB_TRAP(nioLibTrapBegin);
Err NioLibEndClient(UInt16 refNum, UInt32 token, NioLibCleanup *cleanupP, UInt16 *resultP)
    NIO_LIB_TRAP(nioLibTrapEnd);
Err NioLibProbe(UInt16 refNum, NioLibAvailability *availabilityP, UInt16 *resultP)
    NIO_LIB_TRAP(nioLibTrapProbe);
Err NioLibNetOpen(UInt16 refNum, UInt32 token, const NioLibOpenArgs *argsP,
                 UInt16 *handleP, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapNetOpen);
Err NioLibNetInfo(UInt16 refNum, UInt32 token, UInt16 handle, UInt16 *httpP,
                 UInt32 *lengthP, UInt16 *flagsP, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapNetInfo);
Err NioLibNetRead(UInt16 refNum, UInt32 token, UInt16 handle, UInt32 offset,
                 UInt8 *buffer, UInt16 capacity, UInt16 *readP, UInt16 *flagsP,
                 UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapNetRead);
Err NioLibNetWrite(UInt16 refNum, UInt32 token, UInt16 handle, UInt32 offset,
                  const UInt8 *buffer, UInt16 length, UInt16 *writtenP,
                  UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapNetWrite);
Err NioLibNetClose(UInt16 refNum, UInt32 token, UInt16 handle, UInt16 *resultP)
    NIO_LIB_TRAP(nioLibTrapNetClose);
Err NioLibNetReady(UInt16 refNum, UInt32 token, NioLibReadyEntry *entries,
                  UInt16 capacity, UInt16 *countP, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapNetReady);
/* Clock time is eight little-endian bytes, not a compiler-specific uint64 ABI. */
Err NioLibClockGet(UInt16 refNum, UInt32 token, UInt8 *timeLE8, UInt16 *resultP)
    NIO_LIB_TRAP(nioLibTrapClockGet);
Err NioLibClockTimezone(UInt16 refNum, UInt32 token, Char *buffer, UInt16 capacity,
                       UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockZone);
/* Require Open, but never open a physical transport. SetConfig is idle-only. */
Err NioLibGetConfig(UInt16 refNum, NioLibConfig *configP, UInt16 *flagsP)
    NIO_LIB_TRAP(nioLibTrapGetConfig);
Err NioLibSetConfig(UInt16 refNum, const NioLibConfig *configP)
    NIO_LIB_TRAP(nioLibTrapSetConfig);
Err NioLibGetTransportStatus(UInt16 refNum, NioLibTransportStatus *statusP)
    NIO_LIB_TRAP(nioLibTrapTransportStatus);
/* ABI 1.4: reselect JSON on an owned open HTTP response, without another GET.
 * FN_ERR_NOT_READY means configuration applied, body pending: poll NetRead at
 * offset zero, do not repeat configuration (which could reset partial input). */
Err NioLibNetJsonQuery(UInt16 refNum, UInt32 token, UInt16 handle,
                      const Char *selector, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapNetJsonQuery);
#undef NIO_LIB_TRAP
#endif
