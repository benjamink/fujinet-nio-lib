#include <exec/errors.h>
#include <exec/io.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <exec/types.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>

#include "fujinet_nio_device.h"
#include "fn_platform.h"
#include "fujinet-nio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures;

#define CHECK(name, expression) do {                                      \
    if (!(expression)) {                                                  \
        fprintf(stderr, "FAIL: %s (line %d)\n", name, __LINE__);          \
        ++failures;                                                       \
    }                                                                     \
} while (0)

static int g_open_fail;
static int g_port_fail;
static BYTE g_doio_error;
static uint8_t g_doio_nio_error;
static uint16_t g_doio_resp_len;
static unsigned g_open_count;
static unsigned g_close_count;
static unsigned g_doio_count;
static unsigned g_abort_count;
static unsigned g_wait_count;
static unsigned g_create_port_count;
static unsigned g_delete_port_count;
static char g_open_name[64];
static ULONG g_open_unit;
static struct FujiNetNIORequest g_open_snapshot;
static struct FujiNetNIORequest g_doio_snapshot;
static struct Device *g_dummy_device = (struct Device *)(uintptr_t)0x1;

struct MsgPort *CreatePort(CONST_STRPTR name, LONG pri)
{
    struct MsgPort *port;

    (void)name;
    (void)pri;
    ++g_create_port_count;
    if (g_port_fail)
        return NULL;
    port = (struct MsgPort *)calloc(1, sizeof(*port));
    return port;
}

void DeletePort(struct MsgPort *port)
{
    ++g_delete_port_count;
    free(port);
}

LONG OpenDevice(CONST_STRPTR name, ULONG unit, struct IORequest *ioRequest,
                ULONG flags)
{
    (void)flags;
    ++g_open_count;
    g_open_unit = unit;
    g_open_name[0] = '\0';
    if (name != NULL) {
        strncpy(g_open_name, name, sizeof(g_open_name) - 1);
        g_open_name[sizeof(g_open_name) - 1] = '\0';
    }
    if (ioRequest != NULL)
        memcpy(&g_open_snapshot, ioRequest, sizeof(g_open_snapshot));
    if (g_open_fail) {
        if (ioRequest != NULL)
            ioRequest->io_Error = IOERR_OPENFAIL;
        return IOERR_OPENFAIL;
    }
    if (ioRequest != NULL)
        ioRequest->io_Device = g_dummy_device;
    return 0;
}

void CloseDevice(struct IORequest *ioRequest)
{
    (void)ioRequest;
    ++g_close_count;
}

BYTE DoIO(struct IORequest *ioRequest)
{
    struct FujiNetNIORequest *req = (struct FujiNetNIORequest *)ioRequest;

    ++g_doio_count;
    if (req == NULL)
        return IOERR_BADADDRESS;
    memcpy(&g_doio_snapshot, req, sizeof(g_doio_snapshot));
    req->fn_io.io_Error = g_doio_error;
    req->fn_nio_error = g_doio_nio_error;
    req->fn_response_length = g_doio_resp_len;
    return g_doio_error;
}

void AbortIO(struct IORequest *ioRequest)
{
    (void)ioRequest;
    ++g_abort_count;
}

void WaitIO(struct IORequest *ioRequest)
{
    (void)ioRequest;
    ++g_wait_count;
}

static unsigned g_sendio_count;
static unsigned g_wait_call_count;
static ULONG g_wait_script[8];
static unsigned g_wait_script_len;
static ULONG g_last_wait_mask;
static ULONG g_last_wait_result;

void SendIO(struct IORequest *ioRequest)
{
    struct FujiNetNIORequest *req = (struct FujiNetNIORequest *)ioRequest;

    ++g_sendio_count;
    req->fn_io.io_Error = g_doio_error;
    req->fn_nio_error = g_doio_nio_error;
    req->fn_response_length = g_doio_resp_len;
}

/* Done once the most recent Wait() reported the reply port bit (bit 0). */
struct IORequest *CheckIO(struct IORequest *ioRequest)
{
    return (g_last_wait_result & 1u) ? ioRequest : NULL;
}

ULONG Wait(ULONG signalSet)
{
    g_last_wait_mask = signalSet;
    g_last_wait_result = g_wait_call_count < g_wait_script_len
                             ? g_wait_script[g_wait_call_count] : 1u;
    ++g_wait_call_count;
    return g_last_wait_result;
}

