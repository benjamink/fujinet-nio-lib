#include "NioLibClient.h"

Err NioTestGuardedGlobals(UInt16 refNum, UInt32 delta,
                         NioLibGlobalsResult *resultP, UInt16 *registersOK);

/* Ordinary app A5 globals, independent of the library's allocation. */
static volatile UInt32 app_first = 0xfeed1234UL;
static volatile UInt32 app_second = 0xbeef5678UL;
static UInt16 ref, opens;
static Boolean loaded, failed;
static UInt16 checks;
static Err lastError;
static const Char *stage;
static UInt16 net_result, net_palm, net_http, net_bytes, net_host, net_module;
static UInt32 net_time;
static Boolean net_failed;
static const Char *net_stage = "Tap Probe or Run";
static void draw_results(void);

/* Public Internet is deliberately tested separately from module presence.
 * A local HTTP fixture can be selected at build time without changing NIO. */
#ifndef NIO_TEST_URL
#define NIO_TEST_URL "http://example.com/"
#endif

static Boolean net_check(uint8_t result, const Char *name)
{
    if (result) {
        net_failed = true; net_stage = name; net_result = result;
        net_palm = NioLibClientLastError();
        return false;
    }
    return true;
}

/* Developer UI uses only the public 1.2 traps. It opens library globals,
 * never BeginClient or a serial port. No separate preference mechanism. */
static NioLibConfig transport_config;
static NioLibTransportStatus transport_status;
static UInt16 config_flags;
static Err config_error;
static UInt16 config_ref;
static Boolean config_open, config_loaded;

static void config_close(void)
{
    UInt16 remaining = 1;
    if (config_open) {
        if (!NioLibClose(config_ref, &remaining)) config_open = false;
    }
    if (!config_open && config_loaded && !remaining) {
        if (!SysLibRemove(config_ref)) config_loaded = false;
    }
}
static void config_read(void)
{
    NioLibVersion version;
    if (!config_open) {
        config_error = SysLibFind(nioLibName, &config_ref);
        if (config_error) {
            config_error = SysLibLoad(nioLibType, nioLibCreator, &config_ref);
            if (config_error) return;
            config_loaded = true;
        }
        version.size = sizeof(version);
        config_error = NioLibGetVersion(config_ref, &version);
        if (!config_error && (version.abiMajor != 1 || version.abiMinor < 2))
            config_error = nioLibErrParam;
        if (config_error) {
            if (config_loaded) { SysLibRemove(config_ref); config_loaded = false; }
            return;
        }
        config_error = NioLibOpen(config_ref);
        if (config_error) {
            if (config_loaded && !SysLibTblEntry(config_ref)->globalsP) {
                SysLibRemove(config_ref); config_loaded = false;
            }
            return;
        }
        config_open = true;
    }
    transport_config.size = sizeof(transport_config);
    config_error = NioLibGetConfig(config_ref, &transport_config, &config_flags);
    transport_status.size = sizeof(transport_status);
    if (!config_error) config_error = NioLibGetTransportStatus(config_ref, &transport_status);
}
static void config_action(UInt16 id)
{
    if (!config_open) return;
    if (id == 1012) transport_config.transport = nioTransportSpringboard;
    if (id == 1013) transport_config.transport = nioTransportSerial;
    if (id == 1014) {
        switch (transport_config.serialBaud) {
        case 9600: transport_config.serialBaud = 19200; break;
        case 19200: transport_config.serialBaud = 38400; break;
        case 38400: transport_config.serialBaud = 57600; break;
        case 57600: transport_config.serialBaud = 115200; break;
        default: transport_config.serialBaud = 9600;
        }
    }
    if (id == 1017) transport_config.serialOptions ^= nioSerialRTSCTS;
    if (id == 1016) {
        MemSet(&transport_config, sizeof(transport_config), 0);
        transport_config.size = sizeof(transport_config); transport_config.version = nioConfigVersion;
        transport_config.transport = nioTransportSpringboard; transport_config.serialBaud = 115200;
    }
    if (id == 1015 || id == 1016) {
        config_error = NioLibSetConfig(config_ref, &transport_config);
        if (!config_error) config_read();
    }
}

