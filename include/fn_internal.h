/**
 * @file fn_internal.h
 * @brief Internal function declarations for fujinet-nio-lib
 * 
 * These functions are used internally by the library and should not
 * be called directly by applications.
 * 
 * @version 1.0.0
 */

#ifndef FN_INTERNAL_H
#define FN_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _CMOC_VERSION_
    #include <cmoc.h>
#else
    #include <stdint.h>
#endif

#include "fujinet-nio.h"
#include "fn_protocol.h"

/* Shared library state */
extern fn_session_t _fn_sessions[FN_MAX_SESSIONS];
extern uint8_t _fn_initialized;
extern uint8_t _fn_req_buf[FN_MAX_PACKET_SIZE];
extern uint8_t _fn_resp_buf[FN_MAX_PACKET_SIZE];
extern char _fn_tcp_url[FN_MAX_URL_LEN];

typedef struct {
    const uint8_t *request;
    uint8_t *response;
    uint16_t req_len;
    uint16_t resp_max;
    uint16_t resp_len;
} fn_transport_ctx_t;

typedef struct {
    const uint8_t *response;
    uint8_t *data;
    uint16_t resp_len;
    uint16_t data_max;
    uint16_t data_offset;
    uint16_t data_len;
    fn_handle_t handle;
    uint32_t offset_echo;
    uint16_t http_status;
    uint32_t content_length;
    uint8_t status;
    uint8_t flags;
    uint8_t proto_flags;
} fn_parse_ctx_t;

extern fn_transport_ctx_t _fn_transport_ctx;
extern fn_parse_ctx_t _fn_parse_ctx;

uint16_t fn_checksum_fold(const uint8_t *data, uint16_t len,
                          uint16_t checksum);

int8_t fn_find_free_slot(void);

/**
 * Send the Open request already built in _fn_req_buf (req_len bytes), parse
 * the reply and record the session slot. Shared by fn_open() and
 * fn_open_translated().
 */
uint8_t fn_open_exchange(fn_handle_t *handle, uint16_t req_len);

/**
 * Validate fn_open()-style arguments (initialised, non-NULL handle and url,
 * URL length) and map the public FN_OPEN_* flags to the wire flags. Returns
 * FN_OK and stores the wire flags, or an FN_ERR_* code. Shared by fn_open()
 * and fn_open_translated().
 */
uint8_t fn_open_prepare(const fn_handle_t *handle,
                        const char *url,
                        uint8_t flags,
                        uint8_t *wire_flags);
int8_t fn_find_session(fn_handle_t handle);
void fn_free_handle(fn_handle_t handle);

#define FN_READ_LE16(buf, offset) \
    ((uint16_t)(buf)[(offset)] | ((uint16_t)(buf)[(offset) + 1] << 8))

#define FN_READ_LE32(buf, offset) \
    ((uint32_t)(buf)[(offset)] | \
     ((uint32_t)(buf)[(offset) + 1] << 8) | \
     ((uint32_t)(buf)[(offset) + 2] << 16) | \
     ((uint32_t)(buf)[(offset) + 3] << 24))

#define FN_GET_LE16(p) \
    ((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8))

#define FN_GET_LE32(p) \
    ((uint32_t)(p)[0] | \
     ((uint32_t)(p)[1] << 8) | \
     ((uint32_t)(p)[2] << 16) | \
     ((uint32_t)(p)[3] << 24))

#define FN_PUT_LE16(p, value) \
    do { \
        uint16_t _fn_le_value = (uint16_t)(value); \
        (p)[0] = (uint8_t)(_fn_le_value & 0xFFu); \
        (p)[1] = (uint8_t)(_fn_le_value >> 8); \
    } while (0)

#define FN_PUT_LE32(p, value) \
    do { \
        uint32_t _fn_le_value = (uint32_t)(value); \
        (p)[0] = (uint8_t)(_fn_le_value & 0xFFu); \
        (p)[1] = (uint8_t)(_fn_le_value >> 8); \
        (p)[2] = (uint8_t)(_fn_le_value >> 16); \
        (p)[3] = (uint8_t)(_fn_le_value >> 24); \
    } while (0)

#include "fn_slip.h"

/* ============================================================================
 * Packet Building Functions
 * ============================================================================ */

/**
 * Build an Open request packet.
 */
uint16_t fn_build_open_packet(uint8_t *buffer,
                               uint8_t method,
                               uint8_t flags,
                               const char *url);

/**
 * Build an Open request packet, optionally with the content-translation
 * extension block (appended only when ttype != FN_TRANSLATE_NONE).
 * Returns 0 on overflow.
 */
uint16_t fn_build_open_packet_ext(uint8_t *buffer,
                                  uint8_t method,
                                  uint8_t flags,
                                  const char *url,
                                  uint8_t ttype,
                                  uint8_t tflags,
                                  const char *selector);

/**
 * Build a Read request packet.
 */
uint16_t fn_build_read_packet(uint8_t *buffer,
                               fn_handle_t handle,
                               uint32_t offset_val,
                               uint16_t max_bytes);

/**
 * Build a Write request packet.
 */
uint16_t fn_build_write_packet(uint8_t *buffer,
                                fn_handle_t handle,
                                uint32_t offset_val,
                                const uint8_t *data,
                                uint16_t data_len);

/**
 * Build a Close request packet.
 */
uint16_t fn_build_close_packet(uint8_t *buffer, fn_handle_t handle);

/**
 * Build an Info request packet.
 */
uint16_t fn_build_info_packet(uint8_t *buffer, fn_handle_t handle);

/* ============================================================================
 * Response Parsing Functions
 * ============================================================================ */

/**
 * Parse a response packet header.
 */
uint8_t fn_parse_response_header(void);

/**
 * Parse an Open response.
 */
uint8_t fn_parse_open_response(void);

/**
 * Parse a Read response.
 */
uint8_t fn_parse_read_response(void);

/**
 * Parse an Info response.
 */
uint8_t fn_parse_info_response(void);

uint8_t fn_clock_exchange(uint8_t command,
                          const uint8_t *payload,
                          uint16_t payload_len,
                          uint8_t *status,
                          uint16_t *data_offset,
                          uint16_t *data_len);
uint8_t fn_clock_response_byte(uint16_t offset);
uint8_t fn_clock_copy_format_response(uint8_t *time_data,
                                      uint16_t data_offset,
                                      uint16_t data_len);

#ifdef __cplusplus
}
#endif

#endif /* FN_INTERNAL_H */
