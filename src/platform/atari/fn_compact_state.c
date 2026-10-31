#include "fn_internal.h"
#include <stddef.h>
#include "fn_raw.h"

/* Assembly layout invariants fail the build if a future C ABI changes. */
typedef char compact_session_layout[(FN_MAX_SESSIONS == 2 && sizeof(fn_session_t) == 14) ? 1 : -1];
/* `available` was appended after the fields the assembly addresses (status at 24); the C-only parse code uses it. */
typedef char compact_parse_layout[(sizeof(fn_parse_ctx_t) == 29 && offsetof(fn_parse_ctx_t, status) == 24 && offsetof(fn_parse_ctx_t, available) == 27) ? 1 : -1];
typedef char compact_packet_capacity[(FN_MAX_PACKET_SIZE == 512 && FN_MAX_URL_LEN == 128) ? 1 : -1];

#define ASSERT_OFFSET(type, member, byte) \
    typedef char compact_##type##_##member[(offsetof(type, member) == byte) ? 1 : -1]
ASSERT_OFFSET(fn_session_t, active, 0);
ASSERT_OFFSET(fn_session_t, proto_flags, 1);
ASSERT_OFFSET(fn_session_t, needs_body, 2);
ASSERT_OFFSET(fn_session_t, handle, 4);
ASSERT_OFFSET(fn_session_t, write_offset, 6);
ASSERT_OFFSET(fn_session_t, read_offset, 10);
ASSERT_OFFSET(fn_parse_ctx_t, response, 0);
ASSERT_OFFSET(fn_parse_ctx_t, resp_len, 4);
ASSERT_OFFSET(fn_parse_ctx_t, data_offset, 8);
ASSERT_OFFSET(fn_parse_ctx_t, data_len, 10);
ASSERT_OFFSET(fn_parse_ctx_t, status, 24);
ASSERT_OFFSET(fn_parse_ctx_t, flags, 25);
ASSERT_OFFSET(fn_parse_ctx_t, proto_flags, 26);
ASSERT_OFFSET(fn_parse_ctx_t, available, 27);
ASSERT_OFFSET(fn_parse_ctx_t, handle, 12);
ASSERT_OFFSET(fn_transport_ctx_t, request, 0);
ASSERT_OFFSET(fn_transport_ctx_t, response, 2);
ASSERT_OFFSET(fn_transport_ctx_t, req_len, 4);
ASSERT_OFFSET(fn_transport_ctx_t, resp_max, 6);
ASSERT_OFFSET(fn_transport_ctx_t, resp_len, 8);
ASSERT_OFFSET(fn_raw_response_t, status, 0);
ASSERT_OFFSET(fn_raw_response_t, payload_length, 1);
ASSERT_OFFSET(fn_write_diagnostics_t, exchanges, 0);
ASSERT_OFFSET(fn_write_diagnostics_t, transport_retry, 2);
ASSERT_OFFSET(fn_write_diagnostics_t, service_retry, 4);
ASSERT_OFFSET(fn_write_diagnostics_t, zero_accepted, 6);
ASSERT_OFFSET(fn_write_diagnostics_t, response_length, 8);
ASSERT_OFFSET(fn_write_diagnostics_t, response_device, 10);
ASSERT_OFFSET(fn_write_diagnostics_t, response_command, 11);
typedef char compact_write_diagnostics_size[(sizeof(fn_write_diagnostics_t) == 12) ? 1 : -1];
typedef char compact_transport_size[(sizeof(fn_transport_ctx_t) == 10) ? 1 : -1];
typedef char compact_raw_size[(sizeof(fn_raw_response_t) == 3) ? 1 : -1];

fn_session_t _fn_sessions[FN_MAX_SESSIONS];
uint8_t _fn_initialized = 0;
fn_transport_ctx_t _fn_transport_ctx;
fn_parse_ctx_t _fn_parse_ctx;

