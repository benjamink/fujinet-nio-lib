# Experimental Atari small stream

`fn_stream.h` is an optional Atari-only API for matching experimental firmware.
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
retry the **same operation and bytes** or abandon and reinitialize. `0x81` means
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
in the source. The existing general library is unchanged and remains the choice
for other services, platforms, concurrent sessions, HTTP and TLS.

Run `make test-atari-stream` for soft65c02 block/guard/retry and active-low POKEY
readiness vectors. `make check` includes those tests and all configured targets.
The workspace headless diagnostic and game tests exercise actual POKEY through
AltirraSDL; they do not prove electrical or real-Atari interrupt timing.
