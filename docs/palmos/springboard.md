# Palm OS over a Springboard module

The `palmos-springboard` target builds `fujinet-nio-lib` for a Handspring Visor
(Palm OS 3.5, m68k) that talks to `fujinet-nio` through an RP2350 module in the
Springboard slot. The Palm never sees a serial port: it reads and writes the
module's memory-mapped registers, and the module's firmware carries each
FujiBus packet to NIO over USB.

```
Visor app + fujinet-nio-lib  --Springboard CS1-->  RP2350 firmware  --USB, packet link-->  fujinet-nio
       raw FujiBus packet       16-bit MOVE.W        owns the NIO link                    FujiBus services
```

This is a packet-native backend, like the Atari SIO one: there is no SLIP, byte
channel or `fn_transport_stream.c` on the Palm. All packet building, checksums,
parsing and sessions are the common code.

## The CS1 link

The Springboard slot has two chip selects. CS0 is the module's ROM (the
firmware serves the apps from it). CS1, which sits right above the CS0 window
(`CsBase + CsSize` from Handspring's `HsCardAttrGet`), is the link. The
register map, the check word and the result codes are the link's ABI,
`sbnio.h`; read it there rather than here.

Per request the library:

1. writes `SBN_W_BEGIN` with the packet length, the packet two bytes per
   `SBN_W_DATA` word (first byte in the high half, an odd last byte padded with
   0), then `SBN_W_CHECK` with the `SBN_CHECK_STEP` sum of those words;
2. writes `SBN_W_DOORBELL` with a sequence 1-255 and polls `SBN_R_RESP_SEQ`
   until it equals that sequence (the firmware publishes it last);
3. reads `SBN_R_RESULT` and, if it is `SBN_RES_OK`, the reply words from
   `SBN_R_RESP`, checking them against `SBN_R_RESP_CHECK`.

Retry rules, all in `fn_palmos_exchange()`:

- `SBN_RES_BAD_CHECK` means the firmware found the request damaged and did not
  send it on, so the request is sent again with a new sequence, up to three
  times.
- A reply that fails its check is read again (it stays in place), up to three
  more times. The request is never sent again for that.
- If no reply appears within 35 seconds (the firmware itself gives up on NIO
  after 30) the exchange fails without a resend, because NIO may already have
  acted on the request. This is the same no-replay rule as `fn_raw_call()`.

The first sequence after the module is found continues from the firmware's
last `SBN_R_RESP_SEQ`, so a reply left over from an earlier program never
matches.

### Transport results

| Link result | `fn_*` result |
|-------------|---------------|
| `SBN_RES_OK` | `FN_OK` |
| `SBN_RES_NO_HOST` (NIO is not running) | `FN_ERR_TRANSPORT` |
| `SBN_RES_TIMEOUT` (NIO sent nothing back), `SBN_RES_UNANSWERED` (NIO took it but gave no answer), or no reply published in 35 s | `FN_ERR_TIMEOUT` |
| `SBN_RES_BAD_LEN` | `FN_ERR_INVALID` |
| `SBN_RES_ABORTED` | `FN_ERR_ABORTED` |
| `SBN_RES_BAD_CHECK` three times, `SBN_RES_TOO_BIG`, `SBN_RES_REFUSED` (NIO's link refused it unrun), reply never read back right | `FN_ERR_IO` |
| no module open | `FN_ERR_NOT_FOUND` |

A reply longer than the caller's buffer is cut to it; the common code sizes its
buffers by `FN_MAX_PACKET_SIZE` (1024 here). The link itself carries up to
`SBN_REQ_MAX` / `SBN_RESP_MAX` bytes.

### Finding the module

`fn_init()` calls `fn_transport_init()`, which looks at cards 1-3 for one that
is removable and installed, takes `CsBase + CsSize` as CS1 and accepts it if
`SBN_R_MAGIC` and `SBN_R_VERSION` match `sbnio.h`. Without a module it returns
`FN_ERR_NOT_FOUND`. `fn_shutdown()` closes the link, so `fn_shutdown()` followed
by `fn_init()` looks again, for example after the module was pulled and
reinserted.

The lookup uses Handspring's `HsCardAttrGet` trap, so the library runs on
Handspring devices only.

## Diagnostics

Applications normally call only `fn_init()` and the usual `fn_*` calls. Tools
that need the link itself get these, declared in `fujinet-nio.h` for
`__palmos__`:

| Call | Purpose |
|------|---------|
| `fn_palmos_open()`, `fn_palmos_close()` | find the module / forget it |
| `fn_palmos_cs1()`, `fn_palmos_cs0()`, `fn_palmos_card()` | where it was found |
| `fn_palmos_read(offset)`, `fn_palmos_write(reg, value)` | one raw register access |
| `fn_palmos_exchange(...)` | one request and reply; returns `SBN_RES_*`, `FN_PALMOS_NO_ANSWER` or `FN_PALMOS_BAD_READ` |
| `fn_palmos_log(text)` | one line to the firmware console |

Each is its own object in the archive, so an application that does not call
them does not link them.

## Building

The compiler is prc-tools (`m68k-palmos-gcc` 2.95.3) with the Palm OS 3.5 SDK
headers: prc-tools-remix plus a Palm OS SDK set up with
`palmdev-prep -d sdk-3.5`. The simplest way to get that is the Docker image in
[mozzwald/fujinet-palm](https://github.com/mozzwald/fujinet-palm) (`docker/`:
a `Dockerfile` and `palm-build.sh`, which mounts the checkout's parent
directory so sibling checkouts resolve inside the container). The image is
amd64-only and runs under emulation on Apple Silicon.

The library needs `sbnio.h` from `fujinet-nio`
(`bridges/rp2350-springboard/include`). It is not copied into this repository.
By default the build looks for a sibling checkout, `../fujinet-nio`; point
`SBNIO_ABI_INCLUDE` elsewhere if yours is not there:

```bash
# any shell with m68k-palmos-gcc on PATH
make palmos-springboard
make palmos-springboard SBNIO_ABI_INCLUDE=-I/path/to/fujinet-nio/bridges/rp2350-springboard/include

# or through mozzwald's container, from this checkout
../fujinet-palm/docker/palm-build.sh make palmos-springboard
```

The result is `build/fujinet-nio-palmos-springboard.a`. The target is part of
`make check` (`EXTRA_TARGETS`) but not of the default `make`.

An application compiles and links against it like this:

```bash
m68k-palmos-gcc -O2 -Wall -palmos3.5 \
    -Iinclude -Iinclude/palmos app.c build/fujinet-nio-palmos-springboard.a -o app
build-prc -o app.prc app.def app *.bin
```

Add the same `-I...rp2350-springboard/include` if the application uses the
`SBN_*` register names for the diagnostic calls.

### prc-tools constraints

- `include/palmos/` supplies `stdint.h` and `string.h`, which the toolchain
  lacks; `fujinet-nio.h` needs the first. Put it on the include path as above.
- `int` is 16 bits. Use the `uint8_t`/`uint16_t`/`uint32_t` types and
  `(uint32_t)` casts for anything over 16 bits.
- gcc 2.95 accepts neither `-std=c99` nor `-Wextra`, rejects `-MMD -MF` (the
  build moves the `-MMD` output itself), and does not allow declarations after
  statements.
- Palm applications keep their globals in a small heap. The library's share is
  two `FN_MAX_PACKET_SIZE` buffers plus `FN_MAX_SESSIONS` session records; it
  allocates nothing else.

## Tests

`make test-palmos-transport` builds the platform sources on the host with a
stub `PalmOS.h` and a fake module behind the CS1 bus macros
(`FN_PALMOS_HOST_TEST`). The fake plays the firmware: it checks the request
words against the check word, and can damage requests or replies, stay silent,
answer late, or report each error result. The test covers the probe, the
sequence numbers, odd-length padding in both directions, truncation to the
caller's buffer, resend and re-read limits, the result mapping, the 35 second
wait, and `fn_init()`/`fn_raw_call()`/`fn_shutdown()` through the real
library. It needs `sbnio.h` the same way the build does
(`SBNIO_ABI_INCLUDE`). The real build keeps bare volatile `MOVE.W` accesses in
the copy loops; the macros only change under `FN_PALMOS_HOST_TEST`.

Nothing here replaces a run on hardware: the host test cannot show the bus
timing or the `HsCardAttrGet` trap.
