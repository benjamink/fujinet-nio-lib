#ifndef FN_FILE_INTERNAL_H
#define FN_FILE_INTERNAL_H
#include <string.h>
#include "fujinet-nio.h"
#include "fn_internal.h"
#include "fn_raw.h"
uint16_t fn_file_prefix(fn_file_io_t *io, const char *uri, uint16_t tail);
uint8_t fn_file_call(fn_file_io_t *io, uint8_t command, uint16_t length,
                     uint16_t minimum, uint16_t *received);
#endif
