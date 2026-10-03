#include <string.h>

#include "fn_internal.h"

/* Builds the legacy Open packet, then appends the translation extension block
 * (u32 openExtFlags = 1, u8 type, u8 flags, u16 selectorLen, selector), fixes
 * the total-length header field and recomputes the checksum. Kept separate so
 * programs that never ask for translation don't link it. */
uint16_t fn_build_open_packet_ext(uint8_t *buffer,
                                  uint8_t method,
                                  uint8_t flags,
                                  const char *url,
                                  uint8_t ttype,
                                  uint8_t tflags,
                                  const char *selector)
{
    uint16_t offset;
    uint16_t sel_len;
    uint16_t total_len;

    offset = fn_build_open_packet(buffer, method, flags, url);
    if (offset == 0 || ttype == FN_TRANSLATE_NONE) {
        return offset;
    }

    sel_len = 0;
    if (selector != NULL) {
        while (selector[sel_len] != '\0') {
            ++sel_len;
            if (sel_len > FN_MAX_PACKET_SIZE) {
                return 0;
            }
        }
    }

    total_len = offset + 4 + 1 + 1 + 2 + sel_len;
    if (total_len > FN_MAX_PACKET_SIZE) {
        return 0;
    }

    buffer[offset++] = 0x01;
    buffer[offset++] = 0;
    buffer[offset++] = 0;
    buffer[offset++] = 0;
    buffer[offset++] = ttype;
    buffer[offset++] = tflags;
    buffer[offset++] = (uint8_t)(sel_len & 0xFF);
    buffer[offset++] = (uint8_t)(sel_len >> 8);
    if (sel_len) {
        memcpy(buffer + offset, selector, sel_len);
        offset += sel_len;
    }

    buffer[2] = (uint8_t)(total_len & 0xFF);
    buffer[3] = (uint8_t)(total_len >> 8);
    buffer[FN_CHECKSUM_OFFSET] = fn_calc_packet_checksum(buffer, offset);
    return offset;
}