static void probe_network(void)
{
    NioLibAvailability a;
    a.size = sizeof(a);
    net_failed = false; net_result = net_palm = net_bytes = net_http = 0;
    net_module = net_host = 0; net_time = 0;
    if (net_check(NioLibClientProbe(&a), "Module unavailable")) {
        net_module = a.modulePresent; net_host = a.hostPresent;
        net_stage = a.hostPresent ? "Module + host present" : "Module; host unavailable";
    }
}

static uint8_t wait_info(fn_handle_t handle, UInt16 *http)
{
    UInt32 length, start = TimGetTicks();
    uint8_t flags, result;
    do {
        result = fn_info(handle, http, &length, &flags);
        if (result != FN_ERR_NOT_READY && result != FN_ERR_BUSY) return result;
        SysTaskDelay(5);
    } while (TimGetTicks() - start < 20UL * SysTicksPerSecond());
    return FN_ERR_TIMEOUT;
}

static void run_network(void)
{
    NioLibAvailability a;
    NioLibCleanup c;
    FN_TIME_T time;
    fn_handle_t first = 0, second = 0;
    UInt16 i, got;
    uint8_t flags, result;
    UInt8 body[96];
    net_failed = false; net_result = net_palm = net_http = net_bytes = 0;
    net_module = net_host = 0;
    net_time = 0; a.size = sizeof(a);
    net_stage = "Running; please wait";
    draw_results();
    if (!net_check(NioLibClientProbe(&a), "Module unavailable")) goto done;
    net_module = a.modulePresent; net_host = a.hostPresent;
    if (!net_check(fn_init(), "Begin NIO client")) goto done;
    /* Repeated init must not acquire another library/client reference. */
    if (!net_check(fn_init(), "Repeated init")) goto done;
    if (!net_check(fn_clock_get(&time), "Clock.GetTime")) goto done;
    net_time = (UInt32)time;
    if (!net_check(fn_open(&first, FN_METHOD_GET, NIO_TEST_URL, FN_OPEN_FOLLOW_REDIR),
                   "HTTP Open first")) goto done;
    if (!net_check(fn_open(&second, FN_METHOD_GET, NIO_TEST_URL, FN_OPEN_FOLLOW_REDIR),
                   "HTTP Open second")) goto done;
    if (!first || !second || first == second) {
        net_check(FN_ERR_INTERNAL, "Distinct handles"); goto done;
    }
    for (i = 0; i < 2; ++i) {
        result = wait_info(i ? second : first, &net_http);
        if (!net_check(result, "HTTP Info")) goto done;
        if (!net_check(net_http == 200 ? FN_OK : FN_ERR_IO, "HTTP status != 200")) goto done;
        got = 0;
        result = fn_read(i ? second : first, 0, body, sizeof(body), &got, &flags);
        if (!net_check(result, "HTTP Read")) goto done;
        if (!net_check(got ? FN_OK : FN_ERR_IO, "HTTP body empty")) goto done;
        net_bytes += got;
    }
    if (!net_check(fn_close(first), "Explicit Close")) goto done;
    first = 0;
    /* Leave second to EndClient: this is a real cleanup regression test. */
done:
    fn_shutdown();
    NioLibClientCleanupReport(&c);
    if (!net_failed) {
        if (NioLibClientLastError() || NioLibClientLastResult() ||
            c.attempted != 1 || c.confirmed != 1 || c.uncertain || c.ambiguousOperation) {
            net_check(FN_ERR_IO, "End / cleanup uncertain");
        } else net_stage = "Clock + two HTTP PASS";
    }
}

static Boolean check(Boolean condition, const Char *name, Err err)
{
    if (!condition) { failed = true; stage = name; lastError = err; return false; }
    ++checks;
    return true;
}

static Boolean globals(UInt32 delta, UInt32 before, UInt16 count)
{
    NioLibGlobalsResult r;
    UInt16 registersOK = 0;
    Err err;
    r.size = sizeof(r);
    err = NioTestGuardedGlobals(ref, delta, &r, &registersOK);
    if (!check(!err, "Globals trap", err)) return false;
    return check(r.initialized == 0x13579bdfUL, "Initialized integer", 0) &&
        check(r.independent == 0x2468ace0UL, "Independent integer", 0) &&
        check(r.before == before, "BSS / prior state", 0) &&
        check(r.after == before + delta, "Counter mutation", 0) &&
        check(r.openCount == count, "Open reference count", r.openCount) &&
        check(r.pointedValue == r.initialized && r.dataPointerOK, "Data pointer relocation", 0) &&
        check(r.functionValue == (r.independent ^ 0xa55a5aa5UL), "Code pointer relocation", 0) &&
        check(r.distinctOK, "Distinct globals", 0) &&
        check(registersOK, "Caller A4 / A5", 0) &&
        check(app_first == 0xfeed1234UL && app_second == 0xbeef5678UL, "Caller app globals", 0);
}

