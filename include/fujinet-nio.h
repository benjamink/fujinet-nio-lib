/**
 * @file fujinet-nio.h
 * @brief FujiNet-NIO Library for C Applications
 * 
 * This library provides a clean interface for C applications to communicate
 * with FujiNet-NIO devices using the FujiBus protocol. It supports both HTTP
 * and TCP network operations through a handle-based API.
 * 
 * @version 1.0.0
 * @license GPL v3, see LICENSE for details.
 */

#ifndef FUJINET_NIO_H
#define FUJINET_NIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Platform detection for standard types */
#ifdef _CMOC_VERSION_
    #include <cmoc.h>
#else
    #include <stdint.h>
    #include <stddef.h>
#endif

/* cc65 doesn't have uint64_t - use struct for 64-bit time values */
#ifdef __CC65__
    /* 64-bit time value as 8-byte array (little-endian) */
    typedef struct {
        uint8_t b[8];
    } fn_time_t;
    #define FN_TIME_T fn_time_t
#else
    #define FN_TIME_T uint64_t
#endif

/* ============================================================================
 * Constants and Configuration
 * ============================================================================ */

/** Maximum URL length supported */
#ifndef FN_MAX_URL_LEN
#ifdef __BBC__
#define FN_MAX_URL_LEN      512
#elif defined(__CC65__)
#define FN_MAX_URL_LEN      128
#else
#define FN_MAX_URL_LEN      256
#endif
#endif

/** Maximum concurrent network sessions */
#ifndef FN_MAX_SESSIONS
#ifdef __CC65__
#define FN_MAX_SESSIONS     2
#else
#define FN_MAX_SESSIONS     4
#endif
#endif

/** Maximum read/write chunk size */
#ifndef FN_MAX_CHUNK_SIZE
#define FN_MAX_CHUNK_SIZE   512
#endif

/* ============================================================================
 * Error Codes
 * ============================================================================ */

/** Operation completed successfully */
#define FN_OK               0x00

/** Device not found */
#define FN_ERR_NOT_FOUND    0x01

/** Invalid parameter or malformed request */
#define FN_ERR_INVALID      0x02

/** Device is busy, retry later */
#define FN_ERR_BUSY         0x03

/** Operation not ready, poll again */
#define FN_ERR_NOT_READY    0x04

/** I/O error during operation */
#define FN_ERR_IO           0x05

/** Device did not respond in time */
#define FN_ERR_TIMEOUT      0x06

/** Internal error */
#define FN_ERR_INTERNAL     0x07

/** Operation not supported */
#define FN_ERR_UNSUPPORTED  0x08

/** Transport layer error */
#define FN_ERR_TRANSPORT    0x10

/** URL too long */
#define FN_ERR_URL_TOO_LONG 0x11

/** No free handles available */
#define FN_ERR_NO_HANDLES   0x12

/** Request aborted locally (client-visible FN error; not a FujiBus wire status) */
#define FN_ERR_ABORTED      0x13

/** Unknown/unexpected error */
#define FN_ERR_UNKNOWN      0xFF

#ifdef __ATARI__
/* Optional native-SIO capability query.  Never performed implicitly. */
/* Bits of fn_atari_capability_flags(). */
#define FN_ATARI_CAP_ANY_READ 0x01u /* R may ask for more than the response; it is zero padded */
#define FN_ATARI_CAP_PROC     0x02u /* the FujiNet pulses PROC when a network session becomes readable
                                     * (`sio.tune proc 1`); see fn_atari_proc_enable */

uint8_t fn_atari_query_capabilities(void);
uint16_t fn_atari_max_payload(void);
/* Feature bits returned by the most recent successful explicit Q; zero after
 * init or a failed query. */
uint8_t fn_atari_capability_flags(void);
/* ISR-safe readiness latch for an application-owned/chained VPRCED handler. */
void fn_atari_proc_notify(void);

/* Return the selected PBI bit of a compatible, selectable U1MB/Incognito
 * PBI BIOS, or zero if none is available. This does not detect RAM alone or
 * establish whether HSIO is enabled in the BIOS. */
uint8_t fn_atari_u1mb_pbi(void);

/* Clear selected U1MB non-disk device speed cache slots, bits 0-3.
 * The U1MB's undocumented PBI tables are detected before they are touched;
 * device IDs and disk slots are not changed. Call outside an SIO operation.
 * Returns FN_ERR_UNSUPPORTED if the PBI firmware is not found and FN_ERR_IO
 * if a selected byte does not read back as zero. */
uint8_t fn_atari_u1mb_clear_devices(uint8_t mask);
/* Clear every non-disk cache slot whose device ID equals device. */
uint8_t fn_atari_u1mb_clear_device(uint8_t device);
/* Clear disk speed entries 1-15: bit 0 is D1, bit 14 is D15. */
uint8_t fn_atari_u1mb_clear_disks(uint16_t mask);

/* High-speed SIO for the native endpoint (docs/atari/hsio.md). The Atari OS only speaks 19200
 * baud, and a SIO speed driver such as the U1MB's speeds up disks but not this endpoint, so a
 * program that wants it asks the FujiNet for its speed index (the Happy/Speedy 3F query) and from
 * then on sends every native call through a driver of its own (derived from Matthias Reichl's,
 * GPL). Opt-in: nothing of it is linked into a program that does not call fn_atari_hsio_enable.
 * Call it after fn_init.
 *
 * Returns FN_OK, or FN_ERR_UNSUPPORTED when the FujiNet has no high-speed index set (`sio.hsio`
 * on its console) or answers one that is not usable; native calls then stay on the OS SIO. */
uint8_t fn_atari_hsio_enable(void);
/* Native calls use the OS SIO again. */
void fn_atari_hsio_disable(void);
/* The index in use (0-9, the POKEY divisor), or 0xFF when high speed is off. */
uint8_t fn_atari_hsio_index(void);
/* How often the driver met an error since enabling (saturating at 255, retried ones included):
 * code 0x8A timeout, 0x8B NAK/no ACK, 0x8C framing, 0x8E overrun, 0x8F checksum; zero for others.
 * An error after the ACK makes it retry the command at standard speed. */
uint8_t fn_atari_hsio_error_count(uint8_t code);

/* The PROC (proceed) interrupt (docs/atari/proc.md). The FujiNet drives the PROC line to the Atari's PIA,
 * whose falling edge raises the OS PROCEED interrupt (the VPRCED vector). fn_atari_proc_enable installs a
 * handler that counts each interrupt, latches a pending flag and chains to the handler that was there,
 * and turns the PIA interrupt on. The program then polls fn_atari_proc_pending() (or fn_atari_proc_count())
 * from its own loop instead of sending P over SIO. Opt-in: nothing of it is linked into a program that
 * does not call fn_atari_proc_enable. Returns FN_OK; safe to call again.
 *
 * The OS clears the interrupt enable when it drives the cassette motor, so a program that uses the
 * cassette must call fn_atari_proc_enable again afterwards. Polling with P stays correct without PROC. */