static void reset_stubs(void)
{
    fn_amiga_set_wait_hook(0, NULL, NULL);
    g_sendio_count = 0;
    g_wait_call_count = 0;
    g_wait_script_len = 0;
    g_last_wait_mask = 0;
    g_last_wait_result = 0;
    fn_transport_close();
    g_open_fail = 0;
    g_port_fail = 0;
    g_doio_error = 0;
    g_doio_nio_error = FN_OK;
    g_doio_resp_len = 0;
    g_open_count = 0;
    g_close_count = 0;
    g_doio_count = 0;
    g_abort_count = 0;
    g_wait_count = 0;
    g_create_port_count = 0;
    g_delete_port_count = 0;
    g_open_name[0] = '\0';
    g_open_unit = 0xFFFFFFFFu;
    memset(&g_open_snapshot, 0xA5, sizeof(g_open_snapshot));
    memset(&g_doio_snapshot, 0xA5, sizeof(g_doio_snapshot));
}

static void test_init_opens_broker(void)
{
    reset_stubs();
    CHECK("init ok", fn_transport_init() == FN_OK);
    CHECK("ready", fn_transport_ready() == 1);
    CHECK("opened once", g_open_count == 1);
    CHECK("name is broker",
          strcmp(g_open_name, FUJINET_NIO_DEVICE_NAME) == 0);
    CHECK("not serial.device", strcmp(g_open_name, "serial.device") != 0);
    CHECK("unit 0", g_open_unit == FUJINET_NIO_DEVICE_UNIT);
    CHECK("zeroed then NT_MESSAGE",
          g_open_snapshot.fn_io.io_Message.mn_Node.ln_Type == NT_MESSAGE);
    CHECK("reply port set",
          g_open_snapshot.fn_io.io_Message.mn_ReplyPort != NULL);
    CHECK("mn_Length",
          g_open_snapshot.fn_io.io_Message.mn_Length ==
              sizeof(struct FujiNetNIORequest));
}

static void test_port_alloc_fail(void)
{
    reset_stubs();
    g_port_fail = 1;
    CHECK("init io on port fail", fn_transport_init() == FN_ERR_IO);
    CHECK("not ready", fn_transport_ready() == 0);
    CHECK("no OpenDevice", g_open_count == 0);
}

static void test_broker_absent(void)
{
    reset_stubs();
    g_open_fail = 1;
    CHECK("init not found", fn_transport_init() == FN_ERR_NOT_FOUND);
    CHECK("not ready", fn_transport_ready() == 0);
    CHECK("opened broker name",
          strcmp(g_open_name, FUJINET_NIO_DEVICE_NAME) == 0);
    CHECK("port deleted", g_delete_port_count == 1);
    CHECK("no CloseDevice", g_close_count == 0);
}

static void test_reinit_no_second_open(void)
{
    reset_stubs();
    CHECK("first init", fn_transport_init() == FN_OK);
    g_open_count = 0;
    CHECK("re-init", fn_transport_init() == FN_OK);
    CHECK("no second OpenDevice", g_open_count == 0);
}

static void test_happy_exchange(void)
{
    uint8_t req[4] = {1, 2, 3, 4};
    uint8_t resp[8];
    uint16_t resp_len = 99;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    g_doio_error = 0;
    g_doio_nio_error = FN_OK;
    g_doio_resp_len = 5;
    CHECK("exchange ok",
          fn_transport_exchange_buffers(req, sizeof(req), resp, sizeof(resp),
                                        &resp_len) == FN_OK);
    CHECK("resp_len", resp_len == 5);
    CHECK("exchange has caller port", g_create_port_count == 2);
    CHECK("exchange port released", g_delete_port_count == 1);
    CHECK("exchange reply port is not opener port",
          g_doio_snapshot.fn_io.io_Message.mn_ReplyPort !=
              g_open_snapshot.fn_io.io_Message.mn_ReplyPort);
    CHECK("exchange retains opened device binding",
          g_doio_snapshot.fn_io.io_Device == g_dummy_device);
}

static void test_exchange_port_alloc_fail(void)
{
    uint8_t req[1] = {0};
    uint8_t resp[8];
    uint16_t resp_len = 99;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    g_port_fail = 1;
    CHECK("exchange port failure",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_IO);
    CHECK("exchange failure clears length", resp_len == 0);
    CHECK("exchange failure does not submit", g_doio_count == 0);
}

static void test_stale_fn_on_native_fail(void)
{
    uint8_t req[1] = {0};
    uint8_t resp[8];
    uint16_t resp_len = 7;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    g_doio_error = 0;
    g_doio_nio_error = FN_ERR_TIMEOUT;
    g_doio_resp_len = 3;
    CHECK("prior nio leftover",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_TIMEOUT);

    g_doio_error = IOERR_ABORTED;
    g_doio_nio_error = FN_ERR_TIMEOUT;
    g_doio_resp_len = 9;
    resp_len = 7;
    CHECK("mapped abort",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_ABORTED);
    CHECK("resp_len cleared", resp_len == 0);
}