static Boolean open_library(void)
{
    Err err = NioLibOpen(ref);
    if (err) return check(false, "Open / allocation", err);
    ++opens;
    return true;
}

static Boolean close_library(void)
{
    UInt16 remaining = 0xffff;
    Err err = NioLibClose(ref, &remaining);
    if (err) return check(false, "Close", err);
    --opens;
    return check(remaining == opens, "Close refcount", remaining);
}

static void run_tests(void)
{
    Err err;
    UInt32 caps, rom;
    NioLibVersion v;
    SysLibTblEntryPtr entry;
    checks = 0; failed = false; loaded = false; opens = 0; lastError = 0;
    stage = "Starting";
    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &rom);
    if (!check(rom >= sysMakeROMVersion(3, 5, 0, sysROMStageRelease, 0),
               "Needs Palm OS 3.5", 0)) return;
    err = SysLibFind(nioLibName, &ref);
    if (err) {
        err = SysLibLoad(nioLibType, nioLibCreator, &ref);
        if (!check(!err, "Install NioLib.prc", err)) return;
        loaded = true;
    } else {
        /* Do not mutate counters or unload another client's registered library. */
        check(false, "NioLib already in use", 0);
        return;
    }
    entry = SysLibTblEntry(ref);
    if (!check(entry && entry->dispatchTblP && !entry->globalsP,
               "Library table", 0)) goto cleanup;
    v.size = sizeof(v);
    err = NioLibGetVersion(ref, &v);
    if (!check(!err && v.abiMajor == nioLibAbiMajor && v.abiMinor >= nioLibAbiMinor,
               "Incompatible ABI", err)) goto cleanup;
    err = NioLibGetCapabilities(ref, &caps);
    if (!check(!err && (caps & nioLibCapA4Globals), "Capabilities", err)) goto cleanup;
    if (!open_library() || !open_library()) goto cleanup;
    if (!globals(3, 0, 2) || !globals(5, 3, 2)) goto cleanup;
    if (!close_library() || !globals(0, 8, 1)) goto cleanup;
    err = NioLibSleep(ref);
    if (!err) err = NioLibWake(ref);
    if (!check(!err, "Sleep / Wake", err) || !globals(0, 8, 1)) goto cleanup;
    if (!close_library()) goto cleanup;
    if (!check(!SysLibTblEntry(ref)->globalsP, "Globals freed", 0)) goto cleanup;
    if (!open_library() || !globals(1, 0, 1) || !close_library()) goto cleanup;
    err = SysLibRemove(ref);
    if (!check(!err, "Remove", err)) goto cleanup;
    loaded = false;
    err = SysLibFind(nioLibName, &ref);
    if (!check(err != errNone, "Removed from table", 0)) return;
    err = SysLibLoad(nioLibType, nioLibCreator, &ref);
    if (!check(!err, "Reload", err)) return;
    loaded = true;
    if (!open_library() || !globals(0, 0, 1) || !close_library()) goto cleanup;
cleanup:
    /* Unwind only our successfully opened references; never remove when a
     * failed Close or an unexpected remaining reference makes safety unclear. */
    while (opens) {
        if (!close_library()) break;
    }
    if (loaded && !opens && !SysLibTblEntry(ref)->globalsP) {
        err = SysLibRemove(ref);
        if (!err) loaded = false;
        check(!err, "Final removal", err);
    }
    if (!failed) stage = "All lifecycle checks";
}

static void line(const Char *text, Coord y)
{
    WinDrawChars(text, StrLen(text), 4, y);
}

