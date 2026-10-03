# API Reference

This document provides a complete reference for the fujinet-nio-lib API.

## Initialization

### `fn_init()`

Initialize the library and transport layer.

```c
uint8_t fn_init(void);
```

**Returns:** `FN_OK` on success, error code on failure.

**Example:**
```c
uint8_t result = fn_init();
if (result != FN_OK) {
    printf("Init failed: %s\n", fn_error_string(result));
    return 1;
}
```

### `fn_is_ready()`

Check if the FujiNet device is ready for communication.

```c
uint8_t fn_is_ready(void);
```

**Returns:** Non-zero if device is ready, 0 if not.

## Raw FujiBus/NIO Calls

### `fn_raw_call()`

Send one FujiBus/NIO request to an arbitrary NIO device and return the device
status plus payload.

```c
#include "fn_raw.h"

uint8_t fn_raw_call(uint8_t device,
                    uint8_t command,
                    const void *payload,
                    uint16_t payload_length,
                    void *reply,
                    uint16_t reply_capacity,
                    fn_raw_response_t *response);
```

This is for service-specific callers such as DiskService, FileService, and
FujiNet control tools. It uses the same selected backend as the normal
network-session API:

- serial backend: builds a FujiBus packet, SLIP-frames it, and sends it over the
  COM byte channel;
- MS-DOS IOCTL backend: builds the same FujiBus request, forwards device,
  command, and payload through `FUJINET.SYS` `NIO_CALL`, then reconstructs a
  normal FujiBus response for the common parser;
- MS-DOS F5 backend: currently returns `FN_ERR_UNSUPPORTED`.

`fn_raw_call()` initializes the library on first use if needed.

**Parameters:**
- `device` - NIO device ID, such as `FN_DEVICE_DISK`, `FN_DEVICE_FILE`, or
  `FN_DEVICE_NETWORK`
- `command` - command byte for that device
- `payload` / `payload_length` - request payload bytes after the FujiBus header
- `reply` / `reply_capacity` - caller buffer for response payload bytes after
  the returned status byte
- `response` - output status and payload length

**Returns:** `FN_OK` if the transport exchange completed and `response->status`
contains the device status. Returns a library transport/error code if the
request could not be exchanged or parsed.

## DiskDevice client codec

The typed `fn_disk_*` API wraps the generic DiskDevice v1 protocol at wire
device `0xFC`. It provides Mount, Unmount, Info, ClearChanged, ReadSector, and
WriteSector operations without exposing payload layouts to callers. See
[`disk_device_client.md`](disk_device_client.md) for the complete codec
contract and phase-one Amiga profile.

## Application Storage

The app-store API talks to fujinet-nio's dedicated AppStore service (`0xF1`).
It provides namespaced key/value storage for application preferences and state.
Values are opaque bytes and can be larger than one FujiBus packet by using
offset-based chunked reads and writes.

Namespaces and keys are UTF-8 byte strings from 1 to 255 bytes. They are not
filesystem paths; fujinet-nio owns the backing storage layout.

## Legacy Appkeys

`fn_legacy_appkey.h` provides the old fujinet-lib appkey function names for
applications or compatibility layers that already call:

```c
void fuji_set_appkey_details(uint16_t creator_id,
                             uint8_t app_id,
                             enum AppKeySize keysize);
bool fuji_read_appkey(uint8_t key_id, uint16_t *count, uint8_t *data);
bool fuji_write_appkey(uint8_t key_id, uint16_t count, uint8_t *data);
```

This is intentionally a compatibility layer over FileDevice filesystem calls,
not a migration into the new app-store API. It reads and writes the same legacy
key-file layout used by older FujiNet firmware:

```text
persist:///FujiNet/<creator-id><app-id><key-id>.key
```

For example, creator `0xfe0c`, app `0x01`, key `0x01` becomes:

```text
persist:///FujiNet/fe0c0101.key
```

The `persist:///` scheme is resolved by fujinet-nio to the platform's default
persistent filesystem, such as `host` on POSIX/embedded Altirra and `sd0` or
`flash` on ESP32. This keeps client code platform agnostic while preserving the
legacy on-disk format. `fuji_write_appkey()` creates `persist:///FujiNet` before
writing, so a new persistent store starts empty but usable.

These functions live in separate `src/legacy` archive members. Normal
applications do not link them unless they reference the legacy symbols. The BBC
ROM-backed target does not currently include this layer because it does not use
the common raw File service call path.

