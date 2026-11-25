#ifndef AMIGA_TRANSPORT_HOST_PROTO_EXEC_H
#define AMIGA_TRANSPORT_HOST_PROTO_EXEC_H

#include <exec/io.h>
#include <exec/types.h>

LONG OpenDevice(CONST_STRPTR name, ULONG unit, struct IORequest *ioRequest,
                ULONG flags);
void CloseDevice(struct IORequest *ioRequest);
BYTE DoIO(struct IORequest *ioRequest);
void AbortIO(struct IORequest *ioRequest);
void WaitIO(struct IORequest *ioRequest);
void SendIO(struct IORequest *ioRequest);
struct IORequest *CheckIO(struct IORequest *ioRequest);
ULONG Wait(ULONG signalSet);
ULONG SetSignal(ULONG newSignals, ULONG signalSet);

#endif
