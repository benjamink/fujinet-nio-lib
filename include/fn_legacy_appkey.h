/**
 * @file fn_legacy_appkey.h
 * @brief Compatibility wrappers for legacy FujiNet appkey APIs.
 *
 * These functions keep the old fujinet-lib appkey API shape while storing keys
 * through fujinet-nio's FileDevice. They are implemented in separate archive
 * members so normal applications do not link them unless they call them.
 */

#ifndef FN_LEGACY_APPKEY_H
#define FN_LEGACY_APPKEY_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _CMOC_VERSION_
    #include <cmoc.h>
    #ifndef bool
    #define bool unsigned char
    #endif
    #ifndef true
    #define true 1
    #endif
    #ifndef false
    #define false 0
    #endif
#else
    #include <stdint.h>
    #if !defined(__cplusplus)
        #if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
            #include <stdbool.h>
        #else
            #ifndef bool
            #define bool unsigned char
            #endif
            #ifndef true
            #define true 1
            #endif
            #ifndef false
            #define false 0
            #endif
        #endif
    #endif
#endif

#ifndef FN_FUJI_H
/* Matches the legacy fujinet-lib AppKeySize values. */
enum AppKeySize {
    DEFAULT = 0,
    SIZE_256 = 1
};
#endif

/**
 * Set the legacy appkey namespace used by read/write calls.
 *
 * Legacy appkeys are stored as:
 *   persist:///FujiNet/<creator-id><app-id><key-id>.key
 *
 * Example:
 *   creator 0xfe0c, app 0x01, key 0x01 ->
 *   persist:///FujiNet/fe0c0101.key
 */
void fuji_set_appkey_details(uint16_t creator_id,
                             uint8_t app_id,
                             enum AppKeySize keysize);

/** Read a legacy appkey value. Returns true on success. */
bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data);

/** Read using caller-owned scratch. The scratch must not overlap data and must
 * hold at least 74 bytes for DEFAULT, or 266 for SIZE_256. `data` must hold
 * the selected key size. The existing namespace and wire format are unchanged. */
bool fuji_read_appkey_buffered(uint8_t key_id, uint16_t *count, uint8_t *data,
                               uint8_t *scratch, uint16_t scratch_size);

/** Write a legacy appkey value. Returns true on success. */
bool fuji_write_appkey(uint8_t key_id, uint16_t count, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* FN_LEGACY_APPKEY_H */
