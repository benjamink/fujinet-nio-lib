# AGENTS

## Project principles

`fujinet-nio-lib` targets small 6502 and similar systems, so code size and RAM use are part of
correctness, especially on BBC Micro targets.

Agents working in this repo should follow these rules.

## Cross-target implementation rules

The library serves different generations of systems: cc65-based 6502 targets,
16-bit MS-DOS, 32-bit Amiga, and hosted development targets. A limitation of
one compiler or machine must not silently become a limitation of every target.

1. Keep protocol contracts, public APIs, wire formats, and test vectors shared
   wherever practical.
2. Before changing common C code, consider the constraints of every compiler
   family that consumes it: cc65, Open Watcom, amiga-gcc, and hosted GCC.
3. If a workaround is required only for cc65 or another constrained target,
   isolate it with a target-specific source file, build selection, or a small
   clearly documented conditional section. Do not impose it on MS-DOS, Amiga,
   or hosted builds without a separate reason.
4. In particular, do not introduce persistent global buffers, reduced
   re-entrancy, or 8-bit-sized limits into non-cc65 implementations merely to
   satisfy cc65. Case-specific duplication is acceptable when it preserves
   materially better behavior on the larger targets.
5. Document any target-specific memory, stack, re-entrancy, or calling-convention
   trade-off next to the implementation and add tests for the shared behavior.
6. Validate at least one constrained target and one non-cc65 target after
   changes to shared code. When the relevant cross-toolchains are available,
   include the MS-DOS and Amiga builds in the all-target build check.

## BBC size and RAM rules

1. Prefer application-owned buffers over library-owned scratch space.
2. Avoid persistent global buffers unless they are required on every call path.
3. Avoid `malloc` and `free` in library code. Hidden heap usage is usually a size bug.
4. Treat BSS usage as seriously as CODE size. A small program can still fail if RAM
   reservations push it into screen memory or workspace.
5. Do not add convenience layers that drag in heavy runtime support for BBC unless
   the user explicitly wants them.

## Archive/linking rules

`cl65`/`ar65` only pull in archive members that satisfy unresolved symbols. That
means file boundaries matter.

1. When size selectivity matters, keep one externally-useful function per source
   file where practical.
2. Separate rarely-used helpers from hot/common paths so small apps do not link
   unused features.
3. Keep state/data owners separate from public entry points if that prevents large
   unused blocks from linking.
4. When a function needs optional helpers such as JSON, write-path support, or TLS
   rewriting, prefer isolating those helpers into their own files or making the
   calling path explicit.

## Adding a client call

Follow the conventions of recent calls such as `fn_fuji_get_info`.

1. One externally-useful function per source file (see the Archive/linking
   rules above).
2. When BBC needs a different path, add its implementation in
   `src/platform/bbc/` beside the common one.
3. Reference every new public call in `tests/library_link_test.c`.
4. Give every new wire test a `tests/run_*_test.sh` and a Makefile target
   listed in `test:`, so `make check` runs it.
5. For fujinet-nio commands that can grow, follow fujinet-nio
   `docs/protocol_reference.md` "Extending commands": request the highest
   version the library understands, accept a reply at that version or lower,
   and treat a malformed reply as an error, never a partial result.

## BBC implementation strategy

The BBC target is ROM-backed.

1. Prefer calling `fn-rom`/MOS interfaces over reimplementing transport logic.
2. Prefer BBC platform-specific code in `src/platform/bbc/` over generic common C
   when the common code increases footprint.
3. If a BBC path becomes hot or still too large after C-level simplification,
   consider moving it to ASM in a later phase.

## Example/test program rules

1. Example programs should not accidentally dominate map analysis.
2. On BBC, prefer `conio`/simple output over `printf` when possible.
3. Avoid `putenv`/`getenv` emulation on BBC examples if compile-time configuration
   is sufficient, because it can pull in heap support.
4. Keep BBC example buffers intentionally small unless the example is explicitly a
   throughput test.

## Analysis workflow

When changing BBC code, use the map files and app analysis tooling.

1. Build with `.map` and `.lbl` outputs enabled.
2. Use `scripts/app_map_analysis.py` to inspect:
   - app vs library vs runtime totals
   - top modules
   - largest BSS/DATA reservations
3. Use `bbc` and `bbc-clib` builds together to distinguish:
   - library cost
   - generic cc65 runtime cost
   - ROM-backed runtime savings

## Preferred optimization order

1. Remove unused linked objects by improving file granularity.
2. Remove persistent RAM reservations.
3. Simplify example/runtime usage that pulls in heavy support code.
4. Only then consider lower-level C rewrites or ASM conversions.
