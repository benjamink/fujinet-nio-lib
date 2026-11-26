# Palm NioLib ABI reference

The authoritative declarations are [NioLib.h](../include/palmos/NioLib.h);
applications normally use [NioLibClient.h](../include/palmos/NioLibClient.h).
See the [shared-library guide](palmos-shared-library.md) for build and lifecycle.

## Identity and negotiation

| Field | Value |
| --- | --- |
| Library database / type / creator | `NioLib` / `libr` / `NiL1` |
| Diagnostic application / creator | `NioLibTest` / `NiT1` |
| ABI major.minor / implementation | **1.7 / 13** |
| Scalar layout | Palm fixed-width UInt16/UInt32, native big-endian, 2-byte alignment |

Find with `SysLibFind`, load with `SysLibLoad` if missing, query version and
capabilities, then Open. Version/Capabilities work before Open without allocation.
Reject a different major or missing required capabilities; negotiate minor
features before calling their traps. A newer minor preserves existing ordinals
and structures. The networking client accepts ABI >=1.1; configuration requires
>=1.2; USB needs `nioLibCapUSB`; cached JSON query needs >=1.4 plus its capability.
The Network transport's calls need >=1.7 plus `nioLibCapNetLib`.
ABI 1.0 foundation clients remain compatible but cannot use networking.

| Capability | Bit |
| --- | --- |
| A4 globals | `0x01` |
| Network | `0x02` |
| Clock | `0x04` |
| Foreground client | `0x08` |
| Transport configuration/status | `0x10` |
| USB | `0x20` |
| Cached JSON selector | `0x40` |
| Confirmed idle transport recovery | `0x80` |
| Extended services | `0x100` |
| Network transport over NetLib (ABI 1.7) | `0x200` |
| Foundation globals diagnostics (`TestOnly`, legacy `TestGlobals` alias) | `0x80000000` |

The diagnostics bit describes retained test entry points; the library also
contains real networking. No internal session structures or storage pointers
are returned. Callers provide buffers and set structure `size` fields as required.
Clock time crosses the trap as eight little-endian bytes, avoiding a compiler-
specific 64-bit argument ABI. Palm scalars/structures are not wire packet layouts.

## Stable trap table

Use the SDK `SYS_TRAP` declarations, not manually constructed calls. refNum is
UInt16, tokens/offsets are UInt32, and pointers are 32 bits. Wider service-facing
scalars are range-checked before narrowing to the common API.

| Trap | Entry point |
| --- | --- |
| `0xA801`–`0xA804` | Open, Close, Sleep, Wake |
| `0xA805` | GetVersion |
| `0xA806` | GetCapabilities |
| `0xA807` | TestGlobals |
| `0xA808` | BeginClient |
| `0xA809` | EndClient |
| `0xA80A` | Probe |
| `0xA80B` | NetOpen (plain or translated via OpenArgs) |
| `0xA80C` | NetInfo |
| `0xA80D` | NetRead |
| `0xA80E` | NetWrite |
| `0xA80F` | NetClose |
| `0xA810` | NetReady |
| `0xA811` | ClockGet |
| `0xA812` | ClockTimezone |
| `0xA813` | GetConfig |
| `0xA814` | SetConfig |
| `0xA815` | GetTransportStatus |
| `0xA816` | NetJsonQuery |
| `0xA817` | ResetTransport (ABI 1.5) |
| `0xA818`–`0xA847` | ABI 1.6 services (below) |
| `0xA848` | GetNetLibStatus (ABI 1.7) |
| `0xA849` | NetLibHangup (ABI 1.7) |

Direct service traps return Palm `Err` and a separate `FN_*` output. Balanced
Close requires a remaining-count pointer; only a successful zero result permits
removal. Nonzero can represent other Opens or retained driver ownership.
Remove only a library registration the application loaded and can safely release.
Sleep/Wake are no-op lifecycle hooks, not background networking.

## Preferences and status

Saved preference **NiL1 / 0x4e34**, version **1**, stores `NioLibConfig`, **24 bytes**:

```text
UInt16 size, version, transport, serialOptions;
UInt32 serialBaud;
UInt32 reserved[3];
```

Transport IDs are **1=Springboard**, **2=Serial**, **3=USB**, and from ABI 1.7
**4=Network** (no hardware: the NIO device runs inside NioLib over NetLib). Serial options are
zero or `nioSerialRTSCTS=1`; baud is 9600, 19200, 38400, 57600 or 115200.
Reserved fields must be zero. Missing settings produce Springboard/115200/no-flow
defaults without writing preferences. Invalid sizes/versions/values return the
Invalid flag and block initialization. SetConfig validates, saves and reads back;
it returns BUSY while a foreground owner/backend is active.

TransportStatus is 16 bytes with selected/active IDs, flags, maximum packet/session
capacity, configuration flags and `FN_*` result. Supported does not mean available;
passive inspection can report availability unknown. Probe's `modulePresent`
is Springboard-specific. USB/serial unresolved-operation markers use separate
**unsaved** NiL1 preferences 0x4e36/0x4e35, version 1; these are private recovery
state, not configuration. CS1 uses private feature NiL1 0x4e32 (the CS1 base
with the posted sequence in its low byte): a feature write is a table update,
not a database write, so marking each doorbell costs microseconds, and a soft
reset clears it. A CS1 marker that cannot be read (any Feature Manager error but
`ftrErrNoSuchFeature`) counts as unresolved. Applications must never edit these
markers to force a retry.

