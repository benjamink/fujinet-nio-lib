# Transport Backends

`fujinet-nio` firmware can expose the FujiBus/NIO protocol through multiple
transports and channels. `fujinet-nio-lib` is narrower by design: one library
build selects one channel at compile time.

That channel can be any physical path that can carry the selected transport's
framing. The public library API is not serial-specific, and the common stream
transport is not COM-port-specific or POSIX-specific. The currently implemented
MS-DOS channel is COM serial; the currently implemented Linux channel is POSIX
serial/PTY.

## Library Layers

The library has four practical layers:

1. Public API: `fn_open`, `fn_read`, `fn_write`, `fn_close`, clock helpers, and
   platform extension helpers.
2. Common protocol core: packet construction, response parsing, session state,
   and checksum handling.
3. Transport core: frame exchange over a class of channels. The current common
   stream transport owns SLIP framing and calls byte-channel functions.
4. Channel/backend: the target-specific implementation of
   `fn_channel_init`, `fn_channel_ready`, `fn_channel_read_byte`,
   `fn_channel_write_byte`, `fn_channel_drain_rx`, and `fn_channel_close`, or a
   target-specific implementation of the higher-level transport functions.
   The shared stream session also has an optional bulk raw-byte write callback:
   it submits an already-SLIP-encoded frame in one physical operation while
   retaining ordered byte-stream semantics. This is a stream optimization, not
   packet-native FujiBus transport and not a change to the public API.

For direct transports that cannot sensibly share `fn_transport_stream.c`, a
platform can still implement
   `fn_transport_init`, `fn_transport_ready`, `fn_transport_exchange`, and
   `fn_platform_name` directly.

The channel/backend must not duplicate FujiBus packet construction, checksums,
response parsing, or application-level service helpers. Byte-stream backends own
physical I/O only. Non-stream backends, such as the MS-DOS IOCTL backend, own
only the adaptation from the common FujiBus request packet to their resident
driver ABI.

## Current Backends

| Target | Backend | Notes |
|--------|---------|-------|
| `linux` | common stream transport over POSIX serial/PTY channel | Runtime device path through `FN_PORT`. |
| `msdos` | alias that builds all MS-DOS backend libraries | Produces serial, IOCTL, and F5 archives. |
| `msdos-serial` | common stream transport over COM serial channel | Direct 8250-compatible UART access. Defaults to COM1 at 115200 baud. |
| `msdos-ioctl` | DOS block-device IOCTL backend | Sends raw NIO calls through a loaded `FUJINET.SYS`; the driver owns COM1. |
| `msdos-f5` | INT F5 backend | Sends raw NIO calls through the NIO build of `FUJINET.SYS`; the driver owns COM1. |
| `bbc`, `bbc-clib` | `fn-rom`/MOS channels | Does not use the common direct transport stack. BBC delegates transport details to `fn-rom`. |
| `atari` | SIO | Direct target-specific transport. |
| `c64` | IEC serial bus | Direct target-specific transport, through the KERNAL's bus routines. |
| `palmos-serial` | common stream transport over the Palm OS cradle serial port | New Serial Manager at 115200 baud, or the serial library before Palm OS 3.3. Reclaims the line after an idle spell (`FN_STREAM_WAKE_IDLE_MS`) because NIO may lend it to HotSync. |
| `palmos-springboard` | Springboard module CS1 link | Packet-native; the module firmware owns the link to NIO. |

## Atari SIO Backend Notes

The Atari target is a direct packet-native SIO transport, not the old firmware
application protocol and not a stream channel. Application code builds normal
FujiBus request packets; the backend exchanges each unmodified packet at SIO
device `0x7F` through native `W`, `P`, and `R` commands. There is no SLIP,
padding, or segmentation.