App-store calls use caller-owned scratch storage so 8-bit applications can
choose the smallest practical buffer and can share it with other temporary
storage when lifetimes do not overlap.

```c
uint8_t appstore_scratch[320];
fn_appstore_io_t appstore_io = {
    appstore_scratch,
    sizeof(appstore_scratch)
};
```

The scratch buffer is used for both request and response data. It may also be
used as the destination buffer for reads. Do not use it as the source `data`
buffer for writes, because the request header is built in scratch before the
payload is copied.

### `fn_appstore_stat()`

Query metadata for a key.

```c
uint8_t fn_appstore_stat(fn_appstore_io_t *io,
                         const char *namespace_name,
                         const char *key,
                         fn_appstore_stat_t *out);
```

Missing keys return `FN_OK` with `out->exists == 0`.
`fn_appstore_stat_t` exposes 64-bit size and mtime as low/high 32-bit words:
`size_bytes`, `size_bytes_high`, `mtime_unix`, and `mtime_unix_high`.

### `fn_appstore_read()`

Read one chunk from a key.

```c
uint8_t fn_appstore_read(fn_appstore_io_t *io,
                         const char *namespace_name,
                         const char *key,
                         uint32_t offset,
                         uint8_t *buf,
                         uint16_t max_len,
                         fn_appstore_read_t *out);
```

Check `out->flags` for:

- `FN_APPSTORE_READ_EXISTS`
- `FN_APPSTORE_READ_EOF`

Example:

```c
uint8_t buf[256];
uint32_t offset = 0;
fn_appstore_read_t rr;

do {
    result = fn_appstore_read(&appstore_io, "config-ng", "colour.preference",
                              offset, buf, sizeof(buf), &rr);
    if (result != FN_OK || !(rr.flags & FN_APPSTORE_READ_EXISTS)) {
        break;
    }
    /* process buf[0..rr.bytes_read) */
    offset += rr.bytes_read;
} while (!(rr.flags & FN_APPSTORE_READ_EOF) && rr.bytes_read != 0);
```

### `fn_appstore_write()`

Write one chunk to a key.

```c
uint8_t fn_appstore_write(fn_appstore_io_t *io,
                          const char *namespace_name,
                          const char *key,
                          uint32_t offset,
                          const uint8_t *data,
                          uint16_t len,
                          fn_appstore_write_t *out);
```

`offset == 0` creates or replaces the value. Use later offsets for append or
overwrite chunks.

### `fn_appstore_delete()`

Delete a key.

```c
uint8_t fn_appstore_delete(fn_appstore_io_t *io,
                           const char *namespace_name,
                           const char *key,
                           fn_appstore_delete_t *out);
```

Missing keys return `FN_OK` with `out->deleted == 0`.

### `fn_appstore_list()`

List keys in a namespace.

```c
uint8_t fn_appstore_list(fn_appstore_io_t *io,
                         const char *namespace_name,
                         uint16_t start_index,
                         uint8_t *key_data,
                         uint16_t key_data_capacity,
                         fn_appstore_list_t *out);
```

`key_data` receives the raw key list blob: repeated `u16` little-endian key
length followed by key bytes. Use `fn_appstore_list_next_key()` to iterate it.

```c
uint8_t key_data[420];
char key[128];
uint16_t start = 0;

do {
    uint16_t off = 0;
    uint16_t i;
    result = fn_appstore_list(&appstore_io, "config-ng", start, key_data, sizeof(key_data), &lr);
    if (result != FN_OK) {
        break;
    }
    for (i = 0; i < lr.key_count; i++) {
        result = fn_appstore_list_next_key(key_data, lr.key_data_len,
                                           &off, key, sizeof(key));
        if (result == FN_OK) {
            puts(key);
        }
    }
    start += lr.key_count;
} while (lr.flags & FN_APPSTORE_LIST_MORE);
```

## Network Operations

### Protocol Behavior

When you call `fn_open()`, the FujiNet device determines the protocol type from the URL scheme and returns **protocol capability flags**. The client library uses these flags internally to enforce correct offset behavior:

| Protocol | Flags | Offset Behavior |
|----------|-------|-----------------|
| HTTP/HTTPS | `0x00` | Random-access reads allowed (can request any offset) |
| TCP/TLS | `0x07` | Sequential offsets required (must track total bytes read/written) |

**Why this matters:**