`NioLibResetTransport(refNum, transport, confirmation)` requires an open library,
ABI >=1.5 and capability `0x80`. Transport must be 1, 2, 3 or (ABI 1.7) 4, which
holds nothing between sessions and so has nothing to clear; confirmation must
equal `nioRecoveryPeerReset` (1), after the operator retired the peer's old
handles and buffered replies. USB requires a physical reconnect. Active clients,
backends or unreleasable driver resources cause Busy. Invalid arguments cause
Param; failed marker write/read-back causes Globals. The trap clears only the
specified marker, sends no packets and never replays a request. Follow the
user guide's recovery procedure: clearing local state cannot undo side effects.

## ABI 1.6 service additions

`nioLibCapServices` (`0x100`) and ABI minor >= 6 are both required for the
extended traps declared in `NioLibServices.h`, included by `NioLib.h`. Existing
ordinals and structures are preserved. The client checks capability/version
before calling a new trap and returns `FN_ERR_UNSUPPORTED` for older libraries.
New arguments use fixed-width scalars and caller-owned public NIO structures,
Palm two-byte alignment, and no retained caller pointers. Byte-sized inputs
are widened to UInt16 for traps and checked before conversion to common C.
Enums remain on the C-client side; timestamps cross traps as eight LE bytes.

| Traps | Service |
| --- | --- |
| `0xA818` | Guarded RawCall |
| `0xA819`–`0xA81D` | File stat/read/write/mkdir/ListPages |
| `0xA81E` | HTTP Open with headers and body length |
| `0xA81F` | Fuji firmware/build information |
| `0xA820`–`0xA826` | Wi-Fi status/config/set/adapter/scan/trial join/join status |
| `0xA827`–`0xA82B` | AppStore stat/read/write/delete/list |
| `0xA82C`–`0xA82F` | Slot catalogue get/put/delete/range |
| `0xA830`–`0xA831` | Resolve mount target / format display |
| `0xA832`–`0xA833` | Set runtime / persistent timezone |
| `0xA834`–`0xA83B` | Disk mount/unmount/flush/info/change/restore/read/write operations |
| `0xA83C`–`0xA83F` | Set time / NTP sync / formatted time / timezone-specific time |
| `0xA840`–`0xA844` | Legacy appkey namespace/read/buffered read/write and disk last-error query (see declarations) |
| `0xA845` | Disk inspection without mounting |
| `0xA846`–`0xA847` | Fuji HSIO get/set |

The exact per-call ordering is authoritative in `NioLibServices.h` and checked
against the generated dispatch table by the image audit. RawCall accepts the
foreground token and standard `fn_raw_response_t`; Palm Err, FN call result,
and remote response status remain distinct. Raw NetworkService commands return
Unsupported because they bypass owned-handle allocation/cleanup. Use the typed
network calls instead. Other raw commands receive ordinary framing/identity/
checksum validation and are never automatically replayed by this entry point.

The byte-sized C argument ABI and short enums used by Palm GCC are deliberately
kept out of the trap ABI. Client tests exercise compiled wrappers, including
parameters with nonzero byte values, instead of assuming host C layouts.

FileWrite retains command uncertainty when a transport-success reply fails typed
acknowledgement validation (for example, a wrong offset or impossible byte count).
EndClient reports that uncertainty even after a later successful query. Unsent
local argument failures and explicit remote error responses do not acquire this
semantic-uncertainty flag. Valid zero/partial acceptance remains a successful
call with its confirmed count. This adds no retry or protocol change.

Implementation 9 retains master's PR #11 transport code and conservative cleanup
policy. Design A/B certificates, preference timing instrumentation and the
read-only uncertainty-policy change are not included in this integration.
The static libraries and shared client now expose the service families above;
link the client archive alone when using installed NioLib. Caller-supplied
transport callback/context APIs remain static integration interfaces, not new
shared traps. Raw NetworkService calls are explicitly unsupported through the
shared interface because they bypass owned session bookkeeping.

## ABI 1.7: the Network transport

ABI 1.7 is this branch's addition on top of 1.6. It leaves every 1.6 trap,
ordinal, capability bit and structure as it is, and appends:

| Item | Value |
| --- | --- |
| Capability | `nioLibCapNetLib` (`0x200`) |
| Transport ID | `nioTransportNetwork` (4) |
| `0xA848` | `NioLibGetNetLibStatus(refNum, NioLibNetLibStatus *)` |
| `0xA849` | `NioLibNetLibHangup(refNum)` |

Both calls need ABI >=1.7 and `nioLibCapNetLib`, and an Open library, not a
client session. `NioLibNetLibStatus` (36 bytes, `size` set by the caller) holds
flags (`nioNetLibPresent` 1, `nioNetLibUp` 2, `nioNetLibSession` 4), the open
sessions, the connection and failure counts, bytes in and out, and the Palm's
address as text. During a client session its counters are that session's;
otherwise they are the last session's. GetNetLibStatus returns Busy while a
trap is running. NetLibHangup returns Busy while a client session or anything
else has NetLib open.

The Network transport serves the typed 1.6 calls the core implements (network
Open/Read/Write/Close/Info, Fuji information, Wi-Fi status and configuration)
through the same packet path as the hardware transports; the other services
answer Unsupported from the core. `RawCall` is the raw packet entry point for
this transport too.

Unpublished Network-transport builds (implementations 9–12) reported ABI 1.6
with bits `0x100` and `0x200` and had their own traps at `0xA818`–`0xA81A`. A
client must not use the 1.6 services when a library reports ABI 1.6 with
`0x200`; the client archive and NIO Config refuse them there.