static void test_beginio_reject_class(BYTE native, const char *name)
{
    uint8_t req[1] = {0};
    uint8_t resp[8];
    uint16_t resp_len = 4;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    g_doio_error = native;
    g_doio_nio_error = FN_ERR_TIMEOUT;
    g_doio_resp_len = 2;
    CHECK(name,
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_INVALID);
    CHECK("resp_len 0", resp_len == 0);
}

static void test_exchange_field_reset(void)
{
    uint8_t req[2] = {9, 8};
    uint8_t resp[8];
    uint16_t resp_len = 0;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);

    g_doio_error = IOERR_UNITBUSY;
    g_doio_nio_error = FN_ERR_TIMEOUT;
    g_doio_resp_len = 11;
    CHECK("poison",
          fn_transport_exchange_buffers(req, sizeof(req), resp, sizeof(resp),
                                        &resp_len) == FN_ERR_IO);

    g_doio_error = 0;
    g_doio_nio_error = FN_OK;
    g_doio_resp_len = 4;
    CHECK("second exchange",
          fn_transport_exchange_buffers(req, sizeof(req), resp, sizeof(resp),
                                        &resp_len) == FN_OK);
    CHECK("command reset",
          g_doio_snapshot.fn_io.io_Command == FUJINET_NIO_CMD_EXCHANGE);
    CHECK("struct size",
          g_doio_snapshot.fn_struct_size == FUJINET_NIO_REQUEST_SIZE);
    CHECK("flags 0", g_doio_snapshot.fn_flags == 0);
    CHECK("pad0", g_doio_snapshot.fn_pad[0] == 0);
    CHECK("pad1", g_doio_snapshot.fn_pad[1] == 0);
    CHECK("pad2", g_doio_snapshot.fn_pad[2] == 0);
    CHECK("req ptr", g_doio_snapshot.fn_request_data == req);
    CHECK("req len", g_doio_snapshot.fn_request_length == sizeof(req));
    CHECK("resp ptr", g_doio_snapshot.fn_response_data == resp);
    CHECK("resp cap", g_doio_snapshot.fn_response_capacity == sizeof(resp));
    CHECK("length cleared before DoIO",
          g_doio_snapshot.fn_response_length == 0);
    CHECK("io_Error cleared", g_doio_snapshot.fn_io.io_Error == 0);
    CHECK("io_Flags cleared", g_doio_snapshot.fn_io.io_Flags == 0);
    CHECK("fn_nio_error cleared", g_doio_snapshot.fn_nio_error == 0);
}

static void test_close_no_abort(void)
{
    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    fn_transport_close();
    CHECK("CloseDevice", g_close_count == 1);
    CHECK("DeletePort", g_delete_port_count == 1);
    CHECK("not ready", fn_transport_ready() == 0);
    CHECK("no AbortIO", g_abort_count == 0);
    CHECK("no WaitIO", g_wait_count == 0);
}

static void test_reopen_after_close(void)
{
    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    fn_transport_close();
    g_open_count = 0;
    CHECK("re-open", fn_transport_init() == FN_OK);
    CHECK("second OpenDevice", g_open_count == 1);
    CHECK("ready again", fn_transport_ready() == 1);
}

static void test_not_open_clears_length(void)
{
    uint8_t req[1] = {0};
    uint8_t resp[8];
    uint16_t resp_len = 99;

    reset_stubs();
    CHECK("not found",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_NOT_FOUND);
    CHECK("resp_len 0", resp_len == 0);
    CHECK("no DoIO", g_doio_count == 0);
}

static void test_invalid_buffers_clears_length(void)
{
    uint8_t req[1] = {0};
    uint8_t resp[8];
    uint16_t resp_len;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);

    resp_len = 99;
    CHECK("null request",
          fn_transport_exchange_buffers(NULL, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_INVALID);
    CHECK("null request len", resp_len == 0);

    resp_len = 99;
    CHECK("null response",
          fn_transport_exchange_buffers(req, 1, NULL, sizeof(resp),
                                        &resp_len) == FN_ERR_INVALID);
    CHECK("null response len", resp_len == 0);
    CHECK("no DoIO", g_doio_count == 0);
}

static void test_other_io_error(void)
{
    uint8_t req[1] = {0};
    uint8_t resp[8];
    uint16_t resp_len = 3;

    reset_stubs();
    CHECK("init", fn_transport_init() == FN_OK);
    g_doio_error = IOERR_UNITBUSY;
    g_doio_nio_error = FN_ERR_TIMEOUT;
    CHECK("mapped IO",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp),
                                        &resp_len) == FN_ERR_IO);
    CHECK("resp_len 0", resp_len == 0);
}