uint8_t fn_atari_proc_enable(void);
/* Interrupt off, the previous VPRCED handler back. */
void fn_atari_proc_disable(void);
/* 1 if a PROC interrupt came since the last call (or since enabling), and clears it; else 0. */
uint8_t fn_atari_proc_pending(void);
/* PROC interrupts since enabling (wraps at 65536). */
uint16_t fn_atari_proc_count(void);
#endif

#define FN_WIFI_MAX_SSID      32
#define FN_WIFI_MAX_BSSID     17
#define FN_WIFI_MAX_PASSWORD  64
#define FN_WIFI_MAX_SCAN_RECORDS 32
#define FN_WIFI_SCAN_RESPONSE_HEADER_SIZE 3
#define FN_WIFI_SCAN_RECORD_WIRE_MAX (1 + FN_WIFI_MAX_SSID + 9)
#define FN_WIFI_SCAN_RESPONSE_MAX \
    (FN_WIFI_SCAN_RESPONSE_HEADER_SIZE + \
     (FN_WIFI_MAX_SCAN_RECORDS * FN_WIFI_SCAN_RECORD_WIRE_MAX))
#define FN_WIFI_SET_ENABLED   0x01
#define FN_WIFI_SET_SSID      0x02
#define FN_WIFI_SET_BSSID     0x04
#define FN_WIFI_SET_PASSWORD  0x08
#define FN_WIFI_SET_PERSIST   0x10
#define FN_WIFI_SET_RECONNECT 0x20
#define FN_WIFI_CAP_CONFIG    0x0001
#define FN_WIFI_CAP_STATUS    0x0002
#define FN_WIFI_CAP_CONNECT   0x0004
#define FN_WIFI_CAP_DISCONNECT 0x0008
#define FN_WIFI_CAP_SCAN      0x0010
#define FN_WIFI_CAP_BSSID     0x0020
#define FN_WIFI_CAP_HOST      0x0040
#define FN_WIFI_CAP_SIMULATED 0x0080
#define FN_WIFI_BACKEND_UNAVAILABLE 0
#define FN_WIFI_BACKEND_ESP32       1
#define FN_WIFI_BACKEND_POSIX_HOST  2
#define FN_WIFI_BACKEND_POSIX_SIMULATED 3

/* Wi-Fi API buffers are caller-owned. Returned strings are NUL terminated. */
typedef struct { uint8_t bytes[6]; uint8_t valid; } fn_wifi_bssid_t;
typedef struct {
    uint8_t link_state, configured_enabled, bssid_valid, scan_supported;
    int8_t rssi;
    fn_wifi_bssid_t bssid;
    uint16_t capabilities;
    uint8_t backend_kind;
    char ip[16], subnet[16], gateway[16], dns[16];
} fn_wifi_status_t;
typedef struct {
    uint8_t enabled, password_present;
    char ssid[FN_WIFI_MAX_SSID + 1];
    char bssid[FN_WIFI_MAX_BSSID + 1];
} fn_wifi_config_t;
typedef struct {
    uint8_t fields, enabled;
    const char *ssid, *bssid, *password;
} fn_wifi_config_update_t;
typedef struct {
    char ssid[FN_WIFI_MAX_SSID + 1];
    fn_wifi_bssid_t bssid;
    int8_t rssi;
    uint8_t channel, auth;
} fn_wifi_scan_record_t;
typedef struct {
    fn_wifi_bssid_t mac;    /* station interface MAC; mac.valid=0 when unknown */
} fn_wifi_adapter_info_t;

#define FN_WIFI_JOIN_IDLE       0
#define FN_WIFI_JOIN_PENDING    1
#define FN_WIFI_JOIN_CONNECTED  2
#define FN_WIFI_JOIN_FAILED     3
/* A trial retains the previous credentials until the new link is connected.
 * On failure the firmware attempts to restore the previous network. */
uint8_t fn_wifi_try_join(const char *ssid, const char *password);
uint8_t fn_wifi_get_join_status(uint8_t *state);

/* Password input is consumed during the call and is never returned. */
uint8_t fn_wifi_get_status(fn_wifi_status_t *status);
uint8_t fn_wifi_get_config(fn_wifi_config_t *config);
uint8_t fn_wifi_set_config(const fn_wifi_config_update_t *update);
/* Station MAC. Firmware that predates the command returns
 * FN_ERR_UNSUPPORTED; callers should show the MAC as unknown. The firmware
 * version comes from fn_fuji_get_info(). */
uint8_t fn_wifi_get_adapter_info(fn_wifi_adapter_info_t *info);

/* FujiDevice: describes the FujiNet itself. No network is involved, so the
 * call answers quickly on any board. */
#define FN_FUJI_MAX_FIRMWARE_VERSION 32
#define FN_FUJI_HSIO_OFF 0xff
/* Advertised SIO divisor, 0-9 or FN_FUJI_HSIO_OFF. Changes are transient.
 * A speed driver that caches per-device values needs its cache cleared before
 * it will re-query; the call itself does not change an Atari driver's cache. */
uint8_t fn_fuji_get_hsio(uint8_t *index);
uint8_t fn_fuji_set_hsio(uint8_t index);
#define FN_FUJI_MAX_BUILD_PROFILE    64
typedef struct {
    char firmware[FN_FUJI_MAX_FIRMWARE_VERSION + 1];   /* e.g. "0.1.1" */
    char profile[FN_FUJI_MAX_BUILD_PROFILE + 1];       /* build profile name */
} fn_fuji_info_t;
/* Firmware version and build profile. Firmware that predates the command
 * returns FN_ERR_UNSUPPORTED; callers should show the fields as unknown. */
uint8_t fn_fuji_get_info(fn_fuji_info_t *info);
/* offset is a record index; limit/capacity are bounded to 32 records and
 * count is the number written to records. response_buffer is caller-owned
 * scratch storage used for the wire response. The helper reduces the wire
 * page size when the supplied storage cannot hold 32 maximum-size records. */
uint8_t fn_wifi_scan(uint16_t offset, uint8_t limit, fn_wifi_scan_record_t *records,
                     uint8_t capacity, uint8_t *count, uint8_t *more,
                     uint8_t *response_buffer, uint16_t response_capacity);

/* ============================================================================
 * HTTP Method Codes
 * ============================================================================ */

/** HTTP GET method */
#define FN_METHOD_GET       0x01

/** HTTP POST method */
#define FN_METHOD_POST      0x02

/** HTTP PUT method */
#define FN_METHOD_PUT       0x03

/** HTTP DELETE method */
#define FN_METHOD_DELETE    0x04

/** HTTP HEAD method */
#define FN_METHOD_HEAD      0x05

/* ============================================================================
 * Open Flags
 * ============================================================================ */

