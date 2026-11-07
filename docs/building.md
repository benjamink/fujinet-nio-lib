# Building fujinet-nio-lib

This document describes how to build the fujinet-nio-lib library for various platforms.

## Requirements

### For Atari builds:
- CC65 compiler suite (cl65, ar65)
- GNU Make

### For CoCo builds:
- CMOC compiler
- LWTOOLS (lwar, lwasm)
- GNU Make

### For MS-DOS builds:
- Open Watcom (wcc, wlib)
- GNU Make

### For Linux / BBC builds (native testing):
- GCC compiler
- GNU Make
- See [Developer setup](DEVELOPMENT.md) for Beebium integration test prerequisites

### For Amiga builds:
- [amiga-gcc](https://github.com/bebbo/amiga-gcc), including
  `m68k-amigaos-gcc`, `m68k-amigaos-ar`, readable NDK headers, `amiga.lib`,
  and clib2
- GNU Make
- The toolchain binaries on `PATH`; for a typical `/opt/amiga` installation:

  ```bash
  export PATH=/opt/amiga/bin:$PATH
  ```

### For Palm OS builds:
- [prc-tools-remix](https://github.com/jichu4n/prc-tools-remix)
  (`m68k-palmos-gcc` 2.95.3, `m68k-palmos-ar`) and the Palm OS 3.5 SDK.
  Its `tools/setup-palm-os-dev-env.sh` installs both on Debian; the Docker
  image in [mozzwald/fujinet-palm](https://github.com/mozzwald/fujinet-palm)
  (`docker/`) has them ready:

  ```bash
  ../fujinet-palm/docker/palm-build.sh make palmos-serial
  ```
- `sbnio.h` from a `fujinet-nio` checkout for `palmos-springboard`
  (`bridges/rp2350-springboard/include`), found as `../fujinet-nio` unless
  `SBNIO_ABI_INCLUDE` says otherwise; see [Palm OS Springboard](palmos/springboard.md)

## Building

### Build all targets:
```bash
make
```

### Build for a specific platform:
```bash
make atari      # Atari 8-bit
make c64        # Commodore 64
make apple2     # Apple II
make coco       # Tandy CoCo
make msdos      # all MS-DOS backend libraries
make msdos-serial # MS-DOS direct COM backend
make msdos-ioctl  # MS-DOS FUJINET.SYS IOCTL backend
make msdos-f5     # MS-DOS FUJINET.SYS INT F5 backend
make linux      # Native Linux (for testing)
make amiga      # Amiga application archive
make amiga-driver # Resident-driver archive (no application atexit cleanup)
make palmos-serial # Palm OS 3.1+ cradle serial port
make palmos-springboard # Palm OS Springboard CS1 link
```

### Clean build artifacts:
```bash
make clean
```

## Build Output

### Preparing a release

With the workspace environment and all configured toolchains available:

```sh
python3 scripts/release.py --suggest
python3 scripts/release.py 0.1.0
```

The suggestion uses reachable version tags, conventional `feat` and
breaking-change markers, and common feature verbs; inspect it rather than treating it as a compatibility
guarantee. Preparation requires a clean committed tree, runs `make check`,
exports the committed source and the committed Amiga driver ABI, and writes
versioned ZIPs for Atari compact and original, BBC, BBC clib, all three MS-DOS
transports, Amiga application and driver, and Linux. `RELEASE_NOTES.md` and
`CHANGELOG.md` list every non-merge commit and a diffstat; `SHA256SUMS` and
per-package manifests record artifact and build provenance. The first release
uses all prior history; `--since <tag>` selects another notes baseline.
The release tag and package manifests are the version authority; there is no
separate source-tree version file to update by hand.

Publishing is explicit: push the tested commit to `origin/master`, then run
`python3 scripts/release.py 0.1.0 --publish`. It verifies the remote branch,
creates a draft GitHub release with the generated notes and assets, then
publishes it, downloads the assets to verify them, and fetches the tag locally.
An existing prepared output is checked against the current
commit, generated notes and package checksums before it is reused.
Each release gets a random adjective-noun name, shown as `name (version)` in its GitHub title and
stored in `release.json` during preparation so review and publication use the
same name. Pass `--name "Your name"` during preparation to choose one instead;
an already prepared release keeps its existing name. The GitHub release titles
are the shared history: preparation checks all releases and accessible drafts,
and publication checks again. Names are compared without regard to case, including
custom names. Preparing a release therefore requires authenticated `gh` and network
access; it fails rather than guessing if the history cannot be read. If a name
chosen during preparation is subsequently taken, prepare a new release with a
different name. Deleting or renaming a GitHub release removes its original name
from this history. Avoid concurrent publishers, since checking and creating a
release are separate operations. The version remains the release tag.
Without `--push`, the process does not push commits or update workspace submodule pins.
For a single command from a clean `master`, add `--push` to `--publish`; it
pushes the checked commit after preparation and before publishing. It never
force-pushes and does not update workspace submodule pins.

Set `--driver /path/to/fujinet-nio-driver` if the Amiga driver is not a sibling
checkout. Its committed HEAD is exported and recorded in Amiga manifests.
Atari PBI HSIO get/set requires compatible FujiNet-NIO firmware.

### Standalone Atari package

Build a package from a committed revision (uncommitted changes are excluded):

```sh
python3 scripts/package_atari.py --revision HEAD --version 0.1.0 \
    --backend compact --output build/fujinet-nio-atari-0.1.0.zip
```

`--backend original` selects the original transport instead. The ZIP includes
headers, the archive, licence notices and a manifest identifying the source
commit, backend, compiler and archive SHA-256. The script prints the ZIP's
SHA-256. A development package is not a published release; a release should
be made from a reviewed, tested commit and carry the appropriate version.
The ZIP member order and timestamps are fixed, but the cc65 archive currently
contains build-dependent metadata, so independently rebuilt ZIPs need not be
byte-identical. Verify the published ZIP against its recorded checksum.

The build produces the following outputs:

```
fujinet-nio-lib/
  build/
    fujinet-nio-atari.lib    # Atari library
    fujinet-nio-linux.a      # Linux static library
    fujinet-nio-apple2.lib   # Apple II library (planned)
    fujinet-nio-coco.lib     # CoCo library (planned)
    fujinet-nio-msdos-serial.lib # MS-DOS direct COM backend
    fujinet-nio-msdos-ioctl.lib  # MS-DOS FUJINET.SYS IOCTL backend
    fujinet-nio-msdos-f5.lib     # MS-DOS FUJINET.SYS INT F5 backend
    fujinet-nio-amiga.a          # Amiga application library
    fujinet-nio-amiga-driver.a   # Amiga resident-driver library
    fujinet-nio-palmos-serial.a  # Palm OS cradle serial library
    fujinet-nio-palmos-springboard.a # Palm OS Springboard library
```

## Palm OS

`make palmos-serial` builds for Palm OS 3.1 and later. It opens the cradle
serial port with the New Serial Manager on Palm OS 3.3 and later, and with the
older serial library before that (3.1, 3.2), and talks FujiBus on it at 115200
baud through the common stream transport
(`src/platform/palmos/fn_channel_serial.c`). Like `amiga-driver`, it is
outside the top-level `make` loop but built by `make check`.

- gcc 2.95 has no `<stdint.h>` or `<string.h>`; `include/palmos/` supplies
  both. Apps add `-Iinclude/palmos` from this tree to their include path, and
  `-DFN_MAX_PACKET_SIZE=512` to match the library.
- `int` is 16 bits, as on cc65.
- FujiNet NIO can lend the cradle line to HotSync while the bus is idle
  (`docs/hotsync.md` in fujinet-nio, "Cradle"). The target sets
  `FN_STREAM_WAKE_IDLE_MS`, so the first request after 30 s of quiet is
  preceded by FujiDevice GetInfo, retried briefly until FujiNet answers.
  Requests themselves are still sent once.

## Amiga variants

`make amiga` builds the normal application library. It uses process-exit
cleanup suitable for CLI programs.

`make amiga-driver` builds the variant consumed by resident Exec devices. It
defines `FN_AMIGA_EXPLICIT_LIFECYCLE`, so the device owns transport lifetime
instead of registering application `atexit()` cleanup. Both variants expose
the same typed DiskDevice context API and RS-232 FujiBus session behavior.

The `amiga-driver` target is intentionally available explicitly but is not
part of the top-level `make` all-target loop; ordinary users do not need the
resident archive unless they are building an Exec device.

## Palm OS variant

`make palmos-springboard` builds the Handspring Visor library with prc-tools.
Like `amiga-driver` it is in `EXTRA_TARGETS`: `make check` builds it, but the
default `make` loop does not, because it needs prc-tools and `sbnio.h` from
`fujinet-nio`. Pass `SBNIO_ABI_INCLUDE=-I<dir>` if `../fujinet-nio` is not
where the header lives. Applications also add `-Iinclude/palmos` for the
`stdint.h` and `string.h` that prc-tools lacks. See
[Palm OS Springboard](palmos/springboard.md).

## Linux Native Testing

The Linux target allows you to build and test applications natively on your PC,
communicating with a FujiNet-NIO device via a POSIX serial or PTY byte channel.
It uses the same common stream transport as MS-DOS.

### Setting up the connection

Set the `FN_PORT` environment variable to specify the serial device:

```bash
# For ESP32 via USB-serial
export FN_PORT=/dev/ttyUSB0

# For POSIX fujinet-nio via PTY
export FN_PORT=/dev/pts/2

# Optionally set baud rate (default: 115200)
export FN_BAUD=115200
```

### Building a test application

```bash
# Build the library
make linux

# Compile your application
gcc -o my_test my_test.c -I include -L build -l:libfujinet-nio-linux.a

# Run with the serial port
FN_PORT=/dev/ttyUSB0 ./my_test
```

## Cross-Compilation Notes

### Atari (CC65)

The library uses the CC65 compiler's `-t atari` target. Key considerations:

- No floating point support
- No large stack allocations (use static buffers)
- No mixed declarations and code (C89 style)
- Limited standard library

### Platform-Specific Code

Platform-specific transport code is located in:
- `src/platform/atari/` - Atari SIO transport
- `src/platform/bbc/` - BBC Micro `fn-rom` backed MOS/OSWORD wrappers
- `src/platform/apple2/` - Apple II SmartPort (planned)
- `src/platform/coco/` - CoCo Drivewire (planned)
- `src/platform/msdos/` - MS-DOS serial, IOCTL, and F5 backends
- `src/platform/linux/` - Linux/POSIX serial or PTY byte channel
- `src/platform/amiga/` - Amiga `fujinet-nio.device` broker client
- `src/platform/palmos/` - Palm OS: the cradle serial port (New Serial Manager, or the serial library before Palm OS 3.3) and the Springboard CS1 link client
- `include/palmos/` - `stdint.h` and `string.h` for prc-tools

See [Transport Backends](transport-backends.md) for how library build targets map
to `fujinet-nio` transports/channels, and how future MS-DOS parallel or BBC
user-bus/1 MHz bus backends should be added.

### MS-DOS target specifics

The MS-DOS target uses Open Watcom and produces three backend archives:

- `fujinet-nio-msdos-serial.lib`: direct 8250-compatible UART access. The
  application owns the COM port.
- `fujinet-nio-msdos-ioctl.lib`: DOS `INT 21h AH=44h` block-device IOCTL to the
  NIO build of `FUJINET.SYS`. The resident driver owns the COM port.
- `fujinet-nio-msdos-f5.lib`: DOS `INT F5` backend to the NIO build of
  `FUJINET.SYS`. The resident driver owns the COM port.

All three archives use the same public headers and common FujiBus packet,
response parsing, network, clock, raw-call, and session code. The serial archive
also uses the common SLIP stream transport; the IOCTL and F5 archives adapt the
common FujiBus request packet to resident-driver control calls.

By default the transport uses COM1 at 115200 baud. You can override this at
compile time with target C flags, for example:

```bash
make msdos-serial TARGET_CFLAGS_msdos-serial="-DFN_MSDOS_COM=2 -DFN_MSDOS_BAUD_DIVISOR=12"
```

Common divisors are 1 for 115200, 2 for 57600, 6 for 19200, and 12 for 9600.

### BBC target specifics

The BBC target now assumes the `fn-rom` network ROM is installed on the machine. The library no longer owns raw RS423 setup, SLIP framing, or FujiBus packet construction on BBC. Instead it:

- opens network sessions through BBC MOS channel opens
- uses `OSWORD &78` for long URLs, JSON translation, and request-body helpers
- links a much smaller BBC-specific implementation instead of the common direct-transport stack

Current BBC limitations:

- `fn_info()` only reports basic connectivity state
- `HEAD` and `DELETE` are not yet supported on BBC
- clock APIs currently return `FN_ERR_UNSUPPORTED` on BBC

Current BBC example-build workaround:

- the cc65 BBC runtime currently leaves `initenv` unresolved in a default app link
- the bundled example build injects a tiny no-op `initenv` shim object from `src/platform/bbc/initenv.s`
- this is a toolchain/runtime integration workaround, not part of the FujiNet API surface

## Directory Structure

```
fujinet-nio-lib/
├── Makefile              # Top-level build
├── makefiles/
│   ├── build.mk          # Core build logic
│   ├── targets.mk        # Platform definitions
│   ├── compiler.mk       # Compiler selection
│   ├── compiler-cc65.mk  # CC65 config
│   ├── compiler-cmoc.mk  # CMOC config
│   └── compiler-wcc.mk   # Watcom config
├── include/
│   ├── fujinet-nio.h     # Main API header
│   ├── fn_protocol.h     # Protocol definitions
│   └── fn_platform.h     # Platform interface
├── src/
│   ├── common/
│   │   ├── fn_slip.c     # SLIP encoding
│   │   ├── fn_packet.c   # Packet building
│   │   └── fn_network.c  # API implementation
│   └── platform/
│       ├── atari/        # Atari SIO transport
│       ├── apple2/       # Apple II SmartPort
│       ├── coco/         # CoCo Drivewire
│       └── msdos/        # MS-DOS TCP/serial
└── examples/
    ├── Makefile          # Examples build
    └── network/          # Network examples
```
