# fujinet-nio-lib

A clean, multi-platform FujiNet NIO client library for communicating with
FujiNet NIO devices using the FujiBus protocol. 6502 (cc65) targets are an
important class of supported machines, not the definition of the library.

## Overview

**fujinet-nio-lib** is a modern C library for applications that need to
communicate with FujiNet NIO devices. It provides:

- **Handle-based API** - Simple, intuitive session management
- **FujiBus protocol** - Binary protocol with shared packet/checksum handling
- **Multi-platform support** - Atari, BBC, Commodore 64, MS-DOS, Amiga, Palm OS, and Linux
- **Multi-compiler support** - CC65, Open Watcom, GCC, amiga-gcc, and prc-tools
- **HTTP and TCP/TLS** - All protocols supported

## Features

- HTTP GET/POST/PUT/DELETE operations
- TCP socket connections
- Application storage key/value API
- Optional legacy fujinet-lib appkey compatibility API
- TLS/HTTPS support (via `tls://` URL scheme)
- Automatic redirect following
- Streaming reads for large responses
- Caller-provided buffers (no hidden malloc)
- Minimal RAM footprint

## Documentation

- **[Developer setup](docs/DEVELOPMENT.md)** — Beebium BBC tests (three env vars)
- **[API Reference](docs/api.md)** - Complete API documentation
- **[Building](docs/building.md)** - Build instructions for all platforms
- **[Transport Backends](docs/transport-backends.md)** - How platform library targets map to NIO channels
- **[Palm OS Springboard](docs/palmos/springboard.md)** - Handspring Visor over a Springboard module's CS1 link
- **[Examples](examples/README.md)** - Example applications and usage

The legacy appkey compatibility functions are available via
`#include "fn_legacy_appkey.h"`. They are compiled into separate archive members
and only link into applications that call the old `fuji_*_appkey()` symbols.

## Quick Start

### Building

```bash
# Build for Linux (native testing)
make linux

# Build all MS-DOS backends
make msdos

# Or build one MS-DOS backend explicitly
make msdos-serial
make msdos-ioctl
make msdos-f5

# Build for Atari
make atari

# Build the normal Amiga application library
make amiga

# Build the resident Amiga driver library variant
make amiga-driver

# Build the Palm OS (Handspring Springboard) library; needs prc-tools and
# sbnio.h from fujinet-nio (see docs/palmos/springboard.md)
make palmos-springboard
```

### Basic HTTP GET

```c
#include "fujinet-nio.h"
#include <stdio.h>

int main(void) {
    fn_handle_t handle;
    uint8_t buffer[512];
    uint16_t bytes_read;
    uint8_t flags;
    
    fn_init();
    
    // Open HTTP connection
    fn_open(&handle, FN_METHOD_GET, "https://example.com/api", 0);
    
    // Read response
    uint32_t offset = 0;
    do {
        fn_read(handle, offset, buffer, sizeof(buffer) - 1, &bytes_read, &flags);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            printf("%s", buffer);
            offset += bytes_read;
        }
    } while (!(flags & FN_READ_EOF));
    
    fn_close(handle);
    return 0;
}
```

### HTTP request headers and larger reads

`fn_open_headers` supplies caller-owned name/value pairs and a known request
body length. For example, after `fn_init()`:

```c
fn_http_header_t headers[] = {
    {"Authorization", "Bearer example-token"},
    {"Content-Type", "application/json"}
};
fn_handle_t handle;
uint8_t rc = fn_open_headers(&handle, FN_METHOD_POST, "https://example.com/api",
                             FN_OPEN_FOLLOW_REDIR, headers, 2, body_length);
/* On FN_OK, send exactly body_length bytes with fn_write(), then read. */
```

The same API works for HTTP and HTTPS. Arbitrary headers and known body lengths
are not supported by BBC's ROM-backed transport, which returns
`FN_ERR_UNSUPPORTED`. Header strings must fit the target's request packet.

On Amiga and other non-cc65 targets, `fn_read` accepts buffers larger than a
packet, splitting requests as needed. It may return a short read; continue at
the returned offset, and retry `FN_ERR_NOT_READY` as appropriate. The largest
single response remains limited by the transport (1024 bytes on Amiga). On
cc65, each `fn_read` returns at most one packet of data.