v1 has one outstanding request and a 512-byte baseline. `W` is accepted only
while idle, `P` receives its fixed three-byte state/length record, and `R`
uses the exact ready length. The optional `fn_atari_query_capabilities()`
issues fixed-size `Q`; ordinary calls do not probe. Timeout retains the
outstanding request, and later traffic polls and consumes it before sending a
new request; the backend never replays an application request automatically.
PROC notification is opt-in: `fn_atari_proc_notify()` only latches a flag for
an application-owned handler, and foreground code still polls `P`. `fn_atari_proc_enable()` is the ready-made
handler (it counts, latches and chains the previous `VPRCED`; `fn_atari_proc_pending()` and
`fn_atari_proc_count()` read it), linked only by a program that calls it. The firmware does not assert PROC
for the protocol on its own: with `sio.tune proc 1` it pulses PROC when a TCP session of this client becomes
readable and sets `Q` flag bit 1 (`FN_ATARI_CAP_PROC`), so a program can enable the handler and call
`fn_read` when `fn_atari_proc_pending()` says so instead of in a loop. See
`repos/fujinet-nio/docs/atari/proc.md`.

**Reading without the poll.** `P` exists only to learn the length of the ready response, and the firmware
runs the request inside the `W`, so the response is ready when `W` completes. When `Q` reports flag bit 0
(`R` may ask for more than the response; the reply is zero padded and the response released as for an exact
read), a call that can bound its response (`fn_disk_read_sector`: the sector capacity plus at most 21 bytes of
framing) issues `R` for that bound straight after `W` and takes the true packet length from the FujiBus
header, saving a transaction (about 4.6 ms at 108,800 baud). A shorter `R` is still refused. The bound is set
through `fn_reply_hint` around the raw call; transports without the flag ignore it. `fn_atari_hsio_enable()`
queries `Q` after switching to the high-speed driver, so the flag is known when a program turns high speed
on; other programs can call `fn_atari_query_capabilities()`.

When running through Altirra/AltirraSDL's NetSIO custom device, the bridge-facing
side also has to follow the Atari SIO completion sequence precisely:

- SIO write command ACK response size is the payload size plus the checksum
  byte.
- After accepting the Atari write payload and checksum, the device sends ACK and
  then the final SIO complete byte (`'C'`).
- SIO read is ACK, SIO complete byte (`'C'`), the exact number of data bytes the
  Atari requested, and then the SIO checksum byte.
- FujiBus responses must be queued until the Atari issues the matching SIO read
  command. Sending the response immediately when fujinet-nio writes to the
  channel is too early for NetSIO.
- AltirraSDL may miss the initial high-speed setting if it is sent before real
  SIO traffic begins, so the NetSIO bridge repeats the 19200 baud speed-change
  message when the first SIO command is observed.
- NetSIO credit/update frames are connection-management traffic; they must not
  be treated as an active SIO command.

The library sends one complete packet-native `W` and reads one complete,
ready-length `R`; a bridge must neither chunk, pad, nor SLIP-frame either
payload. A queued response remains available until the matching exact-length
`R` is accepted.

## Palm OS Springboard Backend Notes

The `palmos-springboard` target is a packet-native backend like Atari SIO. The
Handspring Visor writes each FujiBus request packet, unframed, into an RP2350
module's CS1 registers with 16-bit `MOVE.W` cycles and reads the reply packet
back the same way. The module firmware owns the link to NIO (NIO's packet link
over USB), so the library has no byte channel and does not use
`fn_transport_stream.c`.

There is one outstanding request. Every request carries a check word and the
firmware refuses a damaged one without forwarding it, so the client may send it
again; a reply that fails its check is read again from the same place. A
request that gets no reply is not sent again, since NIO may have acted on it.
The register map and result codes are the shared ABI header `sbnio.h`, which
lives in `fujinet-nio` and is not copied here. See
[Palm OS Springboard](palmos/springboard.md).

## Commodore 64 IEC Backend Notes

