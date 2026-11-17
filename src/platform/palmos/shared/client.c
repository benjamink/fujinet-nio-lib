#include "NioLibClient.h"

/* These are CALLER A5 globals, not library state. No packet codecs, service
 * implementation or physical I/O is linked into this archive. */
UInt16 nl_client_ref, nl_client_active;
static UInt16 client_open, client_loaded, capacity;
UInt32 nl_client_token;
static UInt16 client_status_api, client_json_api;
static UInt16 palm_error;
UInt16 nl_client_result_code;
static UInt16 client_services_api;
static NioLibCleanup cleanup_report;

uint8_t nl_client_translate(Err err)
{
    palm_error = err;
    if (!err) return (uint8_t)nl_client_result_code;
    if (err == nioLibErrBusy) return FN_ERR_BUSY;
    if (err == nioLibErrOwner || err == nioLibErrNotOpen) return FN_ERR_NOT_READY;
    if (err == nioLibErrParam) return FN_ERR_INVALID;
    return FN_ERR_IO;
}

static void release_library(void)
{
    UInt16 remaining = 0xffff;
    Err err;
    if (client_open) {
        err = NioLibClose(nl_client_ref, &remaining);
        if (err) { palm_error = err; return; }
        client_open = 0;
    } else remaining = 0;
    if (client_loaded && !remaining) {
        err = SysLibRemove(nl_client_ref);
        if (err) { palm_error = err; return; }
    }
    client_loaded = 0;
}

static uint8_t load_library(void)
{
    NioLibVersion version;
    UInt32 caps = 0;
    Err err;
    palm_error = 0;
    if (client_open) return FN_OK;
    err = SysLibFind(nioLibName, &nl_client_ref);
    if (err) {
        err = SysLibLoad(nioLibType, nioLibCreator, &nl_client_ref);
        if (err) { palm_error = err; return FN_ERR_NOT_FOUND; }
        client_loaded = 1;
    }
    version.size = sizeof(version);
    err = NioLibGetVersion(nl_client_ref, &version);
    if (!err) err = NioLibGetCapabilities(nl_client_ref, &caps);
    if (err || version.abiMajor != 1 || version.abiMinor < 1 ||
        (caps & (nioLibCapNetwork | nioLibCapClock | nioLibCapClient)) !=
         (nioLibCapNetwork | nioLibCapClock | nioLibCapClient)) {
        palm_error = err;
        release_library();
        return FN_ERR_UNSUPPORTED;
    }
    client_status_api = version.abiMinor >= 2 && (caps & nioLibCapTransports);
    client_services_api = version.abiMinor >= 6 && (caps & nioLibCapServices);
    client_json_api = version.abiMinor >= 4 && (caps & nioLibCapJSONQuery);
    err = NioLibOpen(nl_client_ref);
    if (err) { palm_error = err; release_library(); return FN_ERR_IO; }
    client_open = 1;
    return FN_OK;
}

uint8_t fn_init(void)
{
    uint8_t result;
    Err err;
    if (nl_client_active) return FN_OK; /* no extra Open/Begin reference */
    result = load_library(); if (result) return result;
    err = NioLibBeginClient(nl_client_ref, &nl_client_token, &capacity, &nl_client_result_code);
    result = nl_client_translate(err);
    if (!result) nl_client_active = 1;
    else release_library();
    return result;
}

void fn_shutdown(void)
{
    Err err;
    if (nl_client_active) {
        cleanup_report.size = sizeof(cleanup_report);
        err = NioLibEndClient(nl_client_ref, nl_client_token, &cleanup_report, &nl_client_result_code);
        nl_client_translate(err);
        if (err) return; /* never Close/remove an owner that failed to end */
        nl_client_active = 0; nl_client_token = 0;
    }
    release_library();
}

uint8_t NioLibClientProbe(NioLibAvailability *a)
{
    UInt16 was_open = client_open;
    uint8_t result;
    result = load_library(); if (result) return result;
    result = nl_client_translate(NioLibProbe(nl_client_ref, a, &nl_client_result_code));
    if (!was_open && !nl_client_active) release_library();
    return result;
}

uint8_t fn_is_ready(void)
{
    NioLibAvailability a;
    NioLibTransportStatus s;
    if (!nl_client_active) return 0;
    if (client_status_api) {
        s.size = sizeof(s);
        return NioLibGetTransportStatus(nl_client_ref, &s) == errNone &&
            s.result == FN_OK && s.active && (s.flags & nioStatusAvailable);
    }
    /* ABI 1.1 only supports Springboard and has no TransportStatus trap. */
    a.size = sizeof(a);
    return NioLibClientProbe(&a) == FN_OK && a.modulePresent;
}

uint8_t fn_open_translated(fn_handle_t *handle, uint8_t method, const char *url,
                           uint8_t flags, uint8_t type, uint8_t tflags, const char *selector)
{
    NioLibOpenArgs a;
    if (!nl_client_active) return FN_ERR_NOT_READY;
    a.size = sizeof(a); a.method = method; a.flags = flags;
    a.translationType = type; a.translationFlags = tflags;
    a.translated = 1; a.url = url; a.selector = selector;
    return nl_client_translate(NioLibNetOpen(nl_client_ref, nl_client_token, &a, handle, &nl_client_result_code));
}