/**
 * Legacy TLS hint.
 *
 * URL schemes are authoritative across the library: use `https://` or `tls://`
 * in the URL itself. This flag is retained for compatibility but should not be
 * relied on for new code.
 */
#define FN_OPEN_TLS         0x01

/** Follow HTTP redirects automatically */
#define FN_OPEN_FOLLOW_REDIR 0x02

/** POST/PUT body length is unknown; commit with a zero-length write */
#define FN_OPEN_BODY_UNKNOWN 0x04

/** Allow handle eviction if no handles available */
#define FN_OPEN_ALLOW_EVICT 0x08

/** Streaming protocol is self-framed; avoid an extra probe read after each chunk */
#define FN_OPEN_STREAM_NO_PROBE 0x10

/** Keep the session until it is closed. The firmware closes a session that sees no request for a long time
 *  (20 minutes by default, wall clock) so a client that went away does not hold a handle; a session opened
 *  with this flag is exempt, and is never evicted to make room (an Open with FN_OPEN_ALLOW_EVICT gets
 *  FN_ERR_BUSY if every handle is kept). For a long-lived connection that may be quiet for a long time.
 *  Firmware that predates it ignores the bit. Not available on the BBC's direct OSFIND open path. */
#define FN_OPEN_KEEP 0x20

/* ============================================================================
 * Read Response Flags
 * ============================================================================ */

/** End of data reached (EOF) */
#define FN_READ_EOF         0x01

/** Response was truncated to fit buffer */
#define FN_READ_TRUNCATED   0x02

/** Additional bytes are already immediately available after this chunk */
#define FN_READ_MORE_AVAILABLE 0x04

/** The firmware reports how many bytes it still holds for the handle (fn_read_available() is meaningful) */
#define FN_READ_AVAILABLE   0x08

/* ============================================================================
 * Info Flags
 * ============================================================================ */

/** HTTP status code is available */
#define FN_INFO_HAS_STATUS  0x04

/** Content length is available */
#define FN_INFO_HAS_LENGTH  0x02

/** TCP: Socket is connected */
#define FN_INFO_CONNECTED   0x10

/** TCP: Peer has closed the connection */
#define FN_INFO_PEER_CLOSED 0x20

/* ============================================================================
 * Application Storage
 * ============================================================================ */

/** App-store read reached end of value */
#define FN_APPSTORE_READ_EOF     0x01

/** App-store read target key exists */
#define FN_APPSTORE_READ_EXISTS  0x02

/** App-store list has more keys after this page */
#define FN_APPSTORE_LIST_MORE    0x01

/** Slot catalogue request should retain the tail of a truncated URI */
#define FN_SLOT_CATALOG_TAIL_URI 0x01
#define FN_SLOT_CATALOG_FORMATTED 0x02

/** Slot catalogue response has another complete-record page */
#define FN_SLOT_CATALOG_MORE 0x01
#define FN_SLOT_CATALOG_RESPONSE_FORMATTED 0x02

/** Slot catalogue entry contains a valid version-1 slot record */
#define FN_SLOT_CATALOG_ENTRY_VALID 0x01

/** Slot catalogue entry requests read-only mounting */
#define FN_SLOT_CATALOG_ENTRY_READ_ONLY 0x02

/** Slot catalogue entry URI was shortened to the requested limit */
#define FN_SLOT_CATALOG_ENTRY_URI_TRUNCATED 0x04

/* ============================================================================
 * BBC / fn-rom Network Extensions
 * ============================================================================ */

/** No explicit content profile */
#define FN_CONTENT_PROFILE_NONE 0x00

/** Send request body as application/json */
#define FN_CONTENT_PROFILE_JSON 0x01

/** Send request body as application/x-www-form-urlencoded */
#define FN_CONTENT_PROFILE_FORM 0x02

/** Send request body as text/plain */
#define FN_CONTENT_PROFILE_TEXT 0x03

/* ============================================================================
 * Types
 * ============================================================================ */

/** Network session handle */
typedef uint16_t fn_handle_t;

/** Invalid handle value */
#define FN_INVALID_HANDLE   0x0000

/** Application storage stat result */
typedef struct {
    uint8_t exists;          /**< Non-zero if the key exists */
    uint32_t size_bytes;     /**< Low 32 bits of value size */
    uint32_t size_bytes_high;/**< High 32 bits of value size */
    uint32_t mtime_unix;     /**< Low 32 bits of modified Unix time, or 0 */
    uint32_t mtime_unix_high;/**< High 32 bits of modified Unix time, or 0 */
} fn_appstore_stat_t;

/** Application storage read result */
typedef struct {
    uint8_t flags;           /**< FN_APPSTORE_READ_* flags */
    uint32_t offset;         /**< Echoed read offset */
    uint16_t bytes_read;     /**< Bytes copied into caller buffer */
} fn_appstore_read_t;

/** Application storage write result */
typedef struct {
    uint32_t offset;         /**< Echoed write offset */
    uint16_t bytes_written;  /**< Bytes accepted by device */
} fn_appstore_write_t;

/** Application storage delete result */
typedef struct {
    uint8_t deleted;         /**< Non-zero if an existing key was removed */
} fn_appstore_delete_t;

/** Application storage list page result */
typedef struct {
    uint8_t flags;           /**< FN_APPSTORE_LIST_* flags */
    uint16_t start_index;    /**< Echoed start index */
    uint16_t key_count;      /**< Keys encoded in key_data */
    uint16_t key_data_len;   /**< Bytes written into key_data */
} fn_appstore_list_t;

/** Caller-owned scratch buffer for request/response services. */
typedef struct {
    uint8_t *buffer;         /**< Scratch buffer used for request and response */
    uint16_t capacity;       /**< Bytes available at buffer */
} fn_service_io_t;

/* ---- FileService: paged directory listing ----------------------------------
 *
 * fn_file_list_pages() asks the device for whole pages of a directory.  The
 * device lists and sorts the directory once, remembers it, and returns as many
 * complete pages as fit `max_payload` bytes, together with the total entry
 * count, so a small client can show "page n of m" and fetch only what it shows.
 * If not even the first page fits, page_count is 0 and FN_LIST_PAGES_MORE is
 * set: ask again with a smaller page_size.
 *
 * After the call `out->pages` points into the caller's buffer at page_count
 * pages, `out->pages_len` bytes in all.  Each page is
 *     u8  entry_count   (== page_size, except the directory's last page)
 *     u16 data_len      (LE; bytes of entries that follow)
 *     entries           entry_count times:
 *         u8  flags     FN_LIST_ENTRY_DIR, FN_LIST_ENTRY_NAME_CUT
 *         u8  name_len
 *         name_len bytes of basename (not NUL terminated)
 *         u32 size      (LE; bytes, saturating; 0 for a directory)
 *         u32 mtime     (LE; Unix seconds, 0 if unknown)
 */
