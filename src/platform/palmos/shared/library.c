#define NIOLIB_IMPLEMENTATION
#include "NioLib.h"
#include <EntryPoints.h>
#include "image.h"
#include "private.h"

/* Real compiler-owned .data/.bss, not a manually substituted context. */
UInt32 nl_initialized = 0x13579bdfUL;
UInt32 nl_independent = 0x2468ace0UL;
UInt32 nl_counter;
UInt16 nl_opens;
UInt32 *nl_data_pointer = &nl_initialized;
UInt32 NioLibTransform(UInt32 value) { return value ^ 0xa55a5aa5UL; }
UInt32 (*nl_function_pointer)(UInt32) = NioLibTransform;

extern void start(void);
extern void NioLibDispatch(void);

/* These helpers alone enter with compiler-supported library A4 setup. Outer
 * lifecycle guards must work when globalsP is NULL, including first Open. */
static Err globals_open(UInt16 refNum) SYSLIB_POINTER_ENTRYPOINT;
static Err globals_close(UInt16 refNum, UInt16 *remainingP) SYSLIB_POINTER_ENTRYPOINT;
static Err globals_test(UInt16 refNum, UInt32 delta, NioLibGlobalsResult *r)
    SYSLIB_POINTER_ENTRYPOINT;

Err NioLibInstall(UInt16 refNum, SysLibTblEntryPtr entry)
{
    (void)refNum;
    entry->dispatchTblP = (MemPtr *)&NioLibDispatch;
    entry->globalsP = NULL;
    return errNone;
}

static Err allocate_globals(SysLibTblEntryPtr entry)
{
    DmOpenRef db;
    UInt16 index;
    MemHandle imageH, codeH;
    unsigned char *image;
    void *data;
    UInt32 total;
    Err result = nioLibErrBadImage;
    /* M1 is installed in internal RAM, card 0. Explicit library DB lookup
     * avoids accidentally finding an app's resource of the same type/id. */
    db = DmOpenDatabase(0, entry->dbID, dmModeReadOnly);
    if (!db) return nioLibErrBadImage;
    index = DmFindResource(db, NL_IMAGE_TYPE, NL_IMAGE_ID, NULL);
    imageH = index == 0xffff ? NULL : DmGetResourceIndex(db, index);
    index = DmFindResource(db, 'libr', 0, NULL);
    codeH = index == 0xffff ? NULL : DmGetResourceIndex(db, index);
    if (imageH && codeH) {
        image = MemHandleLock(imageH);
        if (image && nl_image_validate(image, MemHandleSize(imageH), MemHandleSize(codeH))) {
            total = ((UInt32)image[8] << 24) | ((UInt32)image[9] << 16) |
                    ((UInt32)image[10] << 8) | image[11];
            data = MemPtrNew(total);
            if (!data) result = memErrNotEnoughSpace;
            else if (MemPtrSetOwner(data, 0) != errNone) {
                MemPtrFree(data);
                result = nioLibErrGlobals;
            } else {
                nl_image_copy(image, data, (UInt32)data, (UInt32)&start);
                entry->globalsP = data;
                result = errNone;
            }
        }
        if (image) MemHandleUnlock(imageH);
    }
    if (imageH) DmReleaseResource(imageH);
    if (codeH) DmReleaseResource(codeH);
    DmCloseDatabase(db);
    return result;
}

Err NioLibOpen(UInt16 refNum)
{
    SysLibTblEntryPtr entry = SysLibTblEntry(refNum);
    Err err;
    if (!entry) return nioLibErrParam;
    if (!entry->globalsP) {
        err = allocate_globals(entry);
        if (err) return err;
    }
    return globals_open(refNum);
}

static Err globals_open(UInt16 refNum)
{
    (void)refNum;
    /* A previous app may have balanced its last Close while the library kept
     * a physical-resource hold. Reap it before granting any new external Open. */
    if (!nl_opens && nl_can_close()) return nioLibErrBusy;
    if (nl_opens == 0xffff) return nioLibErrOverflow;
    ++nl_opens;
    return errNone;
}

Err NioLibClose(UInt16 refNum, UInt16 *remainingP)
{
    SysLibTblEntryPtr entry = SysLibTblEntry(refNum);
    Err err;
    if (!remainingP) return nioLibErrParam;
    *remainingP = 0;
    if (!entry || !entry->globalsP) return nioLibErrNotOpen;
    err = globals_close(refNum, remainingP);
    if (!err && !*remainingP) {
        /* A4 helper has returned and restored the caller's register already. */
        MemPtrFree(entry->globalsP);
        entry->globalsP = NULL;
    }
    return err;
}

static Err globals_close(UInt16 refNum, UInt16 *remainingP)
{
    Err err;
    (void)refNum;
    if (!nl_opens) return nioLibErrNotOpen;
    if (nl_opens == 1) {
        if (!nl_client_idle()) return nioLibErrBusy;
        err = nl_can_close();
        --nl_opens;
        /* This is a library-owned residency hold, not an orphaned app Open.
         * Close succeeds and consumes the app reference, but nonzero remaining
         * forbids SysLibRemove/free until a later Open confirms driver release. */
        *remainingP = err ? 1 : 0;
    } else *remainingP = --nl_opens;
    return errNone;
}

Err NioLibSleep(UInt16 refNum) { (void)refNum; return errNone; }
Err NioLibWake(UInt16 refNum) { (void)refNum; return errNone; }

Err NioLibGetVersion(UInt16 refNum, NioLibVersion *v)
{
    (void)refNum;
    if (!v || v->size != sizeof(*v)) return nioLibErrParam;
    v->abiMajor = nioLibAbiMajor; v->abiMinor = nioLibAbiMinor;
    v->implementation = 7;
    return errNone;
}

Err NioLibGetCapabilities(UInt16 refNum, UInt32 *capsP)
{
    (void)refNum;
    if (!capsP) return nioLibErrParam;
    *capsP = nioLibCapA4Globals | nioLibCapTestOnly | nioLibCapNetwork |
             nioLibCapClock | nioLibCapClient | nioLibCapTransports | nioLibCapUSB | nioLibCapJSONQuery;
    return errNone;
}

Err NioLibTestGlobals(UInt16 refNum, UInt32 delta, NioLibGlobalsResult *r)
{
    SysLibTblEntryPtr entry;
    if (!r || r->size != sizeof(*r)) return nioLibErrParam;
    entry = SysLibTblEntry(refNum);
    if (!entry || !entry->globalsP) return nioLibErrNotOpen;
    return globals_test(refNum, delta, r);
}

static Err globals_test(UInt16 refNum, UInt32 delta, NioLibGlobalsResult *r)
{
    (void)refNum;
    r->openCount = nl_opens;
    r->initialized = nl_initialized;
    r->independent = nl_independent;
    r->before = nl_counter;
    nl_counter += delta;
    r->after = nl_counter;
    r->pointedValue = *nl_data_pointer;
    r->functionValue = nl_function_pointer(nl_independent);
    r->dataPointerOK = nl_data_pointer == &nl_initialized;
    r->distinctOK = &nl_initialized != &nl_independent;
    return errNone;
}