- **HTTP**: You can request any offset (e.g., resume a download from byte 1000). The server may support range requests.
- **TCP/TLS**: Offsets must be strictly sequential. The library tracks `read_offset` and `write_offset` internally and validates that your offset matches the expected cursor position.

**Application code pattern:**

For all protocols, the recommended pattern is to track total bytes read/written and pass that as the offset:

```c
uint32_t total_read = 0;
do {
    result = fn_read(handle, total_read, buffer, sizeof(buffer), &bytes_read, &flags);
    if (result == FN_OK && bytes_read > 0) {
        // Process data...
        total_read += bytes_read;  // Track for next offset
    }
} while (result == FN_OK && !(flags & FN_READ_EOF));
```

This pattern works for both HTTP and TCP/TLS. For HTTP, the offset is informational; for TCP/TLS, it's enforced.

### `fn_open()`

Open a network connection to a URL.

```c
uint8_t fn_open(fn_handle_t *handle, 
                uint8_t method,
                const char *url,
                uint8_t flags);
```

**Parameters:**
- `handle` - Output pointer for the session handle
- `method` - HTTP method (`FN_METHOD_GET`, `FN_METHOD_POST`, etc.) or 0 for raw TCP/TLS
- `url` - URL to connect to (e.g., `http://example.com`, `tcp://host:port`, `tls://host:port`)
- `flags` - Optional flags (`FN_OPEN_FOLLOW_REDIR`, `FN_OPEN_ALLOW_EVICT`, `FN_OPEN_STREAM_NO_PROBE`)

**Returns:** `FN_OK` on success, error code on failure.

**URL Schemes:**
- `http://` - HTTP connection
- `https://` - HTTPS connection (TLS)
- `tcp://` - Raw TCP connection
- `tls://` - Raw TLS connection

**Scheme authority:**
- The URL scheme is authoritative. Use `https://` and `tls://` explicitly when secure transport is required.
- `FN_OPEN_TLS` is a legacy compatibility flag and should not be used for new code.

**BBC target notes:**
- The BBC implementation is backed by `fn-rom` and BBC MOS channel semantics.
- `fn_open()` on BBC is the small/common short-URL path.
- URLs longer than the cc65 BBC open-path limit must use `fn_open_long()`, which routes through `OSWORD &78` reason `&04` and the `"://"` sentinel open path.
- `FN_METHOD_HEAD` and `FN_METHOD_DELETE` currently return `FN_ERR_UNSUPPORTED` on BBC because the current `fn-rom` MOS-facing open ABI maps cleanly to GET, PUT, POST, and raw stream opens only.

**Example:**
```c
fn_handle_t handle;
uint8_t result = fn_open(&handle, FN_METHOD_GET, "https://example.com/api", 0);
```

### `fn_tcp_open()`

Open a TCP connection to a host and port (convenience function).

```c
uint8_t fn_tcp_open(fn_handle_t *handle,
                    const char *host,
                    uint16_t port);
```

**Parameters:**
- `handle` - Output pointer for the session handle
- `host` - Hostname or IP address
- `port` - Port number

**Returns:** `FN_OK` on success, error code on failure.

### `fn_open_long()`

Open a network connection using the explicit long-URL path.

```c
uint8_t fn_open_long(fn_handle_t *handle,
                     uint8_t method,
                     const char *url,
                     uint8_t flags);
```

Use this when an application intentionally needs long URLs and wants to opt in to
the larger/open-specialized path on BBC. On other platforms this reduces to
`fn_open()`.

### `fn_open_translated()`

Open a network connection and ask the firmware to translate the content.

```c
uint8_t fn_open_translated(fn_handle_t *handle,
                           uint8_t method,
                           const char *url,
                           uint8_t flags,
                           uint8_t translation_type,
                           uint8_t translation_flags,
                           const char *selector);
```

`translation_type` is `FN_TRANSLATE_NONE` (0), `FN_TRANSLATE_JSON` (1) or
`FN_TRANSLATE_IMAGE` (4). For any type other than `FN_TRANSLATE_NONE` the Open
request carries the extension block (`u32 openExtFlags = 1`, `u8 type`,
`u8 flags`, `u16 LE selectorLen`, selector bytes). With `FN_TRANSLATE_NONE`,
the packet is byte-identical to `fn_open()`. Example image selector:
`"w=624,h=190,colors=12,base=4,par=1:2"`.

