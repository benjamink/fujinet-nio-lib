# Atari small stream (version 1)

`fn_stream.h` is an optional Atari-only API for firmware supporting the version 1
small-stream commands. It is hardware-validated and opt-in; availability in a
published archive depends on its release version. Do not assume library 0.1.2
or older firmware contains it. `fn_stream_init` checks capabilities before use.
It does not replace `fn_open/read/write` or Q/W/P/R. Each call takes one AX
pointer to a caller-owned 262-byte context; it has no cc65 software-stack
arguments and links only `fn_stream.o` and `fn_stream_sio.o`. The request and
result arrays must remain separate for safe retries. There are four private
zero-page bytes plus small non-reentrant scratch variables. No call from an ISR.

Initialize, copy a lowercase tcp:// URL into `data`, set `length`, and call
`fn_stream_open`. Read sets `length` to capacity (1..120) and returns bytes in
`received`, count in `actual`, EOF/more-data in `flags`. Write returns accepted
count; send only the remaining suffix in a new call. Close releases the stream.
`fn_stream_appkey` retrieves the game's legacy creator 1/app 1 lobby key, using
`length` for its number. It does not use the legacy compatibility library.

Return statuses 0..8 follow firmware StatusCode (3 busy, 4 not-ready). The
sequence advances on every validated response, including errors. `0x80` means
carrier failure after bounded retries: the sequence/request remain unchanged;
retry the **same operation and bytes**. If abandoning, reconnect and reconcile
application state: reinitializing cannot tell you whether an uncertain write
reached the peer. `0x81` means
invalid input/capabilities. These codes are distinct from the general FN_ERR API.

The carrier uses 19200 baud, bypasses OS SIO and U1MB, leaves NMI enabled, and
uses POKEY channels 3/4 for serial. Channels 1/2 remain available to a suitably
written immediate VBI. CPU IRQs are masked during a transaction; keyboard IRQ
latches remain enabled. It restores SKCTL and IRQEN from OS shadows and releases
COMMAND. It leaves serial clock configuration in AUDCTL/AUDF3/AUDF4; applications
must not use those registers for audio during calls. No HSIO setting is changed.
Timeouts require the OS RTCLOK VBI clock to keep running. Arbitrarily long NMIs
can still overrun serial reception: hardware validation is required.

The low-level driver derives from Michael Sternberg's GPL-3.0 SYN-ACK WIRE
polled SIO, with bounded transmitter waits added. Original work remains credited
in the source and THIRD_PARTY.md. The library is GPL-3.0; this optional carrier
is not a separately permissive component. Distribution must respect LICENSE
and the retained third-party notices. The existing general library is unchanged and remains the choice
for other services, platforms, concurrent sessions, HTTP and TLS.

Run `make test-atari-stream` for soft65c02 block/guard/retry and active-low POKEY
readiness vectors. `make check` includes those tests and all configured targets.
The workspace headless diagnostic and game tests exercise actual POKEY through
AltirraSDL; they do not prove electrical or real-Atari interrupt timing.

## Who should use it, and why it exists

This path is for Atari games, terminal-like clients and other small interactive
TCP applications whose limiting resource is main RAM/code size, rather than
network bandwidth. SYN-ACK WIRE motivated it: general packet construction,
service parsing, transport buffers and cc65 parameter handling cost several
kilobytes for a client that only needed a small bidirectional byte stream.

The normal API represents many services and packet shapes. This API instead
projects a deliberately narrow set of operations onto Atari SIO. A 6502 routine
passes one context pointer in A/X; FujiNet builds the normal routed requests,
tracks read/write offsets and retains the last result. This is less work on the
Atari, not a second TCP stack or a bypass around the core service architecture.

| Requirement | Small stream | General NIO route |
| --- | --- | --- |
| Target | Atari physical SIO or NetSIO | Multiple targets/transports |
| Network | One TCP connection per ingress | General network service API |
| Transfers | 1–120 bytes per call; caller handles partials | General service-sized packets |
| Other services | Fixed creator 1/app 1 lobby AppKey read only | Files, config, disks, HTTP/TLS and other service calls |
| Serial | Polled 19200, bypasses OS SIOV | Existing target transport and OS integration |
| Preferred use | Small interactive clients with suitable interrupt/audio code | General tools, bulk transfer and portable applications |

Do not move a general application onto this API solely because its object is
smaller. Linking both APIs retains both sets of costs. Multiple contexts do not
provide multiple connections: callers must serialize all operations and own the
single session exclusively. Do not interleave another owner or another API's
access to this connection.

## What the application pays