#define HOOK_SIG 0x100u
static unsigned g_hook_calls;
static int g_hook_answer;
static void *g_hook_ctx_seen;

static int test_hook(void *ctx)
{
    ++g_hook_calls;
    g_hook_ctx_seen = ctx;
    return g_hook_answer;
}

static void hook_setup(void)
{
    reset_stubs();
    g_hook_calls = 0;
    g_hook_answer = 0;
    g_hook_ctx_seen = NULL;
    CHECK("init", fn_transport_init() == FN_OK);
    g_doio_resp_len = 5;
}

static void test_hook_unset_uses_doio(void)
{
    uint8_t req[1] = {0}, resp[8];
    uint16_t len = 99;

    hook_setup();
    CHECK("exchange ok",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp), &len) == FN_OK);
    CHECK("doio once", g_doio_count == 1);
    CHECK("no SendIO", g_sendio_count == 0);
    fn_amiga_set_wait_hook(HOOK_SIG, test_hook, NULL);
    fn_amiga_set_wait_hook(0, test_hook, NULL);
    g_doio_count = 0;
    CHECK("signals 0 ok",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp), &len) == FN_OK);
    CHECK("signals 0 uses DoIO", g_doio_count == 1 && g_sendio_count == 0);
}

static void test_hook_reply_first(void)
{
    uint8_t req[1] = {0}, resp[8];
    uint16_t len = 99;
    int ctx;

    hook_setup();
    fn_amiga_set_wait_hook(HOOK_SIG, test_hook, &ctx);
    g_wait_script[0] = 1u;
    g_wait_script_len = 1;
    CHECK("ok",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp), &len) == FN_OK);
    CHECK("len", len == 5);
    CHECK("hook never called", g_hook_calls == 0);
    CHECK("no abort", g_abort_count == 0);
    CHECK("no DoIO", g_doio_count == 0);
    CHECK("SendIO once", g_sendio_count == 1);
    CHECK("waited on port and hook signals",
          (g_last_wait_mask & HOOK_SIG) != 0 && (g_last_wait_mask & 1u) != 0);
    CHECK("WaitIO once", g_wait_count == 1);
    CHECK("port released", g_delete_port_count == 1);
}

static void test_hook_keeps_waiting(void)
{
    uint8_t req[1] = {0}, resp[8];
    uint16_t len = 0;
    int ctx;

    hook_setup();
    fn_amiga_set_wait_hook(HOOK_SIG, test_hook, &ctx);
    g_wait_script[0] = HOOK_SIG;
    g_wait_script[1] = HOOK_SIG;
    g_wait_script[2] = 1u;
    g_wait_script_len = 3;
    CHECK("ok",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp), &len) == FN_OK);
    CHECK("len", len == 5);
    CHECK("hook called twice", g_hook_calls == 2);
    CHECK("ctx passed", g_hook_ctx_seen == &ctx);
    CHECK("no abort", g_abort_count == 0);
}

static void test_hook_aborts(void)
{
    uint8_t req[1] = {0}, resp[8];
    uint16_t len = 99;

    hook_setup();
    fn_amiga_set_wait_hook(HOOK_SIG, test_hook, NULL);
    g_hook_answer = 1;
    g_wait_script[0] = HOOK_SIG;
    g_wait_script_len = 1;
    CHECK("aborted",
          fn_transport_exchange_buffers(req, 1, resp, sizeof(resp), &len) ==
              FN_ERR_ABORTED);
    CHECK("len 0", len == 0);
    CHECK("abort once", g_abort_count == 1);
    CHECK("WaitIO once", g_wait_count == 1);
    CHECK("hook once", g_hook_calls == 1);
    CHECK("port released", g_delete_port_count == 1);
}

int main(void)
{
    test_hook_unset_uses_doio();
    test_hook_reply_first();
    test_hook_keeps_waiting();
    test_hook_aborts();
    test_init_opens_broker();
    test_port_alloc_fail();
    test_broker_absent();
    test_reinit_no_second_open();
    test_happy_exchange();
    test_exchange_port_alloc_fail();
    test_stale_fn_on_native_fail();
    test_beginio_reject_class(IOERR_NOCMD, "NOCMD");
    test_beginio_reject_class(IOERR_BADLENGTH, "BADLENGTH");
    test_beginio_reject_class(IOERR_BADADDRESS, "BADADDRESS");
    test_exchange_field_reset();
    test_close_no_abort();
    test_reopen_after_close();
    test_not_open_clears_length();
    test_invalid_buffers_clears_length();
    test_other_io_error();

    if (failures != 0) {
        fprintf(stderr, "%u checks failed\n", failures);
        return 1;
    }
    return 0;
}
