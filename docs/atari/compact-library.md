# Atari compact public library

The Atari target uses a hybrid CA65/C backend. Public C headers, fastcall
arguments, errors, packet format and the native `$7F` Q/W/P/R transport retain
the original contracts. This is the ordinary public library, independent of
the resident CIO handler and its size budget.

## Build selection

```
make atari ATARI_BACKEND=compact
make atari ATARI_BACKEND=original
```

`compact` is the default. Each selection has its own objects under
`obj/<backend>/atari` and archive `build/fujinet-nio-atari-<backend>.lib`.
Every `make atari` copies the selected archive to the established public path
`build/fujinet-nio-atari.lib`; switching does not depend on timestamps of that
canonical copy. Source selection and compiler makefiles are object dependencies.
The original portable C implementations remain unchanged and selectable for
the compact backend's packet/transport routines. Both Atari backends use the
same Atari-specific ListPages assembly wrapper; other targets retain their
existing source selection, and the portable C is used as the test reference.
Other targets continue using their established sources. `make check
ATARI_BACKEND=original` preserves the original canonical archive even while
building and testing the separate compact archive.

Compact intentionally supports the existing Atari compile-time defaults:
`FN_MAX_SESSIONS=2`, `FN_MAX_URL_LEN=128`, and `FN_MAX_PACKET_SIZE=512`.
Its layout assertions fail explicitly for incompatible overrides; choose
`ATARI_BACKEND=original` when building with custom limits. Public default
limits and the complete runtime API are unchanged.

## Implementation and measured choice

Compact assembly supplies header/parameter construction, checksum folding,
response descriptor parsing, raw calls, initialisation and the common
Open/Read/Write/Close/Info packet builders. State data is a separate archive
member so Clock and raw users do not pull in session lookup code. C wrappers
are separate from the runtime-free raw/checksum cores.

High-level Clock, other File/Network/session operations and Disk codecs remain C.
Their wire behavior and especially Disk's opposite-buffer copy ordering are
already established. The smaller shared packet/raw routines benefit them
without duplicating their more complex service validation. Disk's C object
remains monolithic; this change does not claim every service is individually
optimal. Open translation and service-specific response parsing also remain C.

Workspace measurements use unchanged fnaccept and minimal Clock clients,
identical compiler/linker options and the original archived baseline. The
measured savings justify the hybrid default; the complete maps, segment and
runtime attribution, occupied ranges and percentages are recorded in the
workspace size evidence. Assembly scratch increases static BSS/ZP slightly;
the substantial CODE savings reduce total occupied memory. The two existing
512-byte packet buffers remain the only packet workspaces. No heap or packet
copy layer is introduced.

## Assembly ABI (no C runtime)

Link `build/fujinet-nio-atari-compact.lib` using `ld65` and provide ordinary
`CODE`, `RODATA`, `DATA`, `BSS`, and `ZEROPAGE` segments. Initialise DATA and
clear BSS at startup; enter with decimal mode clear. The caller owns the
hardware stack and Atari OS/SIOV environment. No cc65 software stack,
startup object, runtime library or `ptr1`/`ptr2` allocation is required for
these entry points. All scratch and public operations are synchronous and
non-reentrant, as the shared packet/state interface already requires.
A/X/Y and flags are clobbered. Interrupt handlers must not call the foreground
library; `_fn_atari_proc_notify` remains the optional latch-only entry.

`fn_asm_raw_call` reads the exported `fn_raw_args` block:

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 1 | device |
| 1 | 1 | command |
| 2 | 2 | payload pointer |
| 4 | 2 | payload length (0–506) |
| 6 | 2 | reply pointer (may be zero) |
| 8 | 2 | reply capacity |
| 10 | 2 | optional response pointer |

All words are little endian. Return A is `FN_OK` or the same `FN_ERR_*` as
`fn_raw_call`; X is zero. The optional response is `{status:u8,
payload_length:u16}`. On entry it is reset to INTERNAL/0. A null reply with
sufficient capacity discards the payload successfully. A null payload with
nonzero length or length above 506 fails before W. The parameter block is
retained, but pointed-to buffers obey the same lifetimes/alias restrictions
as the C API. Disk's payload in `_fn_resp_buf` and reply in `_fn_req_buf` is
specifically supported and tested. Every failed exchange or parse returns
without replay; the native transport alone retires outstanding transactions.

`_fn_init`, `_fn_is_ready`, `_fn_parse_response_header` and all existing native
`_fn_transport_*` / `_fn_atari_*` assembly symbols also link without a C runtime
when only their runtime-free dependencies are selected. The parser takes the
existing exported `__fn_parse_ctx`, whose 27-byte cc65 layout has response
pointer at 0, response length at 4, output offset at 8, output length at 10,
and status at 24. It accepts chained descriptors and all existing field widths.

