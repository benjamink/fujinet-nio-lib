/**
 * @file fn_protocol.h
 * @brief FujiBus Protocol Definitions
 * 
 * Low-level protocol constants and structures for FujiBus communication.
 * This header is used internally by the library but may also be useful
 * for advanced applications that need direct protocol access.
 * 
 * @version 1.0.0
 */

#ifndef FN_PROTOCOL_H
#define FN_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _CMOC_VERSION_
    #include <cmoc.h>
#else
    #include <stdint.h>
#endif

#include "fujinet-nio.h"

/* ============================================================================
 * SLIP Protocol Constants
 * ============================================================================ */

/** SLIP END byte - marks frame boundaries */
#define SLIP_END       0xC0

/** SLIP ESCAPE byte - escape prefix */
#define SLIP_ESCAPE    0xDB

/** Escaped END byte */
#define SLIP_ESC_END   0xDC

/** Escaped ESCAPE byte */
#define SLIP_ESC_ESC   0xDD

/* ============================================================================
 * Wire Device IDs
 * ============================================================================ */

/** FujiNet configuration device */
#define FN_DEVICE_FUJI       0x70

/** Network service device (HTTP/TCP) */
#define FN_DEVICE_NETWORK    0xFD

/** Disk service device */
#define FN_DEVICE_DISK       0xFC

/** File service device */
#define FN_DEVICE_FILE       0xFE

/** Namespaced application-state service */
#define FN_DEVICE_APPSTORE   0xF1

/** Sparse slot-catalogue service */
#define FN_DEVICE_SLOT_CATALOG 0xF2

#define FN_DEVICE_WIFI        0xF3

/** Clock device */
#define FN_DEVICE_CLOCK      0x45

/* ============================================================================
 * Network Device Commands
 * ============================================================================ */

/** Open a network session */
#define FN_CMD_OPEN    0x01

/** Read data from session */
#define FN_CMD_READ    0x02

/** Write data to session */
#define FN_CMD_WRITE   0x03

/** Close a session */
#define FN_CMD_CLOSE   0x04

/** Get session information */
#define FN_CMD_INFO    0x05

#define FN_WIFI_PROTOCOL_VERSION 1
#define FN_WIFI_CMD_GET_STATUS   0x01
#define FN_WIFI_CMD_GET_CONFIG   0x02
#define FN_WIFI_CMD_SET_CONFIG   0x03
#define FN_WIFI_CMD_SCAN         0x04
#define FN_WIFI_CMD_GET_ADAPTER_INFO 0x05

/* ============================================================================
 * Clock Device Commands
 * ============================================================================ */

/** Get current time (raw Unix timestamp) */
#define FN_CMD_CLOCK_GET         0x01

/** Set current time (raw Unix timestamp) */
#define FN_CMD_CLOCK_SET         0x02

/** Get time in specified format */
#define FN_CMD_CLOCK_GET_FORMAT  0x03

/** Get current timezone string */
#define FN_CMD_CLOCK_GET_TZ      0x04

/** Set timezone (non-persistent) */
#define FN_CMD_CLOCK_SET_TZ      0x05

/** Set timezone and persist to config */
#define FN_CMD_CLOCK_SET_TZ_SAVE 0x06

/** Synchronize time from network (NTP) */
#define FN_CMD_CLOCK_SYNC_NETWORK_TIME 0x07

/** Clock protocol version */
#define FN_CLOCK_VERSION    0x01

/* File service commands */

/** Read bytes from a filesystem URI */
#define FN_CMD_FILE_READ       0x03

/** Write bytes to a filesystem URI */
#define FN_CMD_FILE_WRITE      0x04

/** Resolve a filesystem target to canonical URI + display path */
#define FN_CMD_FILE_RESOLVE_PATH 0x05

/** Create a directory at a filesystem URI */
#define FN_CMD_FILE_MKDIR      0x06

/* AppStore service commands */
#define FN_CMD_APPSTORE_STAT   0x01

/** Read bytes from an application storage key */
#define FN_CMD_APPSTORE_READ   0x02

/** Write bytes to an application storage key */
#define FN_CMD_APPSTORE_WRITE  0x03

/** Delete an application storage key */
#define FN_CMD_APPSTORE_DELETE 0x04

/** List keys in an application storage namespace */
#define FN_CMD_APPSTORE_LIST   0x05

/* Slot-catalogue service commands */
#define FN_CMD_SLOT_CATALOG_GET    0x01
#define FN_CMD_SLOT_CATALOG_PUT    0x02
#define FN_CMD_SLOT_CATALOG_DELETE 0x03
#define FN_CMD_SLOT_CATALOG_RANGE  0x04

/* ============================================================================
 * Protocol Version
 * ============================================================================ */

/** Current protocol version */
#define FN_PROTOCOL_VERSION  0x01

/* ============================================================================
 * Open Flags (Wire Format)
 * ============================================================================ */

/** Use TLS for connection */
#define FN_OPEN_FLAG_TLS           0x01

/** Follow HTTP redirects */
#define FN_OPEN_FLAG_FOLLOW_REDIR  0x02

/** Body length unknown (POST/PUT) */
#define FN_OPEN_FLAG_BODY_UNKNOWN  0x04

/** Allow handle eviction */
#define FN_OPEN_FLAG_ALLOW_EVICT   0x08

/** Streaming read policy: return current chunk without an extra probe read */
#define FN_OPEN_FLAG_STREAM_NO_PROBE 0x10