The C64 target exchanges each unmodified FujiBus packet with IEC device 30 on
channel 0, using the KERNAL's own LISTEN, SECOND, CIOUT, UNLSN, TALK, TKSA, ACPTR
and UNTLK. UNLISTEN ends a request packet and the device runs it; the response is
read with TALK up to the length in its own header. There is no SLIP, padding or
segmentation, and one request is outstanding at a time. The bus contract is
`docs/iec/nio-channel.md` in the fujinet-nio repository.

Strings stay in ASCII. The library and its programs compile with `cc65 -t none`
and `-D__C64__ -D__CBM__`, then link against `c64.lib`, because cc65's `c64` target
would translate every literal to PETSCII. A C64 program converts only at its own
screen and keyboard.

The device holds the bus while a request runs, as a drive does while it seeks, so
a local timeout does not exist: a slow request is a slow first byte.

## MS-DOS Backends

MS-DOS has two practical runtime models:

- `msdos-serial`: the application owns COM1/COM2 directly. This is useful for
  standalone diagnostics and for machines where no resident DOS FujiNet driver
  is loaded.
- `msdos-ioctl`: the application talks to the resident NIO build of
  `FUJINET.SYS` using DOS `INT 21h AH=44h` block-device IOCTL. The driver owns
  the serial port, so applications can make network/FileService calls while also
  reading files from FujiNet-provided DOS drives.
- `msdos-f5`: the application talks to the same resident NIO build of
  `FUJINET.SYS` using `INT F5`. This follows the firmware-style application
  entry point while still using the NIO service protocol behind the driver.

The IOCTL backend does not make network traffic "block sized". DOS passes a
control buffer and byte count to the block driver. `fujinet-nio-lib` builds the
same FujiBus request packet as the serial backend, then the IOCTL backend
extracts the device, command, and payload and forwards them to `FUJINET.SYS`
using the driver's `NIO_CALL` control request. The backend reconstructs a normal
FujiBus response packet so the common response parser is still shared.

The `msdos-f5` backend uses `AX=F502h` with `ES:BX` pointing at the same `FUJI`
NIO control block used by the IOCTL backend. `FUJINET.SYS` returns the NIO
status and serial diagnostics in that block, and the library reconstructs the
normal FujiBus response packet so common response parsing is shared.

The `msdos-serial` target selects the COM serial channel because
`src/platform/msdos/fn_channel_serial.c` implements 8250-compatible byte I/O.
That is a build-target/channel choice, not a public API or transport-core
limitation. The same common protocol and stream transport code can support a
future MS-DOS parallel-port channel if that channel implements the byte-channel
interface and presents the same ordered byte-stream semantics.

## Why Not Runtime Transport Selection?

Runtime channel selection is possible on larger targets, but it should not be the
default model for this repository.

Most supported machines are small. A runtime channel table, multiple compiled
channels, extra configuration parsing, and channel-specific persistent buffers
would increase code size and RAM pressure. On BBC in particular, archive member
boundaries and BSS usage are part of correctness because unused code and hidden
buffers can make otherwise small applications fail.

For that reason, the preferred model is compile-time channel selection.

## Recommended Future Shape

Add new physical channels as named build targets, each linking one channel
backend:

| Future target | Platform directory | Backend |
|---------------|--------------------|---------|
| `msdos-serial` | `src/platform/msdos/` or `src/platform/msdos_serial/` | stream transport over COM/RS-232 |
| `msdos-ioctl` | `src/platform/msdos/` | resident `FUJINET.SYS` block IOCTL backend |
| `msdos-f5` | `src/platform/msdos/` | resident `FUJINET.SYS` INT F5 backend |
| `msdos-parallel` | `src/platform/msdos_parallel/` | stream transport over PC parallel port |
| `bbc`, `bbc-clib` | `src/platform/bbc/` | `fn-rom`/MOS channel facade |
| `bbc-userbus` | `src/platform/bbc_userbus/` | Direct BBC user-bus backend |
| `bbc-1mhzbus` | `src/platform/bbc_1mhzbus/` | Direct BBC 1 MHz bus backend |

