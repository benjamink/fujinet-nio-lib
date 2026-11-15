/* Host stand-in for the Mac's <Files.h>: only what the mac68k transport uses.
 * The test supplies the drive queue (FN_MAC_DRVQ_HEAD) and plays the HD20
 * driver and the board's controller behind PBReadSync/PBWriteSync. */
#ifndef MAC68K_TRANSPORT_STUB_FILES_H
#define MAC68K_TRANSPORT_STUB_FILES_H

typedef short OSErr;
typedef char *Ptr;

enum { noErr = 0, ioErr = -36 };
enum { fsFromStart = 1 };

typedef struct QElem {
    struct QElem *qLink;
    short qType;
} QElem, *QElemPtr;

typedef struct QHdr {
    short qFlags;
    QElemPtr qHead;
    QElemPtr qTail;
} QHdr, *QHdrPtr;

typedef struct DrvQEl {
    QElemPtr qLink;
    short qType;
    short dQDrive;
    short dQRefNum;
    short dQFSID;
    unsigned short dQDrvSz;
    unsigned short dQDrvSz2;
} DrvQEl;

typedef struct IOParam {
    short ioRefNum;
    short ioVRefNum;
    Ptr ioBuffer;
    long ioReqCount;
    long ioActCount;
    short ioPosMode;
    long ioPosOffset;
} IOParam;

typedef union ParamBlockRec {
    IOParam ioParam;
} ParamBlockRec;

/* The test's fake drive queue, in place of the low-memory DrvQHdr. */
QElemPtr mac_test_drive_queue(void);
#define FN_MAC_DRVQ_HEAD() mac_test_drive_queue()

#endif
