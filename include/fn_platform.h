/**
 * @file fn_platform.h
 * @brief Platform Transport Interface
 * 
 * Each platform must implement these functions to provide low-level
 * communication with the FujiNet device. The platform layer handles
 * the physical transport (SIO, SmartPort, Drivewire, etc.).
 * 
 * @version 1.0.0
 */

#ifndef FN_PLATFORM_H
#define FN_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _CMOC_VERSION_
    #include <cmoc.h>
#else
    #include <stdint.h>
#endif

/* ============================================================================
 * Platform Transport Interface
 * ============================================================================ */

/**
 * @brief Initialize the platform transport.
 * 
 * Called once during fn_init(). Should set up hardware, check for
 * device presence, and prepare for communication.
 * 
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_transport_init(void);

/** Close a platform transport opened by fn_transport_init(). */
void fn_transport_close(void);

/**
 * @brief Check if transport is ready for communication.
 * 
 * @return 1 if ready, 0 if not ready
 */
uint8_t fn_transport_ready(void);

/**
 * @brief Send a request and receive a response.
 * 
 * This is the core transport function. It:
 * 1. SLIP-encodes the request packet
 * 2. Sends it to the FujiNet device
 * 3. Waits for and receives the response
 * 4. SLIP-decodes the response
 * 
 * The function blocks until a response is received or timeout occurs.
 * 
 * @param request      Request packet buffer (FujiBus format, not SLIP-encoded)
 * @param req_len      Length of request packet
 * @param response     Buffer to receive response packet
 * @param resp_max     Maximum response buffer size
 * @param resp_len     Pointer to receive actual response length
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_transport_exchange(void);

/* Explicit-buffer transport entry point used by resident Amiga clients. */
uint8_t fn_transport_exchange_buffers(const uint8_t *request,
                                      uint16_t request_length,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length);

/**
 * @brief Get the platform name string.
 * 
 * @return Platform name (e.g., "atari", "apple2", "coco")
 */
const char *fn_platform_name(void);

/* ============================================================================
 * Platform-Specific Configuration
 * ============================================================================ */

/* These macros can be overridden by platform-specific headers */

/** Default timeout for transport operations (milliseconds) */
#ifndef FN_TRANSPORT_TIMEOUT
#define FN_TRANSPORT_TIMEOUT  5000
#endif

/** Maximum retries for transport operations */
#ifndef FN_TRANSPORT_RETRIES
#define FN_TRANSPORT_RETRIES  3
#endif

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

/* CC65 targets */
#ifdef __ATARI__
    #define FN_PLATFORM_ATARI    1
    #define FN_PLATFORM_NAME     "atari"
#endif

#ifdef __BBC__
    #define FN_PLATFORM_BBC      1
    #define FN_PLATFORM_NAME     "bbc"
#endif

/* Apple II targets - check ENH first since it also defines __APPLE2__ */
#ifdef __APPLE2ENH__
    #define FN_PLATFORM_APPLE2ENH 1
    #define FN_PLATFORM_NAME     "apple2enh"
#elif defined(__APPLE2__)
    #define FN_PLATFORM_APPLE2   1
    #define FN_PLATFORM_NAME     "apple2"
#endif

#ifdef __CBM__
    #define FN_PLATFORM_CBM      1
    #define FN_PLATFORM_NAME     "cbm"
#endif

/* CMOC targets */
#ifdef _CMOC_VERSION_
    #define FN_PLATFORM_COCO     1
    #define FN_PLATFORM_NAME     "coco"
#endif

/* Watcom targets */
#ifdef __WATCOMC__
    #define FN_PLATFORM_MSDOS    1
    #define FN_PLATFORM_NAME     "msdos"
#endif

/* AmigaOS targets (amiga-gcc defines __amigaos__) */
#ifdef __amigaos__
    #define FN_PLATFORM_AMIGA    1
    #define FN_PLATFORM_NAME     "amiga"
#endif

/* Palm OS targets (prc-tools m68k-palmos-gcc defines __palmos__) */
#ifdef __palmos__
    #define FN_PLATFORM_PALMOS   1
    #define FN_PLATFORM_NAME     "palmos"
#endif

/* Default if not detected */
#ifndef FN_PLATFORM_NAME
    #define FN_PLATFORM_NAME     "unknown"
#endif

#ifdef __cplusplus
}
#endif

#endif /* FN_PLATFORM_H */