The target name should describe the host platform and the physical channel when
there is more than one plausible backend for a platform.

## Adding An MS-DOS Parallel Backend

The least disruptive route is:

1. Add a target mapping in `makefiles/targets.mk`, for example:

   ```make
   PLATFORM_msdos-parallel := msdos_parallel
   COMPILER_FAMILY_msdos-parallel := wcc
   TRANSPORT_FAMILY_msdos-parallel := stream
   TARGET_CFLAGS_msdos-parallel :=
   TARGET_ASFLAGS_msdos-parallel :=
   ```

2. Add `msdos-parallel` to `TARGETS` only when it is ready to build by default.
3. Create `src/platform/msdos_parallel/fn_channel_parallel.c`.
4. Implement the byte-channel entry points:

   ```c
   uint8_t fn_channel_init(void);
   uint8_t fn_channel_ready(void);
   uint8_t fn_channel_write_byte(uint8_t value, uint16_t timeout_ms);
   uint8_t fn_channel_read_byte(uint8_t *value, uint16_t timeout_ms);
   void fn_channel_drain_rx(void);
   void fn_channel_close(void);
   ```

5. Implement `fn_platform_name()` for the target.
6. Keep packet construction, checksums, SLIP framing, response parsing, sessions,
   clock, and network API behavior in `src/common/`.
7. Keep any parallel-port handshaking, timing, IRQ, status-register, or BIOS
   fallback logic inside `src/platform/msdos_parallel/`.

If the parallel channel can provide ordered byte read/write operations, it can
reuse `fn_transport_stream.c` unchanged. If the channel is packet-oriented and
does not need SLIP on the host side, add a different transport family rather than
putting packet protocol knowledge into the channel implementation.

## POSIX Channel Examples

USB serial on a Raspberry Pi is not a different channel in this model. It is the
existing POSIX serial/PTY channel with a different device path, such as
`/dev/ttyUSB0` or `/dev/ttyACM0`.

Useful POSIX channel variants would be:

- `linux-tcp`: a TCP socket byte channel for emulator, CI, or a local
  `fujinet-nio` process exposing a byte-stream endpoint.
- `linux-gpio` or `linux-spi`: a Raspberry Pi channel for an adapter connected
  through GPIO or SPI rather than USB serial.
- `linux-libusb`: a userspace USB endpoint channel if a future adapter exposes a
  vendor-specific USB protocol instead of USB CDC serial.

All of these can reuse `fn_transport_stream.c` if they provide ordered byte
read/write semantics. Only the channel implementation should change.

## Adding BBC User-Bus Or 1 MHz Bus Backends

BBC is different from MS-DOS because the current library target intentionally
delegates transport and channel handling to `fn-rom`. A direct user-bus or 1 MHz
bus backend should therefore be a separate target rather than a hidden mode
inside the existing `bbc` target.

The recommended approach is:

1. Keep `bbc` and `bbc-clib` as ROM-backed targets.
2. Add explicit targets such as `bbc-userbus` or `bbc-1mhzbus`.
3. Put direct bus integration under a separate platform directory such as
   `src/platform/bbc_userbus/`.
4. Decide whether the new BBC target can afford the common protocol and stream
   transport stack. If it cannot, mirror the existing BBC approach and provide
   small platform-specific implementations of the public APIs that call a
   resident ROM or small bus shim.
5. Use map analysis before accepting the backend as practical for real BBC
   applications.

This keeps the existing BBC target small and predictable while leaving room for
NIO firmware channels beyond the current ROM/MOS integration.

## Design Rule

`fujinet-nio` decides which transports and channels exist. `fujinet-nio-lib`
decides which one channel a particular application binary is built to speak.

When a platform needs more than one physical channel, prefer another target and
another channel directory over runtime selection. Reuse the common transport
family where the channel semantics match; write only the channel layer for new
byte-stream channels.