#define FN_LIST_SORT        0x01  /* sort by basename, ASCII case-insensitive */
#define FN_LIST_DIRS_FIRST  0x02  /* with FN_LIST_SORT: directories before files */
#define FN_LIST_REFRESH     0x04  /* re-read the directory, replacing any cached copy */
#define FN_LIST_TOTAL_ONLY  0x08  /* return the total and no pages */

#define FN_LIST_PAGES_MORE      0x01  /* pages exist after those returned */
#define FN_LIST_PAGES_NAME_CUT  0x02  /* some name was cut to max_name */

#define FN_LIST_ENTRY_DIR       0x01
#define FN_LIST_ENTRY_NAME_CUT  0x80

typedef struct {
    uint8_t flags;           /**< FN_LIST_PAGES_* */
    uint32_t total;          /**< entries in the whole directory */
    uint16_t start_page;     /**< echoed */
    uint8_t page_size;       /**< echoed */
    uint8_t page_count;      /**< whole pages returned */
    const uint8_t *pages;    /**< page_count pages, inside the caller's buffer */
    uint16_t pages_len;      /**< their total length in bytes */
} fn_list_pages_t;

typedef fn_service_io_t fn_file_io_t;

/** What fn_file_list_pages asks for. */
typedef struct {
    uint16_t start_page;     /**< first page wanted, 0-based */
    uint8_t page_size;       /**< entries per page, 1..64 */
    uint16_t max_payload;    /**< byte budget for the pages (reply minus 12) */
    uint8_t flags;           /**< FN_LIST_* */
    uint8_t max_name;        /**< cap on each returned name; 0 = the device's limit (220) */
    const char *filter;      /**< optional case-insensitive glob; NULL or "" means no filter */
} fn_list_pages_req_t;

/**
 * Fetch pages req->start_page.. of the directory `uri`.  The request is built
 * in, and the reply lands in, io->buffer, which must hold the request (about
 * 12 + strlen(uri) bytes) and the reply (req->max_payload + 12 bytes).
 */
uint8_t fn_file_list_pages(fn_file_io_t *io,
                           const char *uri,
                           const fn_list_pages_req_t *req,
                           fn_list_pages_t *out);

typedef fn_service_io_t fn_appstore_io_t;
typedef fn_service_io_t fn_slot_catalog_io_t;

/* ============================================================================
 * DiskDevice v1 client API
 * ============================================================================ */

#define FN_DISK_PROTOCOL_VERSION 1
#define FN_DISK_TYPE_AUTO 0
#define FN_DISK_TYPE_ATR  1
#define FN_DISK_TYPE_SSD  2
#define FN_DISK_TYPE_DSD  3
#define FN_DISK_TYPE_RAW  4
#define FN_DISK_TYPE_DC42 5 /* Apple DiskCopy 4.2 (Mac floppy); mount only, no Create */
#define FN_DISK_TYPE_XEXBOOT 6 /* Atari .xex/.com/.exe served as a synthesised read-only boot disk (PicoBoot); mount only, no Create */
#define FN_DISK_TYPE_D64  7 /* Commodore 1541 image; mount only, no Create */
#define FN_DISK_TYPE_PRG  8 /* Commodore .prg served as a synthesised read-only one-file 1541 disk; mount only, no Create */

/*
 * Why a DiskDevice call failed (fujinet-nio disk::DiskError). A failed call
 * returns the transport status as FN_ERR_*; fn_disk_last_error() (or
 * fn_disk_last_error_context()) then gives the exact reason. The values are
 * wire protocol: new ones are appended, never renumbered. See "Status and
 * error mapping" in fujinet-nio's docs/disk_device_protocol.md.
 */
#define FN_DISK_ERR_NONE                0  /* the last call succeeded */
#define FN_DISK_ERR_INVALID_SLOT        1
#define FN_DISK_ERR_INVALID_REQUEST     2
#define FN_DISK_ERR_NO_SUCH_FILESYSTEM  3  /* URI names no filesystem */
#define FN_DISK_ERR_FILE_NOT_FOUND      4
#define FN_DISK_ERR_ALREADY_EXISTS      5
#define FN_DISK_ERR_OPEN_FAILED         6
#define FN_DISK_ERR_UNSUPPORTED_TYPE    7  /* recognised, but not handled */
#define FN_DISK_ERR_BAD_IMAGE           8  /* recognised format, invalid */
#define FN_DISK_ERR_INVALID_GEOMETRY    9  /* sector size does not fit */
#define FN_DISK_ERR_NOT_MOUNTED         10
#define FN_DISK_ERR_READ_ONLY           11
#define FN_DISK_ERR_OUT_OF_RANGE        12
#define FN_DISK_ERR_IO                  13
#define FN_DISK_ERR_INTERNAL            14
#define FN_DISK_ERR_GEOMETRY_REQUIRED   15 /* NIO cannot tell the type or
                                              sector size: mount again with
                                              the ones this machine uses */
#define FN_DISK_ERR_UNREPORTED          0xFF /* failed without a disk error:
                                                a transport failure, or an
                                                NIO that predates them */

/** DiskDevice Info/Mount response flags. */
#define FN_DISK_FLAG_MOUNTED  0x01
#define FN_DISK_FLAG_READONLY 0x02
#define FN_DISK_FLAG_DIRTY    0x04
#define FN_DISK_FLAG_CHANGED  0x08

/** Typed geometry and state returned by DiskDevice Mount/Info. */
typedef struct {
    uint8_t flags;
    uint8_t slot;
    uint8_t type;
    uint16_t sector_size;
    uint32_t sector_count;
    uint8_t last_error;
} fn_disk_info_t;

#define FN_DISK_INSPECT_BOOT_BYTES 512U
typedef struct {
    fn_disk_info_t media;
    uint16_t boot_length;
    uint8_t boot_bytes[FN_DISK_INSPECT_BOOT_BYTES];
} fn_disk_inspection_t;

/** Explicit DiskDevice client state for resident or concurrent callers. */
#define FN_DISK_CONTEXT_PACKET_SIZE 1024
typedef uint8_t (*fn_disk_exchange_fn)(void *exchange_context,
                                      const uint8_t *request,
                                      uint16_t request_length,
                                      uint8_t *response,
                                      uint16_t response_capacity,
                                      uint16_t *response_length);

typedef struct {
    fn_disk_exchange_fn exchange;
    void *exchange_context;
    uint8_t packet_request[FN_DISK_CONTEXT_PACKET_SIZE];
    uint8_t packet_response[FN_DISK_CONTEXT_PACKET_SIZE];
    uint8_t codec_scratch[FN_DISK_CONTEXT_PACKET_SIZE];
    uint8_t last_error; /* FN_DISK_ERR_*; read with fn_disk_last_error_context() */
} fn_disk_client_context_t;

