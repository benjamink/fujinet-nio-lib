#ifndef FN_WIFI_BBC_H
#define FN_WIFI_BBC_H
#include "fujinet-nio.h"
uint8_t fn_bbc_wifi_call(uint8_t command, const uint8_t *request, uint16_t request_len,
    uint8_t *reply, uint16_t reply_capacity, uint16_t *reply_len);
uint8_t fn_bbc_wifi_string(const uint8_t *p, uint16_t len, uint16_t *at, char *out, uint8_t cap);
#endif