For checksums, fill `fn_core_ptr` (two zero-page bytes) and `fn_core_len`
(two BSS bytes). `fn_asm_checksum` takes the initial 16-bit checksum in A/X
and returns the exact 16-bit folded result in A/X. `fn_asm_packet_checksum`
starts at zero and skips offset 4, returning the packet checksum in A.
Both consume the pointer/count. A zero-length fold preserves its seed.
The cores preserve the original 16-bit wrap-before-fold behavior even for
noncanonical seeds. Assembly layout assertions in `fn_compact_state.c`
reject incompatible session, parser, packet or URL configuration at build time.

## Verification

Run from a workspace environment sourced through `scripts/env.sh`:

```
make check
bash tests/run_atari_compact_test.sh
```

`make check` runs the compact suite in addition to all configured target
builds and existing host/public-link tests. The compact suite compiles
unchanged common C implementations under reference symbols and executes
real compiled C callers on sim65's 6502. It compares full packets and parser
contexts, arbitrary checksum seeds, parameter indices through 255, all
256 descriptor bytes, long chains crossing page boundaries, raw bounds,
null pointers, malformed replies, transport errors and one-exchange/no-replay
behavior. Full-buffer comparisons detect over-writes and stack argument
mismatches. A separate ld65-only executable exercises assembly parameter
blocks against the production native transport with a scripted SIOV peer,
including the Disk alias case. It cannot link if any cc65 runtime dependency
leaks into that path. The complete existing soft65c02 recovery/DCB/capability
fixture also runs against the compact transport variant.

Final cross-repository acceptance additionally runs unchanged fnaccept against
both production NetSIO and embedded AltirraSDL, including rejected W,
malformed R length, interrupted R and reset-after-READY cases. The workspace
acceptance record owns those adapter-specific results and size evidence.

## ListPages size and cycle comparison

The Atari ListPages wrapper is shared by the compact and original Atari
backends. It uses the cc65 calling convention and `fn_raw_call`, with ten bytes
of private zero page and twelve bytes of static scratch. Like the Atari raw
transport, it is synchronous and non-reentrant. Its public struct offsets are
checked at compile time. Other platforms' source selection is unchanged.

Measured on the workspace Atari `nio-config` build with cc65 V2.19
(Git 37d4bb24), against library commit
`134678e97d77a934795a2adaf607934a6f0d79fc`:

| Measurement | Portable C | Atari assembly | Difference |
| --- | ---: | ---: | ---: |
| `fn_file_list_pages.o` CODE | 1,427 B | 469 B | -958 B |
| Object BSS | 0 B | 12 B | +12 B |
| Object zero page | 0 B | 10 B | +10 B |
| Linked app MAIN free | 327 B | 1,273 B | +946 B |
| Plain request cycles | 5,565 | 3,508 | -2,057 |
| Filtered request cycles | 6,553 | 4,027 | -2,526 |
| Plain request instructions | 1,645 | 1,044 | -601 |
| Filtered request instructions | 1,936 | 1,189 | -747 |

Cycle counts cover the function call and the same mock's request validation and
canned reply, not a physical SIO exchange. Instruction counts are the number
of trace lines between the same markers in soft65c02's verbose output. This
fixture compiles both implementations with the same cc65 optimisation flags.
It checks independently specified wire bytes, status/error mapping, untouched
output on failures, 255/256-byte filter limits, a 300-byte URI, exact buffer
boundaries, a 300-byte reply and guard bytes. It runs under `make check`.

Before/after maps and verbose traces from this run are in the ignored
`build/list-pages-optimization/` directory. They survive normal incremental
builds but not `make clean`. To rerun the two profiles:

```sh
soft65c02_unit -v -b build/tests/list-pages-c-profile -i tests/atari_list_pages_reference_soft65c02.yaml
soft65c02_unit -v -b build/tests/list-pages-atari-profile -i tests/atari_list_pages_soft65c02.yaml
```

For each of the first two cases, subtract the displayed start cycle count from
the end count. Changes in code layout can slightly affect cycle totals.

## AppKey URI size and timing

The Atari AppKey URI builder uses a small assembly implementation with the same
private calling convention and output as the portable C version. Run
`bash tests/run_atari_appkey_uri_test.sh` for both implementations against the
same literal expected paths, capacity boundaries and guard bytes. With the
workspace cc65 toolchain, the standalone objects use 293 bytes for C and 147
for assembly (code plus read-only data). The first vector takes 2,168 cycles in
C and 706 in assembly, measured between the soft65c02 profile markers.

The Atari network Read response parser also uses a private assembly path,
checked against the portable C implementation with
`bash tests/run_atari_parse_read_test.sh`. The vectors include short and
malformed replies, errors, and 0, 255, 256 and 260-byte reads with guards.
For the 260-byte case the portable parser takes 5,647 cycles and the assembly
parser 4,740, measured between the same markers. Its object is 232 bytes
smaller in the game build; the assembly reserves four bytes of scratch.