/** FN_DISK_ERR_* for the context's most recent DiskDevice call. */
uint8_t fn_disk_last_error_context(const fn_disk_client_context_t *context);

uint8_t fn_disk_context_init(fn_disk_client_context_t *context,
                             fn_disk_exchange_fn exchange,
                             void *exchange_context);
uint8_t fn_disk_mount_context(fn_disk_client_context_t *context, uint8_t slot,
                              const char *uri, uint8_t readonly, uint8_t type,
                              uint16_t sector_size_hint, fn_disk_info_t *info);
/* Restore the firmware-configured default image into a 1-based slot. */
uint8_t fn_disk_restore_boot_context(fn_disk_client_context_t *context,
                                     uint8_t slot, fn_disk_info_t *info);
uint8_t fn_disk_info_context(fn_disk_client_context_t *context, uint8_t slot,
                             fn_disk_info_t *info);
uint8_t fn_disk_read_sector_context(fn_disk_client_context_t *context,
                                    uint8_t slot, uint32_t lba, uint8_t *data,
                                    uint16_t data_capacity,
                                    uint16_t *data_length);
uint8_t fn_disk_write_sector_context(fn_disk_client_context_t *context,
                                     uint8_t slot, uint32_t lba,
                                     const uint8_t *data,
                                     uint16_t data_length);
uint8_t fn_disk_unmount_context(fn_disk_client_context_t *context,
                                uint8_t slot);
uint8_t fn_disk_clear_changed_context(fn_disk_client_context_t *context,
                                      uint8_t slot);
uint8_t fn_disk_flush_context(fn_disk_client_context_t *context,
                               uint8_t slot);
uint8_t fn_disk_inspect_context(fn_disk_client_context_t *context,
                                const char *uri, uint8_t type,
                                uint16_t sector_size_hint,
                                fn_disk_inspection_t *inspection);

/**
 * Mount an image URI into a DiskDevice slot.
 *
 * The URI is copied into the FujiBus request and is not retained by the
 * library. Slot numbering is 1-based. A non-zero readonly value requests
 * read-only access; the effective mode is returned in info when supplied.
 */
uint8_t fn_disk_mount(uint8_t slot, const char *uri, uint8_t readonly,
                      uint8_t type, uint16_t sector_size_hint,
                      fn_disk_info_t *info);
uint8_t fn_disk_restore_boot(uint8_t slot, fn_disk_info_t *info);

/**
 * FN_DISK_ERR_* for the most recent fn_disk_*() call: FN_DISK_ERR_NONE after
 * success, the exact reason after a failure NIO explained, otherwise
 * FN_DISK_ERR_UNREPORTED. After FN_DISK_ERR_GEOMETRY_REQUIRED, mount again
 * with the image type and sector size this machine uses.
 */
uint8_t fn_disk_last_error(void);

/** Unmount a DiskDevice slot. */
uint8_t fn_disk_unmount(uint8_t slot);

/** Make successful writes since the previous flush durable. */
uint8_t fn_disk_flush(uint8_t slot);

/** Query DiskDevice slot state and geometry. */
uint8_t fn_disk_info(uint8_t slot, fn_disk_info_t *info);

/** Clear the DiskDevice changed flag for a slot. */
uint8_t fn_disk_clear_changed(uint8_t slot);

/**
 * Read one sector by LBA.
 *
 * `data_capacity` is sent as the protocol maxBytes value. The response may
 * contain fewer bytes for variable-sector formats; `data_length` receives
 * the returned byte count.
 */
uint8_t fn_disk_read_sector(uint8_t slot, uint32_t lba,
                            uint8_t *data, uint16_t data_capacity,
                            uint16_t *data_length);

/**
 * Write one sector by LBA.
 *
 * The server validates that data_length contains a complete sector. This
 * helper does not assume a particular geometry, so callers should obtain
 * sector_size through fn_disk_mount() or fn_disk_info() first.
 */
uint8_t fn_disk_write_sector(uint8_t slot, uint32_t lba,
                             const uint8_t *data, uint16_t data_length);

/** Slot catalogue range page; pointers refer to the caller-owned I/O buffer. */
typedef struct {
    uint8_t flags;
    uint8_t next_index;
    uint8_t presence_len;
    uint8_t entry_count;
    uint16_t entry_data_len;
    const uint8_t *presence;
    const uint8_t *entry_data;
} fn_slot_catalog_page_t;

/** One decoded slot catalogue entry; URI bytes are not null-terminated. */
typedef struct {
    uint8_t index;
    uint8_t flags;
    uint16_t uri_len;
    const uint8_t *uri;
} fn_slot_catalog_entry_t;

/** Caller-owned scratch buffer for mount/path resolution helpers */
typedef struct {
    uint8_t *buffer;         /**< Scratch buffer used for request and response */
    uint16_t capacity;       /**< Bytes available at buffer */
} fn_mount_resolve_io_t;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize the FujiNet-NIO library.
 * 
 * This must be called before any other library functions.
 * Performs platform-specific initialization and checks for device presence.
 * 
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_init(void);

/**
 * @brief Release the active transport and reset library session state.
 *
 * A later fn_init() opens a new transport session normally.
 */
void fn_shutdown(void);

/**
 * @brief Check if the FujiNet device is present and ready.
 * 
 * @return 1 if ready, 0 if not ready or not present
 */
uint8_t fn_is_ready(void);

/* ============================================================================
 * Network Operations
 * ============================================================================ */

/**
 * @brief Open a network session.
 * 
 * Creates a new network session and returns a handle for subsequent operations.
 * The URL scheme determines the protocol:
 *   - "http://" or "https://": HTTP protocol
 *   - "tcp://": Raw TCP socket
 * 
 * For HTTP:
 *   - Use FN_METHOD_* constants for the method parameter
 *   - GET/HEAD/DELETE typically complete immediately
 *   - POST/PUT may require fn_write() for body data
 * 
 * For TCP:
 *   - Use method = 0
 *   - URL format: "tcp://hostname:port"
 *   - Connection is established asynchronously
 * 
 * @param handle     Pointer to receive the session handle
 * @param method     HTTP method (FN_METHOD_*) or 0 for TCP
 * @param url        URL string (null-terminated)
 * @param flags      Open flags (FN_OPEN_*)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_open(fn_handle_t *handle, 
                uint8_t method,
                const char *url,
                uint8_t flags);

typedef struct {
    const char *name;
    const char *value;
} fn_http_header_t;

/* Open with HTTP request headers and a known POST/PUT body length. Headers
 * and strings belong to the caller and need only remain valid for this call.
 * BBC: requires ROM reason &09; encoded request must fit 128 bytes.
 * Use fn_bbc_open_headers (fn_bbc.h) with caller storage for up to 512 bytes. */
uint8_t fn_open_headers(fn_handle_t *handle, uint8_t method, const char *url,
                        uint8_t flags, const fn_http_header_t *headers,
                        uint16_t header_count, uint32_t body_len);