On BBC, any translation type other than `FN_TRANSLATE_NONE` returns
`FN_ERR_UNSUPPORTED`.

### `fn_read()`

Read data from an open connection.

```c
uint8_t fn_read(fn_handle_t handle,
                uint32_t offset,
                uint8_t *buf,
                uint16_t max_len,
                uint16_t *bytes_read,
                uint8_t *flags);
```

**Parameters:**
- `handle` - Session handle from `fn_open()`
- `offset` - Read offset (typically total bytes read so far)
- `buf` - Buffer to receive data
- `max_len` - Maximum bytes to read
- `bytes_read` - Output: actual bytes read
- `flags` - Output: read flags (`FN_READ_EOF`, etc.)

**Returns:** `FN_OK` on success, `FN_ERR_NOT_READY` if no data available, error code on failure.

**Read Flags:**
- `FN_READ_EOF` - End of stream reached
- `FN_READ_TRUNCATED` - Response filled the supplied caller buffer
- `FN_READ_MORE_AVAILABLE` - Additional bytes are already immediately available after this chunk

**Streaming no-probe mode:**
- `FN_OPEN_STREAM_NO_PROBE` is an opt-in streaming policy for framed application protocols.
- When enabled on a streaming session, `fn_read()` returns the current buffered chunk without forcing an extra probe read solely to determine whether more bytes are immediately available.
- This is useful when the application protocol carries its own message length or framing and the caller prefers one read round trip per chunk.
- Default behavior remains probe-compatible for existing callers.

**Example:**
```c
uint8_t buffer[512];
uint16_t bytes_read;
uint8_t flags;
uint32_t total = 0;

do {
    result = fn_read(handle, total, buffer, sizeof(buffer) - 1, &bytes_read, &flags);
    if (result == FN_OK && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
        total += bytes_read;
    }
} while (result == FN_OK && !(flags & FN_READ_EOF));
```

### `fn_write()`

Write data to an open connection.

```c
uint8_t fn_write(fn_handle_t handle,
                 uint32_t offset,
                 const uint8_t *data,
                 uint16_t len,
                 uint16_t *written);
```

**Parameters:**
- `handle` - Session handle from `fn_open()`
- `offset` - Write offset (typically total bytes written so far)
- `data` - Data to write
- `len` - Length of data
- `written` - Output: actual bytes written

**Returns:** `FN_OK` on success, error code on failure.

`fn_write()` is completion-oriented: it retries transient `FN_ERR_NOT_READY` /
`FN_ERR_BUSY` responses and continues partial writes internally until all
requested bytes have been accepted or a non-transient error occurs. This hides
asynchronous TCP connect completion from applications after `fn_open()`.

**Half-Close:** To signal end of write (send FIN), call with `len=0` at current offset:
```c
// After writing all data, half-close the write side
fn_write(handle, total_written, NULL, 0, &dummy);
```

**BBC target note:** zero-length writes are treated as a no-op. The current BBC `fn-rom` backed path does not expose a true half-close signal through this library API.

### `fn_info()`

Get information about an open connection.

```c
uint8_t fn_info(fn_handle_t handle,
                uint16_t *http_status,
                uint32_t *content_length,
                uint8_t *flags);
```

**Parameters:**
- `handle` - Session handle from `fn_open()`
- `http_status` - Output: HTTP status code (if applicable)
- `content_length` - Output: Content-Length (if available)
- `flags` - Output: info flags

**Info Flags:**
- `FN_INFO_HAS_STATUS` - HTTP status is valid
- `FN_INFO_HAS_LENGTH` - Content-Length is valid
- `FN_INFO_CONNECTED` - Connection is established
- `FN_INFO_PEER_CLOSED` - Peer has closed their side

### `fn_close()`

Close a connection and free the handle.

```c
uint8_t fn_close(fn_handle_t handle);
```

**Parameters:**
- `handle` - Session handle to close

**Returns:** `FN_OK` on success, error code on failure.

### `fn_set_body_length()`

Set the one-shot body length for the next network open on BBC.

```c
uint8_t fn_set_body_length(uint16_t len);
```

**Returns:** `FN_OK` on BBC when accepted by `fn-rom`, `FN_ERR_UNSUPPORTED` on other targets.

### `fn_set_content_profile()`

Set the one-shot request content profile for the next network open on BBC.

```c
uint8_t fn_set_content_profile(uint8_t profile);
```

