# Installed Palm OS NioLib

`NioLib.prc` is a Palm OS 3.5 system shared library containing the common NIO
packet/service code and three Palm-private backends. Applications link the small
`libniolib-client.a` adapter instead of embedding a complete NIO implementation.
The adapter exposes familiar `fn_*` calls and checks the installed library's ABI
and capabilities. `NioLibTest.prc` provides offline and networking diagnostics;
NIO Config (the `nio-config` repository) selects the transport.

```text
Palm app → client archive → installed NioLib → one selected transport
  Springboard CS1 → RP2350 → separate packet link → NIO services
  Serial cradle  → SLIP stream backend          → NIO services
  USB cradle     → HsUs Generic 2 → USB Host     → NIO services
```

The USB cradle path bypasses RP2350. All three reuse existing FujiBus commands
and services. This guide describes the installed library; the separate static
`palmos-springboard` and `palmos-serial` archives remain supported as documented
in [building](building.md) and [Springboard](palmos/springboard.md).

## Build

Requirements: prc-tools GCC 2.95.3, Palm OS SDK 3.5, PilRC, build-prc 2.3, Python 3,
Handspring SDK `Include/UsbMgr.h`, and `sbnio.h` from
`fujinet-nio/bridges/rp2350-springboard/include`. SDK headers are external inputs,
not vendored. Use the existing `fujinet-palm-springboard-tools` Docker image
(see the [Palm toolchain reference](palmos/springboard.md#building)); Apple
Silicon needs emulation for that amd64 image.

From this repository with the tools on PATH:

```sh
make palmos-shared PALMOS_SHARED_BUILD=build/palmos-shared-usb \
  SBNIO_ABI_INCLUDE=-I../fujinet-nio/bridges/rp2350-springboard/include \
  HANDSPRING_INCLUDE=-I/path/to/Handspring/Include
```

Or, from a parent directory containing sibling `fujinet-nio-lib` and
`fujinet-nio` checkouts, with `HANDSPRING_SDK` set to the SDK directory containing
`Include/`:

```sh
docker run --rm --network none --user "$(id -u):$(id -g)" \
  -v "$PWD:/work" -v "$HANDSPRING_SDK:/handspring:ro" \
  -w /work/fujinet-nio-lib fujinet-palm-springboard-tools \
  make palmos-shared PALMOS_SHARED_BUILD=build/palmos-shared-usb
```

The target builds **NioLib.prc**, **NioLibTest.prc** and **libniolib-client.a**
in that output directory, with maps, symbols, disassembly and `verification.json`.
Use a new output directory when preserving qualified binaries. No historical
bundle, Springboard ROM rebuild or firmware flash is required.

## Install and use

Install the PRCs into **internal Palm RAM** using ordinary HotSync. NioLibTest
and NIO Config are ordinary applications; NioLib is a `libr` database and has
no launcher icon. Installing NIO Config is optional for applications that use
the default Springboard transport.

Before replacing or downgrading NioLib, end all dependent applications normally,
return Home and verify that the library is unloaded: NioLibTest's offline test
checks safe lifecycle/removal; complete it and return Home without opening
another NIO application. A registered/in-use library or retained driver resources
must be released before replacement. Do not force-delete a loaded database or
use HotSync to overwrite code still registered with Palm OS. If release fails,
stop and preserve the error rather than resetting or reinstalling blindly.
Keep previous PRCs for rollback, using the same idle-library preflight.

Select a transport in NIO Config and Save before launching a client. Settings
apply to the next foreground session, without fallback. Run NioLibTest offline
without any device; use its Network page for Clock + two HTTP handles when a
backend is available. Return Home normally to release the client and handles.

Applications include `NioLibClient.h`, link only the client archive and balance
`fn_init()` with `fn_shutdown()` on normal success/error exits. For example,
with the toolchain on PATH:

```sh
m68k-palmos-gcc -O2 -palmos3.5 \
  -I../fujinet-nio-lib/include/palmos -I../fujinet-nio-lib/include \
  app.c ../fujinet-nio-lib/build/palmos-shared-usb/libniolib-client.a -o app
```

Package the application resources with its own build-prc target. Do not link a
complete static NIO archive alongside the client archive. Existing applications
using earlier compatible traps need no rebuild merely to select USB.

## Application contract

Current identity is **NioLib / libr / NiL1**, ABI **1.4**, implementation **7**.
See [ABI reference](palmos-shared-abi.md) for capabilities, traps and preferences.
The client supports networking libraries from ABI 1.1 onward, negotiates optional
functions, and returns Unsupported instead of calling an unknown JSON trap.
NioConfig needs ABI 1.2 settings support; USB additionally needs capability `0x20`.

Supported client operations are `fn_init`, `fn_shutdown`, `fn_is_ready`,
`fn_open`, `fn_open_translated`, `fn_info`, `fn_read`, `fn_write`, `fn_close`,
`fn_ready`, `fn_json_query`, `fn_clock_get` and `fn_clock_get_timezone`.
This is a subset of the generic NIO-LIB API, not an export of every service.

One foreground application owns a session, identified by its Palm database and
a token. Multiple network handles belong to that owner; capacity is returned
by BeginClient, not fixed in the public ABI. Calls are synchronous and nested
calls return BUSY. No sessions persist across normal app switching and no
background networking is provided. EndClient attempts remote handle closure
before transport shutdown. A failed cleanup reports uncertainty; it does not
prove that every remote side effect was undone.

`fn_is_ready()` means the initialized selected local transport is usable, not
Internet connectivity. ABI 1.1 uses its Springboard-specific Probe fallback;
newer libraries use TransportStatus. `Probe.modulePresent` retains its literal
Springboard meaning. Direct traps return Palm `Err` separately from `FN_*`
service results; the client translates these and exposes LastError/LastResult
and the cleanup report through `NioLibClient.h`.

## Transports

| ID | Backend and hardware | Framing |
| --- | --- | --- |
| 1 | Handspring Springboard CS1, RP2350 module and its separate NIO host link | Raw canonical FujiBus packets over 16-bit registers |
| 2 | Physical serial cradle/cable and an NIO SLIP backend | Common stream session + SLIP |
| 3 | Handspring USB cradle, HsUs library and a compatible USB Host backend | Raw canonical FujiBus packets on Generic Port 2 |

Springboard is the default when no saved preference exists. The backend is
fixed at BeginClient until EndClient. Configuration/status inspection and saving
do not open ports or send network traffic. Invalid saved settings are reported
as safe display defaults but prevent initialization until explicitly repaired.

**Springboard** validates Handspring support and card mapping instead of using
a hardcoded physical address. Its shared-only guards preserve 16-bit bus
operations, request/reply sequences and a pending-sequence tombstone, with
mapping/deadline checks during access. It does not change RP2350 timing or ROM.

**Serial** uses New Serial Manager where present and falls back to the legacy
Serial Library. It selects the cradle port, 8N1, 9600/19200/38400/57600/115200 baud
(default 115200), with no flow control or RTS/CTS. It reuses `fn_stream_session_t`
and SLIP. A 1024-byte packet needs at most 2050 encoded bytes, including two END
bytes. Port, installed RX buffer, TX storage and acquired legacy references are
tracked independently. No keyboard-daemon takeover or HotSync arbitration is
implemented; Handspring-specific physical serial behavior remains unqualified.

**USB** finds/loads Handspring's `USB Library` / `HsUs`, checks the required
counted API dispatch range and explicitly opens Generic Port 2 (`0x0002`), not
HotSync (`0x0202`). Partial counted Send/Receive calls and ReceiveCheck/SendCheck
polls use bounded deadlines. A library-owned TX buffer stays alive while the
driver can reference it; no caller buffer is retained after a trap. Missing,
busy or unsupported APIs return errors. Only references this backend acquired
are eligible for removal. The peer is the optional POSIX libusb profile in
`fujinet-nio`, documented there in
`docs/visor-usb-host.md`. USB needs no Springboard module or firmware changes.

All shared backends have a **1024-byte complete raw packet limit**, including
headers, status and service metadata. Common Read requests reserve 22 bytes,
so each response payload request is at most **1002 bytes**; larger caller reads
can span packets. Oversized or inconsistent replies fail instead of truncating.
Serial's SLIP limit does not reduce CS1/USB packet capacity.

## Network and JSON reads

Open/Info/Read/Write/Close, Ready and Clock reuse the common NIO implementation,
packet IDs and validation. Handles are checked against the foreground owner's
local session table. Response identity, length, checksum and relevant
handle/offset metadata are checked before results reach application buffers.

For JSON, open once with `fn_open_translated()` and read the selected value.
ABI 1.4 adds `fn_json_query(handle, selector)`: it sends the existing
NetworkService **TranslateConfigure (0x07)** on that same handle. The NIO host
retains the completed HTTP body and applies subsequent selectors without a
new GET. The Palm does not contain a new JSON parser or a cross-session cache.
Each selector resets that handle's translated read offset to zero; other
handles are unchanged. Closing the handle or ending the client releases it.

A query returning `FN_ERR_NOT_READY` has applied the selector but is still
waiting for the first body. Poll Read at offset zero; do not repeat configuration,
which can discard partial input. Selector strings are bounded to 1011 bytes.
Use ABI >=1.4 and JSON capability `0x40`; older libraries return Unsupported.
The host must support existing TranslateConfigure cached-body semantics.

The shared ISS/Weather shim retains one handle per JSON document and closes it
after extracting its fields. Compiled app tests show one GET per ISS refresh
and four per Weather startup (location plus three existing weather URLs).
The user reports correct fields and much faster loading with the new apps and
library. No percentage or measured timing improvement is claimed. This is not
request coalescing across URLs, persistent app caching or asynchronous fetching.

## Globals, cleanup and recovery

The system library is packaged with `build-prc -L`: `libr/0` contains the entry
and dispatch table; `nDat/0` contains a validated initialized-data/BSS/fixup image.
`-mown-gp` and `SYSLIB_POINTER_ENTRYPOINT` establish library-owned A4 via
`SysLibTblEntry(refNum)->globalsP`, preserving the caller's A4/A5. First Open
allocates system-owned storage, initializes data/BSS and relocates data/code
pointers. The serializer rejects unsupported fixups, caller-A5 dependencies,
unresolved helpers and code/global images above its 32767-byte limits.

External library Open counts, foreground ownership, physical driver ownership
and unresolved-command markers are separate. EndClient releases foreground
ownership even when physical teardown reports BUSY. If idle final Close cannot
release USB/serial resources, it still consumes the app's Open and returns a
nonzero remaining count as a library residency hold. Do not free/remove it.
A later first Open or idle BeginClient makes one bounded cleanup attempt;
confirmed release permits reuse without the previous app's A5 state. RX buffer
restoration, TX abort and port Close errors retain storage until actual driver
release. Legacy `serErrStillOpen` consumes only the acquired reference.

Ordinary operations have a 35-second budget; EndClient has a two-second total
polling budget. These cannot preempt a faulty ROM trap that itself does not
return. USB/serial ambiguity markers and CS1 pending-sequence state survive
library unload independently of physical ownership. Successful validated USB/
serial replies clear only their own exchange marker; cleanup does not clear an
unresolved one. Ambiguous Open/Write/Close is never replayed automatically.
There is no marker-clear button or general physical recovery guarantee. Stop
on recovery-required errors, preserve both ends' errors/logs and establish
physical teardown before considering a separately reviewed recovery procedure.

## Verification and limits

[Testing and bench guidance](palmos-shared-testing.md) describes image/ABI audits,
compiled m68k tests with mocked Palm traps, optional app fixtures and safe
operator tools. These validate actual generated code, not full Palm OS behavior.

On a real Visor Prism, Linux USB Generic 2 passed Clock + two HTTP and
ISS → Weather → ISS; switching to Springboard passed Clock + two HTTP again.
Post-audit corrections passed USB Clock/HTTP, ISS/Weather and CS1 Clock/HTTP,
with host logs showing no remaining network handles. The later ABI 1.4 JSON
library/apps separately passed user testing with faster loading; that report
does not establish a repeat of the entire transport sequence for that revision.

Serial hardware, macOS compile/runtime, ESP32-S3 USB Host, general Palm-branded
USB, arbitrary physical fault recovery and long-duration heap/stress use remain
unqualified. ESP32-S3 USB Host is future work: it would be an alternative to the
RP2350 Springboard configuration, not a simultaneous connection or role switch.
