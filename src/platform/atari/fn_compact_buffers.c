#include "fn_internal.h"

/* Default compact workspaces. Kept in their own archive member so a client
 * can supply both buffers and place them in application-owned memory. They
 * must remain distinct: recovery may receive an old reply while a new request
 * is already present. See fn_atari_buffers.h. */
uint8_t _fn_req_buf[FN_MAX_PACKET_SIZE];
uint8_t _fn_resp_buf[FN_MAX_PACKET_SIZE];