uint8_t fn_open(fn_handle_t *handle, uint8_t method, const char *url, uint8_t flags)
{
    NioLibOpenArgs a;
    if (!nl_client_active) return FN_ERR_NOT_READY;
    a.size = sizeof(a); a.method = method; a.flags = flags;
    a.translationType = a.translationFlags = a.translated = 0;
    a.url = url; a.selector = NULL;
    return nl_client_translate(NioLibNetOpen(nl_client_ref, nl_client_token, &a, handle, &nl_client_result_code));
}

uint8_t fn_info(fn_handle_t handle, uint16_t *http, uint32_t *length, uint8_t *flags)
{
    UInt16 h = 0, f = 0;
    UInt32 n = 0;
    uint8_t result;
    if (!nl_client_active) return FN_ERR_NOT_READY;
    result = nl_client_translate(NioLibNetInfo(nl_client_ref, nl_client_token, handle, &h, &n, &f, &nl_client_result_code));
    if (http) *http = h;
    if (length) *length = n;
    if (flags) *flags = (uint8_t)f;
    return result;
}

uint8_t fn_read(fn_handle_t handle, uint32_t offset, uint8_t *buffer, uint16_t max,
                uint16_t *readP, uint8_t *flags)
{
    UInt16 f = 0;
    uint8_t result;
    if (!nl_client_active) return FN_ERR_NOT_READY;
    result = nl_client_translate(NioLibNetRead(nl_client_ref, nl_client_token, handle, offset, buffer, max,
                                    readP, &f, &nl_client_result_code));
    if (flags) *flags = (uint8_t)f;
    return result;
}

uint8_t fn_write(fn_handle_t handle, uint32_t offset, const uint8_t *buffer,
                 uint16_t length, uint16_t *written)
{
    if (!nl_client_active) return FN_ERR_NOT_READY;
    return nl_client_translate(NioLibNetWrite(nl_client_ref, nl_client_token, handle, offset, buffer,
                                    length, written, &nl_client_result_code));
}

uint8_t fn_close(fn_handle_t handle)
{
    if (!nl_client_active) return FN_ERR_NOT_READY;
    return nl_client_translate(NioLibNetClose(nl_client_ref, nl_client_token, handle, &nl_client_result_code));
}

uint8_t fn_ready(fn_ready_entry_t *entries, uint8_t max, uint8_t *count)
{
    NioLibReadyEntry *temporary;
    UInt16 n = 0, i, size = max < capacity ? max : capacity;
    uint8_t result;
    if (!nl_client_active) return FN_ERR_NOT_READY;
    if (!count || (!entries && max) || size > 255) return FN_ERR_INVALID;
    *count = 0;
    temporary = size ? MemPtrNew((UInt32)size * sizeof(*temporary)) : NULL;
    if (size && !temporary) return FN_ERR_IO;
    result = nl_client_translate(NioLibNetReady(nl_client_ref, nl_client_token, temporary, size, &n, &nl_client_result_code));
    if (!result) for (i = 0; i < n; ++i) {
        entries[i].handle = temporary[i].handle;
        entries[i].flags = (uint8_t)temporary[i].flags;
        entries[i].available = temporary[i].available;
    }
    if (temporary) MemPtrFree(temporary);
    *count = (uint8_t)n;
    return result;
}

uint8_t fn_clock_get(FN_TIME_T *time)
{
    UInt8 bytes[8];
    UInt16 i;
    uint8_t result;
    if (!nl_client_active) return FN_ERR_NOT_READY;
    if (!time) return FN_ERR_INVALID;
    result = nl_client_translate(NioLibClockGet(nl_client_ref, nl_client_token, bytes, &nl_client_result_code));
    if (!result) {
        *time = 0;
        for (i = 0; i < 8; ++i) *time |= ((FN_TIME_T)bytes[i]) << (8 * i);
    }
    return result;
}

uint8_t fn_clock_get_timezone(char *zone)
{
    if (!nl_client_active) return FN_ERR_NOT_READY;
    return nl_client_translate(NioLibClockTimezone(nl_client_ref, nl_client_token, zone,
                                         FN_MAX_TIMEZONE_LEN, &nl_client_result_code));
}

UInt16 NioLibClientLastError(void) { return palm_error; }
UInt16 NioLibClientLastResult(void) { return nl_client_result_code; }
UInt16 NioLibClientCapacity(void) { return capacity; }
void NioLibClientCleanupReport(NioLibCleanup *p) { if (p) *p = cleanup_report; }

uint8_t fn_json_query(fn_handle_t handle, const char *selector)
{
    if (!nl_client_active) return FN_ERR_NOT_READY;
    if (!client_json_api) return FN_ERR_UNSUPPORTED;
    return nl_client_translate(NioLibNetJsonQuery(nl_client_ref, nl_client_token, handle, selector, &nl_client_result_code));
}

uint8_t nl_client_extended_ready(void)
{
    if (!nl_client_active) return FN_ERR_NOT_READY;
    return client_services_api ? FN_OK : FN_ERR_UNSUPPORTED;
}