static void draw_results(void)
{
    Char text[48];
    RectangleType area = {{2, 17}, {156, 119}};
    WinEraseRectangle(&area, 0);
    if (FrmGetActiveFormID() == 1010) {
        line("Transport settings (ABI 1.2)", 19);
        line(transport_config.transport == nioTransportSerial ? "Edit: Serial cradle" : transport_config.transport == nioTransportUSB ? "Edit: USB cradle" : "Edit: Springboard CS1", 31);
        StrPrintF(text, "Baud %lu  Flow %s", transport_config.serialBaud,
            transport_config.serialOptions ? "RTS/CTS" : "None"); line(text, 43);
        StrPrintF(text, "Saved %u  Active %u", transport_status.selected, transport_status.active); line(text, 55);
        StrPrintF(text, "Err %04X Pref %u Flags %X", config_error, config_flags, transport_status.flags); line(text, 67);
        line(transport_status.flags & nioStatusUSBUnsupported ? "USB: unsupported" : "USB: supported (NIO Config)", 79);
        line("Availability unknown until open", 91);
        return;
    }
    if (FrmGetActiveFormID() == 1002) {
        line(net_failed ? "FAIL - networking" : "NIO library ABI 1.2", 20);
        line(net_stage, 35);
        StrPrintF(text, "FN %02X  Palm %04X", net_result, net_palm); line(text, 50);
        StrPrintF(text, "Module %u  Host %u", net_module, net_host); line(text, 65);
        StrPrintF(text, "UTC epoch %lu", net_time); line(text, 80);
        StrPrintF(text, "HTTP %u  Bytes %u", net_http, net_bytes); line(text, 95);
        line("Run: clock + two GETs", 110);
        line("Exit always ends client", 125);
        return;
    }
    line(failed ? "FAIL" : "PASS - ABI 1.0", 20);
    line(stage, 35);
    StrPrintF(text, "Checks %u   Err %04X", checks, lastError);
    line(text, 50);
    if (!failed) {
        line("Init 13579BDF / 2468ACE0", 65);
        line("BSS 0 -> 3 -> 8; reset 0", 80);
        line("Data + code pointers PASS", 95);
        line("Caller A4/A5 + globals PASS", 110);
        line("Close / unload / reload PASS", 125);
    }
}

static Boolean handle(EventType *e)
{
    if (e->eType == frmOpenEvent) {
        FrmDrawForm(FrmGetActiveForm());
        if (FrmGetActiveFormID() == 1000) run_tests();
        if (FrmGetActiveFormID() == 1010) config_read();
        draw_results(); return true;
    }
    if (e->eType == ctlSelectEvent && e->data.ctlSelect.controlID == 1001) {
        run_tests(); draw_results(); return true;
    }
    if (e->eType == ctlSelectEvent) {
        if (e->data.ctlSelect.controlID == 1011) { config_close(); FrmGotoForm(1000); return true; }
        if (e->data.ctlSelect.controlID == 1007) { fn_shutdown(); FrmGotoForm(1010); return true; }
        if (e->data.ctlSelect.controlID >= 1012 && e->data.ctlSelect.controlID <= 1017) {
            config_action(e->data.ctlSelect.controlID); draw_results(); return true;
        }
        if (e->data.ctlSelect.controlID == 1006) { FrmGotoForm(1002); return true; }
        if (e->data.ctlSelect.controlID == 1005) { fn_shutdown(); FrmGotoForm(1000); return true; }
        if (e->data.ctlSelect.controlID == 1003) { probe_network(); draw_results(); return true; }
        if (e->data.ctlSelect.controlID == 1004) { run_network(); draw_results(); return true; }
    }
    return false;
}

UInt32 PilotMain(UInt16 cmd, NioPalmLaunchPtr pbp, UInt16 flags)
{
    EventType event;
    FormType *form;
    (void)pbp; (void)flags;
    if (cmd != sysAppLaunchCmdNormalLaunch) return 0;
    FrmGotoForm(1000);
    do {
        EvtGetEvent(&event, evtWaitForever);
        if (SysHandleEvent(&event)) continue;
        if (event.eType == frmLoadEvent) {
            form = FrmInitForm(event.data.frmLoad.formID);
            FrmSetActiveForm(form); FrmSetEventHandler(form, handle);
        } else FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
    fn_shutdown();
    config_close();
    FrmCloseAllForms();
    return 0;
}
