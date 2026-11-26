/* Host stand-in for the Mac's <Devices.h>: the driver-level block calls. */
#ifndef MAC68K_TRANSPORT_STUB_DEVICES_H
#define MAC68K_TRANSPORT_STUB_DEVICES_H

#include <Files.h>

OSErr PBReadSync(ParamBlockRec *pb);
OSErr PBWriteSync(ParamBlockRec *pb);

#endif
