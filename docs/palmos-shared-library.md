# Installed Palm OS NioLib

`NioLib.prc` is a Palm OS system shared library built with SDK 3.1 by default
(SDK 3.5 is optional), containing the common NIO
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

Use the separate [palmdev-toolchain](https://github.com/mozzwald/palmdev-toolchain)
repository, reviewed at commit `4161cf9`. Its pinned tools are
[prc-tools-remix](https://github.com/mozzwald/prc-tools-remix) 2.3.6 (GCC
2.95.3-kgpd), [PilRC](https://github.com/mozzwald/pilrc) 3.2.91 and Python 3.
Build-prc retains its component banner 2.3 and PilRC its older banner 3.2.90.
The common image name is `palmdev-toolchain:local`. Compiler installation and
image recipes live there; NioLib never clones or installs tools automatically.

One-time explicit setup, alongside the NIO repositories:

```sh
git clone https://github.com/mozzwald/palmdev-toolchain ../palmdev-toolchain
git -C ../palmdev-toolchain checkout 4161cf9
(cd ../palmdev-toolchain && ./setup.sh)
```

Select Palm SDK 3.1 and the optional Handspring SDK; install SDK 3.5 too if you
want the optional build. Review each agreement. SDK sources/revisions and
acceptance are managed by the toolchain, not this library. The selected SDK
is mounted read-only; it is not part of the image. Linux amd64 is tested;
macOS/ARM use the wrapper's amd64 container support and still need validation.

Handspring's `UsbMgr.h` is also available in `Handspring.zip` on
[PalmDB's Palm OS SDK page](https://palmdb.net/app/palm-os-sdk). Its EULA
restricts redistribution: do not vendor the SDK or publish it in the image.
The toolchain installer currently obtains its pinned Handspring SDK from
[the SDK mirror](https://github.com/mozzwald/handspring-sdk) and records local
agreement acceptance; it does not download the PalmDB archive. The required
layout is `Include/UsbMgr.h`. The wrapper exports `HANDSPRING_INCLUDE` as the
read-only container directory `/toolchain/.local/sdk/handspring/Include`.
Known risk, accepted by the maintainer: that mirror redistributes an SDK whose EULA
forbids it; the publisher no longer exists to object. NioLib itself never ships it.

From this library checkout:

```sh
make palmos-shared                         # native tools if available, else container
make palmos-shared PALMOS_BUILDER=container # explicitly select container
make palmos-shared PALMOS_SDK_VERSION=3.5   # optional SDK build
```

`PALMOS_BUILDER=auto|native|container` controls selection. `PALMDEV_TOOLCHAIN`
points to a non-sibling toolchain checkout; `NIO_WORKSPACE` changes the mounted
workspace (default: the project parent). This includes sibling
`fujinet-nio/bridges/rp2350-springboard/include/sbnio.h`, an external NIO ABI
input. Missing tools produce setup instructions and a failing Palm target.
Non-Palm targets never inspect the toolchain. `make check` deliberately requires
Palm and all other configured target toolchains.

The equivalent direct wrapper invocation is:

```sh
../palmdev-toolchain/palm-build --sdk 3.1 --workspace "$(dirname "$PWD")" \
  --project "$PWD" -- make -f makefiles/palmos-shared.mk
```

The wrapper performs the Docker run with the selected SDK, per-invocation GCC
specs and read-only toolchain mount. A bare `docker run IMAGE make` has no SDK.
For native builds, pass `HANDSPRING_INCLUDE=/path/to/Handspring/Include` (a
directory, without `-I`) and use `PALMOS_BUILDER=native`. Native tools must have
the selected SDK configured through palmdev-prep. Both paths use
`PALMOS_SDK_VERSION=3.1` by default, with compatibility declarations for legacy
headers and feature-guarded newer serial traps. No OS 3.5-only call was found.
Actual 3.1 ROM compatibility remains subject to device qualification.

The target builds **NioLib.prc**, **NioLibTest.prc** and **libniolib-client.a**
in `build/palmos-shared-3.1` (or `build/palmos-shared-3.5`), with maps,
symbols, disassembly and `verification.json`. Override with `PALMOS_SHARED_BUILD`.
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
m68k-palmos-gcc -O2 -palmos3.1 -DNIO_PALMOS_SDK31 \
  -I../fujinet-nio-lib/include/palmos -I../fujinet-nio-lib/include \
  app.c ../fujinet-nio-lib/build/palmos-shared-3.1/libniolib-client.a -o app
```

Package the application resources with its own build-prc target. Do not link a
complete static NIO archive alongside the client archive. Existing applications
using earlier compatible traps need no rebuild merely to select USB.

## Application contract

Current identity is **NioLib / libr / NiL1**, ABI **1.5**, implementation **8**.
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
ABI 1.5 adds idle-only `NioLibResetTransport` and Recovery capability `0x80`.
NIO Config exposes it as **Reset**, with explicit confirmation:

1. End NIO clients and return Home. Preserve the failed operation's errors;
   clearing local state cannot undo an unknown remote side effect.
2. For USB, release the host as needed, **physically disconnect/reconnect**
   the cradle/device, and restart the host. The compatible host retires the old
   owner on a fresh physical connection. UsbLibOpen alone does not prove this,
   so a successful driver open does not clear ambiguity automatically.
3. For serial, reset/restart the remote peer so its old sessions and buffered
   replies are gone. For CS1, reset/reinitialize the module and remote host
   ownership; do not clear the Palm marker while an old command remains live.
4. Open NIO Config, select and Save the intended transport, tap **Reset**, then
   confirm its peer/link has been reset. Cancel leaves the marker intact.
   Reset clears only that transport's marker, sends no request, and refuses a
   live client or driver resources that cannot safely be released.
5. Start the host before **Test** or a client. A fresh Clock reply confirms
   reuse. No automatic replay of the failed operation occurs.

Use the public API, never edit private preferences directly. Failed preference
verification reports a Palm error instead of successful recovery. A hard reset
is not required by this procedure. Real soft-reset persistence and physical
recovery still need the documented device acceptance tests; unsaved preferences
must not be treated as permanent storage.

## Verification and limits

[Testing guidance](palmos-shared-testing.md) describes image/ABI audits,
compiled m68k tests with mocked Palm traps, optional app fixtures and physical
acceptance steps. Automated tests validate generated code, not full Palm OS behavior.

The current ABI 1.5 recovery changes and SDK 3.1-built artifacts have not been
qualified on physical hardware. Linux builds and mocked execution tests pass;
the user has independently confirmed the Palm build works. The Prism results
below describe earlier revisions and do not verify the new Reset action,
soft-reset marker persistence or the current host/library pairing.

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
