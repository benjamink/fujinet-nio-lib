/* Application-owned compatibility shim. Link only libniolib-client.a, never
 * a complete static NIO library, when using the installed shared library. */
#ifndef NL_PALM_CLIENT_H
#define NL_PALM_CLIENT_H
#include "NioLib.h"
#include "fujinet-nio.h"
UInt16 NioLibClientLastError(void);
UInt16 NioLibClientLastResult(void);
UInt16 NioLibClientCapacity(void);
void NioLibClientCleanupReport(NioLibCleanup *resultP);
uint8_t NioLibClientProbe(NioLibAvailability *availabilityP);
#endif