**Profiles:**
- `FN_CONTENT_PROFILE_NONE`
- `FN_CONTENT_PROFILE_JSON`
- `FN_CONTENT_PROFILE_FORM`
- `FN_CONTENT_PROFILE_TEXT`

**Returns:** `FN_OK` on BBC when accepted by `fn-rom`, `FN_ERR_UNSUPPORTED` on other targets.

### `fn_json_query()`

Configure JSON translation on an already-open BBC network channel.

```c
uint8_t fn_json_query(fn_handle_t handle, const char *path);
```

After a successful call, subsequent `fn_read()` calls return the translated JSON match rather than the raw body bytes, mirroring the `fn-rom` `*FJSON` / `OSWORD &78` behavior.

**Returns:** `FN_OK` on BBC when accepted by `fn-rom`, `FN_ERR_UNSUPPORTED` on other targets.

## Utilities

### `fn_error_string()`

Get a human-readable error string.

```c
const char *fn_error_string(uint8_t error);
```

**Parameters:**
- `error` - Error code from a failed operation

**Returns:** Static string describing the error.

### `fn_version()`

Get the library version string.

```c
const char *fn_version(void);
```

**Returns:** Version string (e.g., "1.0.0").

## Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0x00 | `FN_OK` | Success |
| 0x01 | `FN_ERR_NOT_FOUND` | Device not found |
| 0x02 | `FN_ERR_INVALID` | Invalid parameter or malformed request |
| 0x03 | `FN_ERR_BUSY` | Device is busy, retry later |
| 0x04 | `FN_ERR_NOT_READY` | Operation not ready, poll again |
| 0x05 | `FN_ERR_IO` | I/O error during operation |
| 0x06 | `FN_ERR_TIMEOUT` | Device did not respond in time |
| 0x07 | `FN_ERR_INTERNAL` | Internal error |
| 0x08 | `FN_ERR_UNSUPPORTED` | Operation not supported |
| 0x10 | `FN_ERR_TRANSPORT` | Transport layer error |
| 0x11 | `FN_ERR_URL_TOO_LONG` | URL exceeds maximum length |
| 0x12 | `FN_ERR_NO_HANDLES` | No free handles available |
| 0x13 | `FN_ERR_ABORTED` | Request aborted locally (client-visible FN error; not a FujiBus wire status) |
| 0xFF | `FN_ERR_UNKNOWN` | Unknown/unexpected error |

## HTTP Methods

| Code | Name |
|------|------|
| 0x01 | `FN_METHOD_GET` |
| 0x02 | `FN_METHOD_POST` |
| 0x03 | `FN_METHOD_PUT` |
| 0x04 | `FN_METHOD_DELETE` |
| 0x05 | `FN_METHOD_HEAD` |

## Open Flags

| Flag | Value | Description |
|------|-------|-------------|
| `FN_OPEN_TLS` | 0x01 | Use TLS/HTTPS (for URLs without scheme) |
| `FN_OPEN_FOLLOW_REDIR` | 0x02 | Follow HTTP redirects |
| `FN_OPEN_ALLOW_EVICT` | 0x04 | Allow handle eviction under memory pressure |

## Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `FN_MAX_URL_LEN` | 256 | Maximum URL length |
| `FN_MAX_SESSIONS` | 3 on cc65, 4 otherwise | Maximum concurrent sessions |
| `FN_MAX_CHUNK_SIZE` | 512 | Maximum read/write chunk size |

## Protocol Capability Flags

These flags are returned by the server in the `fn_open()` response and used internally by the library. Applications typically don't need to check these directly, but understanding them helps explain offset behavior:

| Flag | Value | Description |
|------|-------|-------------|
| `FN_PROTO_FLAG_SEQUENTIAL_READ` | 0x01 | Reads must use sequential offsets (TCP/TLS) |
| `FN_PROTO_FLAG_SEQUENTIAL_WRITE` | 0x02 | Writes must use sequential offsets (TCP/TLS) |
| `FN_PROTO_FLAG_STREAMING` | 0x04 | Protocol is streaming, not request/response |

**Protocol flag values:**
- HTTP/HTTPS: `0x00` (random-access, no sequential requirement)
- TCP/TLS: `0x07` (streaming, sequential read/write required)

## Types

### `fn_handle_t`

Session handle type. Opaque handle returned by `fn_open()` and `fn_tcp_open()`.

```c
typedef uint8_t fn_handle_t;
```
