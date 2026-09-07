#include "fn_session.h"

#include "fujinet-nio.h"
#include "fn_protocol.h"
#include "fn_slip.h"

#ifndef FN_SESSION_WRITE_TIMEOUT
#define FN_SESSION_WRITE_TIMEOUT 1000
#endif

#ifndef FN_SESSION_READ_POLL_TIMEOUT
#define FN_SESSION_READ_POLL_TIMEOUT 10
#endif

static uint8_t write_frame(fn_stream_session_t *session,
                           const uint8_t *request, uint16_t request_length)
{
    uint16_t encoded_length;
    uint16_t i;
    uint8_t value;
    uint8_t result;

    /* A bulk-capable channel is still a byte stream, not a packet-native
     * transport: the common session performs exactly the same SLIP encoding
     * and submits the resulting ordered bytes in one physical operation. */
    if (session->ops->write_bytes != NULL) {
        encoded_length = fn_slip_encode(request, request_length,
                                        session->wire_buffer);
        return session->ops->write_bytes(session->context, session->wire_buffer,
                                         encoded_length,
                                         FN_SESSION_WRITE_TIMEOUT);
    }

    result = session->ops->write_byte(session->context, SLIP_END,
                                      FN_SESSION_WRITE_TIMEOUT);
    if (result != FN_OK) return result;

    for (i = 0; i < request_length; ++i) {
        value = request[i];
        if (value == SLIP_END) {
            result = session->ops->write_byte(session->context, SLIP_ESCAPE,
                                              FN_SESSION_WRITE_TIMEOUT);
            if (result != FN_OK) return result;
            value = SLIP_ESC_END;
        } else if (value == SLIP_ESCAPE) {
            result = session->ops->write_byte(session->context, SLIP_ESCAPE,
                                              FN_SESSION_WRITE_TIMEOUT);
            if (result != FN_OK) return result;
            value = SLIP_ESC_ESC;
        }
        result = session->ops->write_byte(session->context, value,
                                          FN_SESSION_WRITE_TIMEOUT);
        if (result != FN_OK) return result;
    }

    return session->ops->write_byte(session->context, SLIP_END,
                                    FN_SESSION_WRITE_TIMEOUT);
}

static uint8_t read_frame(fn_stream_session_t *session, uint16_t timeout_ms,
                          uint16_t *frame_length)
{
    uint16_t elapsed = 0;
    uint16_t length = 0;
    uint8_t value;

    while (elapsed < timeout_ms) {
        if (!session->ops->read_byte(session->context, &value,
                                     FN_SESSION_READ_POLL_TIMEOUT)) {
            if ((uint32_t)elapsed + FN_SESSION_READ_POLL_TIMEOUT > timeout_ms) {
                elapsed = timeout_ms;
            } else {
                elapsed = (uint16_t)(elapsed + FN_SESSION_READ_POLL_TIMEOUT);
            }
            continue;
        }

        /*
         * Do not skip bytes before the first END, and do not insert one.
         * A frame still closes only on a real SLIP_END (0xC0) with length
         * >= 2. Payload 0xC0 is escaped as DB DC, so it cannot end the
         * frame. fn_slip_decode already treats a leading END as optional.
         * Hunt-for-END recovered line noise but, if the opening END was
         * missing, skipped the payload and waited forever after the
         * trailing END (PiStorm 38400 first file-list).
         */
        if (length >= session->wire_capacity) return FN_ERR_IO;
        session->wire_buffer[length++] = value;
        if (length >= 2 && value == SLIP_END) {
            *frame_length = length;
            return FN_OK;
        }
    }
    return FN_ERR_TIMEOUT;
}

uint8_t fn_stream_session_init(fn_stream_session_t *session,
                               const fn_stream_channel_ops_t *ops,
                               void *context,
                               uint8_t *wire_buffer,
                               uint16_t wire_capacity)
{
    if (!session || !ops || !ops->open || !ops->close ||
        !ops->write_byte || !ops->read_byte || !ops->flush ||
        !wire_buffer || wire_capacity < FN_HEADER_SIZE) {
        return FN_ERR_INVALID;
    }
    session->ops = ops;
    session->context = context;
    session->wire_buffer = wire_buffer;
    session->wire_capacity = wire_capacity;
    session->capabilities.max_packet_size =
        (uint16_t)(wire_capacity > 2 ? (wire_capacity - 2) / 2 : 0);
    session->capabilities.max_payload_size =
        session->capabilities.max_packet_size > FN_HEADER_SIZE
            ? (uint16_t)(session->capabilities.max_packet_size - FN_HEADER_SIZE)
            : 0;
    session->capabilities.packet_native = 0;
    session->capabilities.max_outstanding = 1;
    session->opened = 0;
    session->busy = 0;
    return FN_OK;
}

uint8_t fn_stream_session_open(fn_stream_session_t *session)
{
    uint8_t result;
    if (!session || !session->ops) return FN_ERR_INVALID;
    if (session->opened) return FN_OK;
    result = session->ops->open(session->context);
    if (result == FN_OK) session->opened = 1;
    return result;
}

void fn_stream_session_close(fn_stream_session_t *session)
{
    if (!session || !session->opened) return;
    session->ops->close(session->context);
    session->opened = 0;
    session->busy = 0;
}

uint8_t fn_stream_session_flush(fn_stream_session_t *session)
{
    if (!session || !session->opened) return FN_ERR_NOT_READY;
    session->ops->flush(session->context);
    return FN_OK;
}

const fn_session_capabilities_t *fn_stream_session_capabilities(
    const fn_stream_session_t *session)
{
    return session ? &session->capabilities : 0;
}

uint8_t fn_stream_session_request(fn_stream_session_t *session,
                                   const uint8_t *request,
                                   uint16_t request_length,
                                   uint8_t *response,
                                   uint16_t response_capacity,
                                   uint16_t *response_length,
                                   uint16_t timeout_ms)
{
    uint16_t raw_length;
    uint16_t decoded_length;
    uint8_t result;

    if (!session || !session->opened || !request || !request_length ||
        !response || !response_capacity || !response_length) {
        return FN_ERR_INVALID;
    }
    if (session->busy) return FN_ERR_BUSY;
    if (request_length > session->capabilities.max_packet_size) {
        return FN_ERR_INVALID;
    }

    session->busy = 1;
    result = fn_stream_session_flush(session);
    if (result == FN_OK) result = write_frame(session, request, request_length);
    if (result == FN_OK) {
        result = read_frame(session, timeout_ms, &raw_length);
        if (result == FN_OK) {
            decoded_length = fn_slip_decode(session->wire_buffer, raw_length,
                                             response);
            if (!decoded_length) {
                result = FN_ERR_IO;
            } else if (decoded_length > response_capacity) {
                result = FN_ERR_IO;
            } else {
                *response_length = decoded_length;
            }
        }
    }
    session->busy = 0;
    return result;
}
