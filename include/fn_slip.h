/**
 * @file fn_slip.h
 * @brief SLIP encode/decode declarations shared by the library and broker.
 */

#ifndef FN_SLIP_H
#define FN_SLIP_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _CMOC_VERSION_
    #include <cmoc.h>
#else
    #include <stdint.h>
#endif

/**
 * Encode data with SLIP framing.
 */
uint16_t fn_slip_encode(const uint8_t *input, uint16_t in_len, uint8_t *output);

/**
 * Decode SLIP-framed data into at most out_max bytes; 0 if it does not fit.
 */
uint16_t fn_slip_decode(const uint8_t *input, uint16_t in_len, uint8_t *output,
                        uint16_t out_max);

#ifdef __cplusplus
}
#endif

#endif /* FN_SLIP_H */