### TCP/TLS Connection

```c
#include "fujinet-nio.h"

int main(void) {
    fn_handle_t handle;
    uint8_t buffer[256];
    uint16_t bytes_read, written;
    uint8_t flags;
    
    fn_init();
    
    // Connect using URL scheme (tcp:// or tls://)
    fn_open(&handle, 0, "tls://example.com:443", 0);
    
    // Send data
    const char *request = "HELLO\n";
    fn_write(handle, 0, (const uint8_t *)request, 6, &written);
    
    // Receive response
    fn_read(handle, 0, buffer, sizeof(buffer), &bytes_read, &flags);
    
    fn_close(handle);
    return 0;
}
```

## Current Status

**Working:**
- HTTP operations tested and working
- TCP/TLS operations tested and working
- Linux native target for PC-based testing using the common FujiBus/NIO stream transport over a POSIX serial or PTY channel
- MS-DOS builds with Open Watcom and produces serial, `FUJINET.SYS` IOCTL, and INT F5 backend libraries
- BBC target now uses the installed `fn-rom` network ROM through MOS channel opens and `OSWORD &78`
- FujiBus protocol with SLIP framing
- Handle-based session management

**In Progress:**
- Atari SIO transport (assembly implementation complete, needs testing)
- Additional platform transports (Apple II, CoCo, etc.)

**Known Issues:**
- BBC `fn_info()` currently reports connectivity only and does not expose HTTP status/content length yet.
- BBC currently supports GET, PUT, POST, and raw TCP/TLS opens. HEAD and DELETE are not yet mapped through the `fn-rom` MOS-facing open ABI.
- Atari hardware needs testing with a build of fujinet-nio for Atari SIO.

## Platform Support

| Platform | Transport | Compiler | Status |
|----------|-----------|----------|--------|
| Atari 8-bit | SIO | CC65 | ✅ Implemented |
| BBC Micro | `fn-rom` MOS + `OSWORD &78` | CC65 | ✅ Implemented |
| Apple II | SmartPort | CC65 | 🚧 Planned |
| Commodore 64 | IEC | CC65 | 🚧 Planned |
| Tandy CoCo | Drivewire | CMOC | 🚧 Planned |
| MS-DOS | COM serial | Watcom | ✅ Implemented |
| MS-DOS | `FUJINET.SYS` block IOCTL | Watcom | ✅ Implemented |
| MS-DOS | `FUJINET.SYS` INT F5 | Watcom | ✅ Implemented |
| Linux | stream over POSIX serial/PTY | GCC | ✅ Working |
| Amiga applications | `fujinet-nio.device` broker | amiga-gcc | ✅ Working |
| Amiga resident driver | `fujinet-nio.device` broker | amiga-gcc | ✅ Working |
| Palm OS (Handspring Visor) | Springboard module CS1 link | prc-tools | ✅ Working |

## Differences from fujinet-lib

| Feature | fujinet-lib (old) | fujinet-nio-lib (new) |
|---------|-------------------|----------------------|
| Protocol | Legacy SIO | FujiBus binary |
| Addressing | Device specs | Handles |
| Framing | Platform-specific | Shared protocol core; SLIP for stream backends |
| Build | Complex recursive | Simplified explicit |
| Platform code | Mixed with common | Clean separation |
| TLS | HTTP only | HTTP and raw TCP |

## License

GPL v3 - See LICENSE file for details.

## Contributing

Contributions are welcome! Please ensure:

1. Code follows the existing style
2. Platform-specific code goes in `src/platform/<platform>/`
3. Common code goes in `src/common/`
4. All functions are documented with Doxygen-style comments

## Related Projects

- [fujinet-nio](https://github.com/FujiNetWIFI/fujinet-nio) - FujiNet firmware rewrite
- [fujinet-lib](https://github.com/FujiNetWIFI/fujinet-lib) - Legacy library (reference)
- [cc65](https://github.com/cc65/cc65) - 6502 C compiler

## Small Atari TCP clients

The optional [Atari small-stream API](docs/atari-small-stream.md) trades general
service coverage and throughput for a compact 6502 client with application-owned
buffers. Read its interrupt/audio, partial-transfer and retry responsibilities
before choosing it over the general API. It requires version 1 capable firmware.