/**
 * @brief Open a network session using an explicit long-URL path.
 *
 * This is intended for platforms where the common open path should stay small and
 * long URL support is an opt-in feature. On platforms without a special long-URL
 * path, this reduces to `fn_open()`.
 *
 * @param handle     Pointer to receive the session handle
 * @param method     HTTP method (FN_METHOD_*) or 0 for TCP
 * @param url        URL string (null-terminated)
 * @param flags      Open flags (FN_OPEN_*)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_open_long(fn_handle_t *handle,
                     uint8_t method,
                     const char *url,
                     uint8_t flags);

/* Content translation types for fn_open_translated(). FN_TRANSLATE_IMAGE is
   image translation, with the output format chosen by the selector. */
#define FN_TRANSLATE_NONE  0
#define FN_TRANSLATE_JSON  1
#define FN_TRANSLATE_XML   2
#define FN_TRANSLATE_RSS   3
#define FN_TRANSLATE_IMAGE 4

/**
 * @brief Open a network session, asking the firmware to translate the content.
 *
 * Sends the Open-extension block (translation type, flags, selector) after the
 * standard Open fields. With FN_TRANSLATE_NONE the packet is identical to
 * `fn_open()`. For FN_TRANSLATE_IMAGE the selector chooses the output format and
 * its parameters, e.g. "w=624,h=190,colors=12,base=4,par=1:2" (default
 * keys such as fmt=ilbm are best left out, for older firmware).
 *
 * On BBC, any type other than FN_TRANSLATE_NONE returns FN_ERR_UNSUPPORTED.
 *
 * @param handle            Pointer to receive the session handle
 * @param method            HTTP method (FN_METHOD_*) or 0 for TCP
 * @param url               URL string (null-terminated)
 * @param flags             Open flags (FN_OPEN_*)
 * @param translation_type  FN_TRANSLATE_*
 * @param translation_flags Type-specific translation flags (0 if unused)
 * @param selector          Translation selector string (may be NULL)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_open_translated(fn_handle_t *handle,
                           uint8_t method,
                           const char *url,
                           uint8_t flags,
                           uint8_t translation_type,
                           uint8_t translation_flags,
                           const char *selector);

/**
 * @brief Open a TCP connection (convenience wrapper).
 * 
 * @param handle     Pointer to receive the session handle
 * @param host       Hostname or IP address (null-terminated)
 * @param port       Port number
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_tcp_open(fn_handle_t *handle,
                    const char *host,
                    uint16_t port);

/**
 * @brief Write data to a session.
 * 
 * For HTTP POST/PUT: writes request body data.
 * For TCP: sends data on the socket.
 * 
 * Offsets must be sequential. For HTTP, the request is dispatched
 * automatically when bodyLenHint bytes have been written.
 *
 * The library retries transient FN_ERR_NOT_READY/FN_ERR_BUSY responses and
 * continues partial writes until the requested byte count has been accepted or
 * a non-transient error occurs. Writes larger than one packet are split into
 * bounded requests; each request includes 15 bytes of protocol overhead.
 * 
 * @param handle     Session handle
 * @param offset     Byte offset (must be sequential)
 * @param data       Data buffer to write
 * @param len        Length of data
 * @param written    Pointer to receive bytes actually written
 * @return FN_OK on success, error code on failure
 */
typedef struct {
    uint16_t exchanges;          /* transport exchanges issued by fn_write */
    uint16_t transport_retry;    /* transport returned BUSY or NOT_READY */
    uint16_t service_retry;      /* parsed write response was BUSY/NOT_READY */
    uint16_t zero_accepted;      /* successful response accepted no bytes */
    uint16_t response_length;    /* raw response size from the final exchange */
    uint8_t response_device;     /* raw response header device, if present */
    uint8_t response_command;    /* raw response header command, if present */
} fn_write_diagnostics_t;

/* Diagnostics for the most recent fn_write(). This is observational only:
 * it does not alter the retry or write behaviour. */
void fn_write_get_last_diagnostics(fn_write_diagnostics_t *diagnostics);

#ifdef __AMIGA__
/* Request-local broker completion detail from the most recent transport
 * exchange. Intended for focused diagnostics; zero means no broker detail. */
void fn_amiga_transport_last_broker_detail(uint8_t *stage, uint8_t *result);
/* Detailed class for the most recent broker exchange: zero on success.
 * The value is observational and does not alter fn_* error handling. */
void fn_amiga_transport_last_broker_cause(uint8_t *cause);
/* Native serial.device error/status for the most recent broker exchange. */
void fn_amiga_transport_last_serial_detail(uint8_t *native_error,
                                            uint16_t *status);
#endif

#ifdef __palmos__
/* Springboard CS1 link, for tools that need more than fn_init(). The SBN_*
 * names (registers, results) are in sbnio.h from fujinet-nio's
 * bridges/rp2350-springboard/include. */

/* Find the module and check its CS1 registers. Returns the CS1 base, 0 if
 * none. fn_palmos_close forgets it so the next open looks again. */
uint32_t fn_palmos_open(void);
void fn_palmos_close(void);
uint32_t fn_palmos_cs0(void);
uint32_t fn_palmos_cs1(void);
uint16_t fn_palmos_card(void);

/* One 16-bit register read at an SBN_R_* offset, one write to an SBN_W_*
 * register; 0 and nothing while no module is open. */
uint16_t fn_palmos_read(uint16_t offset);
void fn_palmos_write(uint16_t reg, uint16_t value);

/* Send one raw FujiBus packet and wait for its reply. Returns an SBN_RES_*
 * value, FN_PALMOS_NO_ANSWER (no module, or no reply published) or
 * FN_PALMOS_BAD_READ (reply never read back with the right check). A longer
 * reply is cut to cap bytes. */
#define FN_PALMOS_NO_ANSWER 0xFFFFu
#define FN_PALMOS_BAD_READ  0xFFFEu
uint16_t fn_palmos_exchange(const uint8_t *req, uint16_t len,
                            uint8_t *resp, uint16_t cap, uint16_t *resp_len);

/* One line of text to the firmware console. */
void fn_palmos_log(const char *text);
#endif

uint8_t fn_write(fn_handle_t handle,
                 uint32_t offset,
                 const uint8_t *data,
                 uint16_t len,
                 uint16_t *written);

/**
 * @brief Read data from a session.
 * 
 * For HTTP: reads response body data.
 * For TCP: receives data from the socket.
 * 
 * Continue reading until FN_READ_EOF flag is set or bytes_read is 0.
 * On non-cc65 targets, requests larger than one packet are split into bounded
 * exchanges. A short read or temporary not-ready after progress returns the
 * bytes already received; the caller should continue from the new offset.
 * On cc65, each call returns at most one packet's worth of data.
 * 
 * @param handle      Session handle
 * @param offset      Byte offset (must be sequential for TCP)
 * @param buf         Buffer to receive data
 * @param max_len     Maximum bytes to read
 * @param bytes_read  Pointer to receive bytes actually read
 * @param flags       Pointer to receive read flags (FN_READ_*)
 * @return FN_OK on success, FN_ERR_NOT_READY if no data available
 */
