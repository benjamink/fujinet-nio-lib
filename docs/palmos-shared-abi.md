# Palm NioLib ABI reference

The authoritative declarations are [NioLib.h](../include/palmos/NioLib.h);
applications normally use [NioLibClient.h](../include/palmos/NioLibClient.h).
See the [shared-library guide](palmos-shared-library.md) for build and lifecycle.

## Identity and negotiation

| Field | Value |
| --- | --- |
| Library database / type / creator | `NioLib` / `libr` / `NiL1` |
| Diagnostic application / creator | `NioLibTest` / `NiT1` |
| ABI major.minor / implementation | **1.5 / 8** |
| Scalar layout | Palm fixed-width UInt16/UInt32, native big-endian, 2-byte alignment |

Find with `SysLibFind`, load with `SysLibLoad` if missing, query version and
capabilities, then Open. Version/Capabilities work before Open without allocation.
Reject a different major or missing required capabilities; negotiate minor
features before calling their traps. A newer minor preserves existing ordinals
and structures. The networking client accepts ABI >=1.1; configuration requires
>=1.2; USB needs `nioLibCapUSB`; cached JSON query needs >=1.4 plus its capability.
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

Transport IDs are **1=Springboard**, **2=Serial**, **3=USB**. Serial options are
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
state, not configuration. CS1 uses private unsaved marker 0x4e32. Applications
must never edit these records to force a retry.

`NioLibResetTransport(refNum, transport, confirmation)` requires an open library,
ABI >=1.5 and capability `0x80`. Transport must be 1, 2 or 3; confirmation must
equal `nioRecoveryPeerReset` (1), after the operator retired the peer's old
handles and buffered replies. USB requires a physical reconnect. Active clients,
backends or unreleasable driver resources cause Busy. Invalid arguments cause
Param; failed marker write/read-back causes Globals. The trap clears only the
specified marker, sends no packets and never replays a request. Follow the
user guide's recovery procedure: clearing local state cannot undo side effects.