Measured stream objects contribute **875 CODE bytes, 17 scratch BSS bytes and
4 zero-page bytes**. The application supplies a **262-byte context** (131-byte
request plus 131-byte result), plus its own URL, queues and UI storage. These
measurements exclude application code, cc65 startup/runtime, screen and stack.
Separate request/reply buffers preserve the exact request across failed replies.
There is no library heap allocation or hidden 1 KiB packet buffer. The six small
entry points share two archive members; their small shared core links together.

The measured game XEX went from 19,719 bytes (general NIO) to 13,941 bytes, saving
5,778 bytes. Legacy was 12,718 bytes. These are whole executable comparisons,
not a promise that every application will save the same amount.

The application must:

- Keep the context alive, initialize once, and set length/data before each call.
  Do not edit token, sequence or version fields; the library owns them.
- Treat open success as acceptance, not proof of a connected socket. Busy (3)
  and NotReady (4) mean yield to the main loop and try later with a new call.
  A connection failure can arrive on the first write. Error 5 means firmware
  I/O failure; carrier failure is 128. Check the server, URL and firewall too.
- On successful write, advance by `actual`, never assume the whole buffer was
  sent. Retain unsent bytes. On read, consume only `actual` bytes, process data
  before acting on EOF (flags bit 0), and tolerate short reads. TCP has no message
  boundaries; framing remains the application's job. Successful zero progress
  is not permission to discard data or busy-loop forever.
- On 128, retry the identical operation/length/data before any other operation.
  Do not change the context to issue a read or close while expecting replay of
  an uncertain write. Each call already makes up to three transaction attempts.
  A new operation after a validated Busy/NotReady is different from a retry of a
  missing response. Choose application deadlines/backoff and a reconnect policy.
- Serialize foreground calls; no ISR or concurrent calls. CPU IRQs are masked
  during serial transfers, so IRQ-driven work is delayed. VBI/DLI NMIs continue.
  Keep RTCLOK advancing and preserve registers in interrupt handlers. Long NMIs
  can still lose serial bytes; this is not immunity to arbitrary interrupt load.
- Reserve POKEY channels 3/4 and AUDCTL for serial while calling. Channel 1/2
  VBI sound is compatible when it does not change serial clock/control registers.
  The driver does not save/restore arbitrary audio state. Disk SIO and other
  code using POKEY must also cooperate. U1MB tables and advertised HSIO settings
  are untouched; firmware must permit fallback from fast disk traffic to 19200.

There is no background CPU thread: calls poll hardware synchronously, although
socket progress can be NotReady. Missing devices can take multiple timeout and
retry intervals; COMPLETE waits up to 960 VBI ticks per attempt (about 16 s NTSC,
19.2 s PAL). Do not assume short or fixed latency when disconnected.

## Costs moved to FujiNet, and reliability boundaries

The firmware lazily owns one session per ingress: 262 bytes of fixed replay
arrays, metadata, a result vector up to 131 bytes and bounded temporary routed
request/response vectors. Existing NetworkDevice TCP socket/RX buffers remain
additional costs. No extra task or task stack is created. There is no unlimited
history/cache: only the last request and result are retained.

Each operation uses an S write followed by a T read, rather than the general
packet/poll exchange. Replies are padded to the caller's requested capacity:
asking for 120 bytes when only one is ready still receives 131 bytes including
the header. Two SIO command phases, checksums and serial gaps remain. This buys
smaller client code, not maximum throughput; tune read capacity to your workload.

Exact token/sequence/request retries replay the previous result without repeating
socket side effects. This does not guarantee delivery to the server application,
or exactly-once game moves after reconnect/reboot. TCP disconnects, firmware
restart and abandoned transactions require application-level recovery. Tokens
are not security credentials. Sequence exhaustion requires an ingress reset;
calling init again does not reset an exhausted firmware session.

## Acceptance and compatibility

Hardware tests on the user's Atari completed 459 echoes with HSIO off and 600
with U1MB HSIO enabled, DLI and VBI sound on, with no errors. SYN-ACK WIRE gameplay,
disk help, sound, colours and logoff/rejoin also worked. A stale game/missing-grid
observation remains separate and unresolved; no transport cause is established.
These tests validate those workloads, not every Atari, interrupt routine or server.

Version 1 uses the documented s/S/T commands and fixed 120-byte capability.
Incompatible firmware must be rejected by init; this path does not silently
fall back to the general library. Keep version 1 behavior compatible when adding
future capabilities; use a separately negotiated version for incompatible wire
changes. No removal of the general backend or release-version bump is implied.

See the firmware `docs/atari/small-stream.md` for wire/lifecycle details and
nio-apps `docs/atari-small-stream.md` for the runnable diagnostic.