uint8_t fn_read(fn_handle_t handle,
                uint32_t offset,
                uint8_t *buf,
                uint16_t max_len,
                uint16_t *bytes_read,
                uint8_t *flags);

/**
 * @brief Bytes already held for the handle after the most recent fn_read().
 *
 * On BBC this is the ROM-buffered lower bound (0 on older ROMs), not a firmware queue snapshot.
 * Meaningful when that fn_read() returned FN_READ_AVAILABLE in its flags (the firmware reports it; older
 * firmware does not, and this returns 0). A lower bound: more may arrive. Saturates at 65535. A client
 * reading a length-prefixed or request/reply stream uses it to know whether the next fn_read() will find data
 * already waiting, without a call that answers FN_ERR_NOT_READY.
 */
uint16_t fn_read_available(void);

/** Flags of an fn_ready_entry_t. */
#define FN_READY_READ 0x01 /* fn_read() would not answer FN_ERR_NOT_READY (data held, end of stream or an error) */
#define FN_READY_INFO 0x02 /* fn_info() would answer (HTTP: the status and headers have arrived) */

typedef struct {
    fn_handle_t handle;
    uint8_t flags;       /* FN_READY_* */
    uint16_t available;  /* bytes held for the handle (saturating), 0 when the protocol cannot say */
} fn_ready_entry_t;

/**
 * @brief Which of this client's sessions can be read or queried now.
 *
 * One small call replaces a speculative fn_read() or fn_info() on each handle: wait (on a timer, or on a
 * hardware hint such as the Atari PROC line, see fn_atari_proc_enable) and then ask once. Only ready sessions
 * are listed. It is a hint: an entry can be early or for data another call already took, and the call it
 * announces still answers FN_ERR_NOT_READY if so. A client that keeps calling this is not idle for the
 * firmware's idle timeout.
 *
 * @param entries      Buffer for the entries
 * @param max_entries  Capacity of the buffer
 * @param count        Receives the number of entries stored (at most max_entries)
 * @return FN_OK, or FN_ERR_UNSUPPORTED from firmware that predates the command (poll each handle instead)
 */
uint8_t fn_ready(fn_ready_entry_t *entries, uint8_t max_entries, uint8_t *count);

/**
 * @brief Get session information.
 * 
 * For HTTP: returns HTTP status code and content length.
 * For TCP: returns connection state.
 * 
 * @param handle          Session handle
 * @param http_status     Pointer to receive HTTP status (or 0 if N/A)
 * @param content_length  Pointer to receive content length (or 0 if unknown)
 * @param flags           Pointer to receive info flags (FN_INFO_*)
 * @return FN_OK on success, FN_ERR_NOT_READY if info not yet available
 */
uint8_t fn_info(fn_handle_t handle,
                uint16_t *http_status,
                uint32_t *content_length,
                uint8_t *flags);

/**
 * @brief Close a network session.
 * 
 * Releases the session handle and any associated resources.
 * 
 * @param handle     Session handle to close
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_close(fn_handle_t handle);

/**
 * @brief Set the one-shot request body length for the next BBC fn-rom network open.
 *
 * This maps to `OSWORD &78` reason `&01` on BBC and returns
 * `FN_ERR_UNSUPPORTED` on platforms that do not expose an equivalent API.
 */
uint8_t fn_set_body_length(uint16_t len);

/**
 * @brief Set the one-shot request content profile for the next BBC fn-rom network open.
 *
 * This maps to `OSWORD &78` reason `&03` on BBC and returns
 * `FN_ERR_UNSUPPORTED` on platforms that do not expose an equivalent API.
 */
uint8_t fn_set_content_profile(uint8_t profile);

/**
 * @brief Configure JSON translation for an already-open BBC fn-rom network channel.
 *
 * This maps to `OSWORD &78` reason `&00` on BBC and returns
 * `FN_ERR_UNSUPPORTED` on platforms that do not expose an equivalent API.
 */
uint8_t fn_json_query(fn_handle_t handle, const char *path);

/* ============================================================================
 * Application Storage Operations
 * ============================================================================ */

/**
 * @brief Query metadata for a namespaced application storage key.
 *
 * Missing keys return FN_OK with out->exists set to 0.
 */
uint8_t fn_appstore_stat(fn_appstore_io_t *io,
                         const char *namespace_name,
                         const char *key,
                         fn_appstore_stat_t *out);

/**
 * @brief Read bytes from a namespaced application storage key.
 *
 * Missing keys return FN_OK with FN_APPSTORE_READ_EXISTS clear, EOF set, and
 * bytes_read set to 0.
 */
uint8_t fn_appstore_read(fn_appstore_io_t *io,
                         const char *namespace_name,
                         const char *key,
                         uint32_t offset,
                         uint8_t *buf,
                         uint16_t max_len,
                         fn_appstore_read_t *out);

/**
 * @brief Write bytes to a namespaced application storage key.
 *
 * offset==0 creates or replaces the value. Later calls can append or overwrite
 * by using the desired byte offset.
 */
uint8_t fn_appstore_write(fn_appstore_io_t *io,
                          const char *namespace_name,
                          const char *key,
                          uint32_t offset,
                          const uint8_t *data,
                          uint16_t len,
                          fn_appstore_write_t *out);

/**
 * @brief Delete a namespaced application storage key.
 *
 * Missing keys return FN_OK with out->deleted set to 0.
 */
uint8_t fn_appstore_delete(fn_appstore_io_t *io,
                           const char *namespace_name,
                           const char *key,
                           fn_appstore_delete_t *out);

/**
 * @brief List keys in an application storage namespace.
 *
 * key_data receives the raw AppStore list blob: repeated u16 little-endian
 * key length followed by key bytes. Use fn_appstore_list_next_key() to iterate.
 */
uint8_t fn_appstore_list(fn_appstore_io_t *io,
                         const char *namespace_name,
                         uint16_t start_index,
                         uint8_t *key_data,
                         uint16_t key_data_capacity,
                         fn_appstore_list_t *out);

/**
 * @brief Decode the next key from an app-store list key_data blob.
 *
 * Pass offset=0 for the first key. On success, key_out is null-terminated and
 * offset advances to the next encoded key.
 */
uint8_t fn_appstore_list_next_key(const uint8_t *key_data,
                                  uint16_t key_data_len,
                                  uint16_t *offset,
                                  char *key_out,
                                  uint16_t key_out_capacity);

/** @brief Read one sparse slot entry by index. */
uint8_t fn_slot_catalog_get(fn_slot_catalog_io_t *io,
                            uint8_t index,
                            fn_slot_catalog_entry_t *out);

