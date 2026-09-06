/*
 * fn_transport.c - AmigaOS Transport Implementation
 *
 * Broker client for fujinet-nio.device. Does not open serial.device or
 * timer.device; SLIP framing stays in the broker backend.
 *
 * Requires: exec.library, fujinet-nio.device
 * Compiler: m68k-amigaos-gcc (amiga-gcc)
 *
 * Kickstart 1.3: CreatePort/DeletePort (amiga.lib), not CreateMsgPort.
 */

#include <exec/errors.h>
#include <exec/io.h>
#include <exec/nodes.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>

#include <string.h>

#ifndef FN_AMIGA_EXPLICIT_LIFECYCLE
#include <stdlib.h>
#endif

#include "fujinet-nio.h"
#include "fn_platform.h"
#include "fn_internal.h"
#include "fujinet_nio_device.h"

struct fn_amiga_transport {
    struct MsgPort *port;
    struct FujiNetNIORequest req;
    uint8_t device_open;
};

static struct fn_amiga_transport g_transport;
static uint8_t last_broker_stage;
static uint8_t last_broker_result;
static uint8_t last_broker_cause;
static uint8_t last_native_error;
static uint16_t last_native_status;

void fn_amiga_transport_last_broker_detail(uint8_t *stage, uint8_t *result)
{
    if (stage != NULL) *stage = last_broker_stage;
    if (result != NULL) *result = last_broker_result;
}

void fn_amiga_transport_last_broker_cause(uint8_t *cause)
{
    if (cause != NULL) *cause = last_broker_cause;
}

void fn_amiga_transport_last_serial_detail(uint8_t *native_error,
                                            uint16_t *status)
{
    if (native_error != NULL) *native_error = last_native_error;
    if (status != NULL) *status = last_native_status;
}

void fn_transport_close(void);

static uint8_t map_native_io_error(BYTE io_error)
{
    if (io_error == IOERR_ABORTED)
        return FN_ERR_ABORTED;
    if (io_error == IOERR_NOCMD ||
        io_error == IOERR_BADLENGTH ||
        io_error == IOERR_BADADDRESS)
        return FN_ERR_INVALID;
    return FN_ERR_IO;
}

static void init_request_for_open(void)
{
    memset(&g_transport.req, 0, sizeof(g_transport.req));
    g_transport.req.fn_io.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    g_transport.req.fn_io.io_Message.mn_ReplyPort = g_transport.port;
    g_transport.req.fn_io.io_Message.mn_Length =
        (UWORD)sizeof(struct FujiNetNIORequest);
}

static void prepare_exchange(struct FujiNetNIORequest *req,
                             const uint8_t *request,
                             uint16_t request_length,
                             uint8_t *response,
                             uint16_t response_capacity)
{
    req->fn_io.io_Command = FUJINET_NIO_CMD_EXCHANGE;
    req->fn_io.io_Flags = 0;
    req->fn_io.io_Error = 0;
    req->fn_struct_size = (UWORD)FUJINET_NIO_REQUEST_SIZE;
    req->fn_flags = 0;
    req->fn_pad[0] = 0;
    req->fn_pad[1] = 0;
    req->fn_pad[2] = 0;
    req->fn_request_data = request;
    req->fn_request_length = request_length;
    req->fn_response_data = response;
    req->fn_response_capacity = response_capacity;
    req->fn_response_length = 0;
    req->fn_nio_error = 0;
}

uint8_t fn_transport_init(void)
{
    LONG od_ret;

    if (g_transport.device_open)
        return FN_OK;

    /* CreatePort/DeletePort are amiga.lib and Kickstart 1.3-safe.
     * CreateMsgPort requires exec V36. */
    g_transport.port = CreatePort(NULL, 0);
    if (g_transport.port == NULL)
        return FN_ERR_IO;

    init_request_for_open();

    od_ret = OpenDevice((CONST_STRPTR)FUJINET_NIO_DEVICE_NAME,
                        FUJINET_NIO_DEVICE_UNIT,
                        &g_transport.req.fn_io, 0);
    if (od_ret != 0) {
        DeletePort(g_transport.port);
        g_transport.port = NULL;
        return FN_ERR_NOT_FOUND;
    }

    g_transport.device_open = 1;
#ifndef FN_AMIGA_EXPLICIT_LIFECYCLE
    atexit(fn_transport_close);
#endif
    return FN_OK;
}

uint8_t fn_transport_ready(void)
{
    return g_transport.device_open ? 1 : 0;
}

uint8_t fn_transport_exchange(void)
{
    return fn_transport_exchange_buffers(_fn_transport_ctx.request,
                                         _fn_transport_ctx.req_len,
                                         _fn_transport_ctx.response,
                                         _fn_transport_ctx.resp_max,
                                         &_fn_transport_ctx.resp_len);
}

uint8_t fn_transport_exchange_buffers(const uint8_t *request,
                                      uint16_t request_length,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length)
{
    struct MsgPort *port;
    struct FujiNetNIORequest req;
    uint8_t result;

    if (response_length != NULL)
        *response_length = 0;
    if (response_length == NULL)
        return FN_ERR_INVALID;
    if (!g_transport.device_open)
        return FN_ERR_NOT_FOUND;
    if (request == NULL || response == NULL)
        return FN_ERR_INVALID;

    /* A MsgPort is bound to the task that creates it.  The original global
     * port worked only while every fn_* call came from the initial task: a
     * second task could send the request but its DoIO() waited for a reply
     * signal delivered to the original task.  Give every synchronous
     * exchange a caller-owned port and IORequest instead.  The opened device
     * stays global; copying its device/unit binding is the normal Exec way
     * to submit another request to that open device. */
    port = CreatePort(NULL, 0);
    if (port == NULL)
        return FN_ERR_IO;

    memset(&req, 0, sizeof(req));
    req.fn_io.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    req.fn_io.io_Message.mn_ReplyPort = port;
    req.fn_io.io_Message.mn_Length = (UWORD)sizeof(req);
    req.fn_io.io_Device = g_transport.req.fn_io.io_Device;
    req.fn_io.io_Unit = g_transport.req.fn_io.io_Unit;
    prepare_exchange(&req, request, request_length, response, response_capacity);
    (void)DoIO(&req.fn_io);
    last_broker_stage = req.fn_pad[0];
    last_broker_result = req.fn_pad[1];
    last_broker_cause = req.fn_pad[2];
    last_native_error = (uint8_t)(req.fn_flags & 0xFF);
    last_native_status = (uint16_t)(req.fn_flags >> 8);

    if (req.fn_io.io_Error != 0) {
        result = map_native_io_error(req.fn_io.io_Error);
        DeletePort(port);
        *response_length = 0;
        return result;
    }

    *response_length = req.fn_response_length;
    result = req.fn_nio_error;
    DeletePort(port);
    return result;
}

void fn_transport_close(void)
{
    if (!g_transport.device_open)
        return;

    CloseDevice(&g_transport.req.fn_io);
    DeletePort(g_transport.port);
    g_transport.port = NULL;
    g_transport.device_open = 0;
}

const char *fn_platform_name(void)
{
    return "amiga";
}