/* ============================================================================
 * Open Response Flags (Wire Format)
 * ============================================================================ */

/** Handle was allocated successfully */
#define FN_OPEN_RESP_ACCEPTED      0x01

/** Body write required (POST/PUT) */
#define FN_OPEN_RESP_NEEDS_BODY    0x02

/* ============================================================================
 * Protocol Capability Flags (returned in Open response)
 * ============================================================================ */

/** Read operations must be sequential (streaming protocols like TCP) */
#define FN_PROTO_FLAG_SEQUENTIAL_READ  0x01

/** Write operations must be sequential (streaming protocols like TCP) */
#define FN_PROTO_FLAG_SEQUENTIAL_WRITE 0x02

/** Streaming protocol (no content-length, read until EOF) */
#define FN_PROTO_FLAG_STREAMING        0x04

/* ============================================================================
 * Read Response Flags (Wire Format)
 * ============================================================================ */

/** End of data reached */
#define FN_READ_RESP_EOF           0x01

/** Response truncated */
#define FN_READ_RESP_TRUNCATED     0x02

/** Additional bytes are already immediately available after this chunk */
#define FN_READ_RESP_MORE_AVAILABLE 0x04

/* ============================================================================
 * Info Response Flags (Wire Format)
 * ============================================================================ */

/** Response headers included */
#define FN_INFO_RESP_HEADERS       0x01

/** Content length available */
#define FN_INFO_RESP_HAS_LENGTH    0x02

/** HTTP status available */
#define FN_INFO_RESP_HAS_STATUS    0x04

/* ============================================================================
 * FujiBus Packet Structure
 * ============================================================================ */

/**
 * FujiBus packet header structure.
 * 
 * Wire format:
 *   u8  device_id     - WireDeviceId
 *   u8  command       - Command byte
 *   u16 length        - Total packet length (little-endian)
 *   u8  checksum      - Checksum byte (sum with carry folding)
 *   u8  descriptor    - Parameter descriptor (0 for simple packets)
 *   u8[] payload      - Payload data
 */

/** Maximum FujiBus packet size */
#ifndef FN_MAX_PACKET_SIZE
#ifdef __CC65__
#define FN_MAX_PACKET_SIZE   512
#else
#define FN_MAX_PACKET_SIZE   1024
#endif
#endif

/** Maximum parameters per packet */
#define FN_MAX_PARAMS        4

/** Parameter descriptor size in bytes */
#define FN_PARAM_DESC_SIZE   4

/** FujiBus packet header size */
#define FN_HEADER_SIZE       6

/** Byte offset of the checksum field within a FujiBus packet */
#define FN_CHECKSUM_OFFSET   4

/** Legacy name for compatibility */
#define FN_PACKET_HEADER_SIZE FN_HEADER_SIZE

/* ============================================================================
 * Parameter Descriptor Format
 * ============================================================================ */

/**
 * Parameter descriptor (4 bytes):
 *   u8  size      - Parameter size (1, 2, or 4 bytes)
 *   u8  reserved  - Must be 0
 *   u16 value     - Parameter value (little-endian, right-aligned)
 */

/** Parameter sizes */
#define FN_PARAM_SIZE_U8     1
#define FN_PARAM_SIZE_U16    2
#define FN_PARAM_SIZE_U32    4

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

/**
 * Session state for tracking open handles.
 * Used internally by the library.
 */
typedef struct {
    uint8_t active;        /**< 1 if session is active */
    uint8_t proto_flags;   /**< Protocol capability flags (FN_PROTO_FLAG_*) */
    uint8_t needs_body;    /**< 1 if body write required */
    uint8_t reserved;      /**< Padding */
    fn_handle_t handle;    /**< Device-assigned handle */
    uint32_t write_offset; /**< Current write offset */
    uint32_t read_offset;  /**< Current read offset */
} fn_session_t;

/* ============================================================================
 * Low-Level Packet Functions (Internal Use)
 * ============================================================================ */

/**
 * Calculate FujiBus checksum.
 * 
 * @param data      Byte array
 * @param len       Length of data
 * @return Checksum byte
 */
uint8_t fn_calc_checksum(const uint8_t *data, uint16_t len);

/**
 * Calculate a FujiBus packet checksum without mutating the packet.
 *
 * The encoded checksum field at FN_CHECKSUM_OFFSET is excluded from the
 * calculation. Packet creation callers assign the returned byte to that
 * field; validation callers compare it with the received field.
 */
uint8_t fn_calc_packet_checksum(const uint8_t *packet, uint16_t len);

/**
 * Build a FujiBus packet header.
 * 
 * Header format: device(1) + command(1) + length(2) + checksum(1) + descr(1) = 6 bytes
 * 
 * @param buffer      Output buffer
 * @param device_id   WireDeviceId
 * @param command     Command byte
 * @param total_len   Total packet length (header + payload)
 * @return Number of bytes written
 */
uint16_t fn_build_header(uint8_t *buffer,
                         uint8_t device_id,
                         uint8_t command,
                         uint16_t total_len);

/**
 * Add a parameter to a packet.
 * 
 * @param buffer    Packet buffer (positioned after header)
 * @param param     Parameter index (0-3)
 * @param value     Parameter value
 * @param size      Parameter size (1, 2, or 4)
 * @return Number of bytes written
 */
uint16_t fn_add_param(uint8_t *buffer,
                      uint8_t param,
                      uint32_t value,
                      uint8_t size);

#ifdef __cplusplus
}
#endif

#endif /* FN_PROTOCOL_H */