/**
 * @brief Resolve a target and replace one sparse slot entry.
 *
 * Relative targets are resolved by SlotCatalogService against current host
 * state. The returned entry points into the caller-owned I/O buffer.
 */
uint8_t fn_slot_catalog_put(fn_slot_catalog_io_t *io,
                            uint8_t index,
                            uint8_t flags,
                            const char *target,
                            fn_slot_catalog_entry_t *out);

/** @brief Delete one sparse slot entry without renumbering any other entry. */
uint8_t fn_slot_catalog_delete(fn_slot_catalog_io_t *io,
                               uint8_t index,
                               uint8_t *deleted);

/**
 * @brief Read populated entries from an inclusive sparse slot range.
 *
 * max_payload_bytes covers the presence bitmap and complete entry records and
 * must not exceed io->capacity minus the seven-byte response header.
 */
uint8_t fn_slot_catalog_range(fn_slot_catalog_io_t *io,
                              uint8_t lower,
                              uint8_t upper,
                              uint8_t cursor,
                              uint8_t request_flags,
                              uint8_t max_uri_bytes,
                              uint16_t max_payload_bytes,
                              fn_slot_catalog_page_t *out);

/** Decode the next entry from a slot catalogue page. */
uint8_t fn_slot_catalog_next_entry(const fn_slot_catalog_page_t *page,
                                   uint16_t *offset,
                                   fn_slot_catalog_entry_t *out);

/* ============================================================================
 * Mount/Path Resolution
 * ============================================================================ */

uint8_t fn_resolve_mount_target(fn_mount_resolve_io_t *io,
                                const char *host,
                                const char *browse_path,
                                const char *leaf,
                                char *canonical_uri,
                                uint16_t canonical_cap,
                                char *display_path,
                                uint16_t display_cap,
                                uint8_t *flags_out);

uint8_t fn_format_mount_display(fn_mount_resolve_io_t *io,
                                const char *canonical_uri,
                                char *display_path,
                                uint16_t display_cap,
                                uint8_t *flags_out);

/* ============================================================================
 * Clock Operations
 * ============================================================================ */

/**
 * @brief Time format codes for fn_clock_get_format().
 * 
 * These codes match the server-side TimeFormat enum and the legacy
 * fujinet-lib TimeFormat enum for compatibility.
 */
typedef enum {
    /** 7 bytes: [century, year, month, day, hour, min, sec] */
    FN_TIME_FORMAT_SIMPLE     = 0x00,
    
    /** 4 bytes: Apple ProDOS format */
    FN_TIME_FORMAT_PRODOS     = 0x01,
    
    /** 6 bytes: [day, month, year, hour, min, sec] */
    FN_TIME_FORMAT_APETIME    = 0x02,
    
    /** ISO string with TZ offset: "YYYY-MM-DDTHH:MM:SS+HHMM" */
    FN_TIME_FORMAT_TZ_ISO     = 0x03,
    
    /** ISO string UTC: "YYYY-MM-DDTHH:MM:SS+0000" */
    FN_TIME_FORMAT_UTC_ISO    = 0x04,
    
    /** 16 bytes: "YYYYMMDD0HHMMSS000" */
    FN_TIME_FORMAT_APPLE3_SOS = 0x05,
} FnTimeFormat;

/** Maximum formatted time string length (for string formats) */
#ifndef FN_MAX_TIME_STRING
#define FN_MAX_TIME_STRING  32
#endif

/** Maximum timezone string length */
#ifndef FN_MAX_TIMEZONE_LEN
#ifdef __CC65__
#define FN_MAX_TIMEZONE_LEN 32
#else
#define FN_MAX_TIMEZONE_LEN 64
#endif
#endif

/**
 * @brief Get the current time from the FujiNet device.
 * 
 * Returns the current Unix timestamp (seconds since 1970-01-01).
 * 
 * @param time       Pointer to receive the Unix timestamp (8 bytes, little-endian)
 * @return FN_OK on success, FN_ERR_NOT_READY if time not available
 */
uint8_t fn_clock_get(FN_TIME_T *time);

/**
 * @brief Set the time on the FujiNet device.
 * 
 * Sets the device's real-time clock to the specified Unix timestamp.
 * 
 * @param time       Pointer to Unix timestamp to set (8 bytes, little-endian)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_set(const FN_TIME_T *time);

/**
 * @brief Get the current time in a specific format.
 * 
 * Returns the time pre-formatted by the FujiNet device, offloading
 * complex time conversion from the host.
 * 
 * @param time_data   Buffer to receive formatted time (size depends on format)
 * @param format      Desired time format (FnTimeFormat enum)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_get_format(uint8_t *time_data, FnTimeFormat format);

/**
 * @brief Get the current time for a specific timezone without affecting system TZ.
 * 
 * @param time_data   Buffer to receive formatted time
 * @param tz          Timezone string (POSIX format, e.g., "CET-1CEST,M3.5.0,M10.5.0/3")
 * @param format      Desired time format
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_get_tz(uint8_t *time_data, const char *tz, FnTimeFormat format);

/**
 * @brief Get the current timezone string.
 * 
 * @param tz          Buffer to receive timezone string (min FN_MAX_TIMEZONE_LEN bytes)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_get_timezone(char *tz);

/**
 * @brief Set the timezone (non-persistent, runtime only).
 * 
 * @param tz          Timezone string (POSIX format)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_set_timezone(const char *tz);

/**
 * @brief Set the timezone and persist to configuration.
 * 
 * @param tz          Timezone string (POSIX format)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_set_timezone_save(const char *tz);

/**
 * @brief Synchronize time from network (NTP).
 * 
 * Requests the FujiNet device to fetch the current time from NTP servers.
 * Useful after setting time manually to restore correct network time.
 * 
 * @param time       Pointer to receive the new Unix timestamp after sync (8 bytes)
 * @return FN_OK on success, error code on failure
 */
uint8_t fn_clock_sync_network_time(FN_TIME_T *time);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get a human-readable error string.
 * 
 * @param error      Error code
 * @return Pointer to static error string
 */
const char *fn_error_string(uint8_t error);

#ifdef __ATARI__
/**
 * @brief Return the last Atari SIO DSTATS value observed by the transport.
 */
uint8_t fn_atari_last_sio_status(void);

/**
 * @brief Run the OS DCB the caller filled in through the library's SIO entry: the OS SIOV, or the
 * high-speed driver after fn_atari_hsio_enable(). For diagnostics such as timing single frames.
 */
void fn_atari_sio_call(void);
#endif

/**
 * @brief Get the library version string.
 * 
 * @return Pointer to version string (e.g., "1.0.0")
 */
const char *fn_version(void);

#ifdef __cplusplus
}
#endif

#endif /* FUJINET_NIO_H */
