# Palm shared-library code layout

The expanded NioLib uses the prc-tools `text_64k` linker script, whose code
resource bound is **64,720 bytes**. This is the conservative resource bound
supported by early Palm OS versions, not the unsigned-16-bit maximum 65,535.
The independent A4 globals budget remains **32,767 bytes**. Normal builds do
not allocate padding or reserve the unused code budget as writable RAM.

The primary source for these limits and the function-placement technique is
[prc-tools, Special projects](https://prc-tools.sourceforge.net/cgi-bin/info/special%2Bprojects.html).
The sibling palmdev-toolchain provides that linker script. No toolchain or
other platform's compiler flags or memory limits have been changed here.

## Placement

`makefiles/palmos-shared.mk` places the following groups in order:

1. Entry, dispatch table and lifecycle/image code.
2. New service entries and existing service entries, keeping trap destinations
   within signed 16-bit reach of the dispatch table.
3. Legacy helpers and higher-level common service codecs.
4. A4 ownership guards (`owner.c`), transport dispatch and configuration, and
   the required libc/libgcc helpers. These functions need to be reachable from
   both the early entries and the later core/backends.
5. Common disk/network codecs and physical backend code.
6. The Network transport (ABI 1.7): `netlib.o` and its NIO core (`netlib/`).
   Placed last so the groups above keep their distances; `services.o`
   reaches it through `transport.c`, and the core calls back only into the
   libc helpers.

The common service sources, protocol contracts and public `fn_*` architecture
are retained. File boundaries remain useful for archive selection; the owner
module only relocates existing foreground state/guards and adds no buffer.

The table still uses the existing ordinals and signed offsets. No offset is
silently widened, masked or treated as unsigned. The linker continues to reject
PC-relative relocation overflows. The serializer rejects code-resident absolute
relocations and unsupported globals fixups, and the audit verifies dispatch
ordinals, A4 setup/restoration, and absence of caller A5 dependence.

Objdump also decodes inline data as instructions. The A5 audit exempts only the
already validated dispatch/name bytes and the byte-for-byte verified legacy
appkey hexadecimal/persistent-prefix constants and the Network transport's
named text constants (`k_netlib`, `k_http_host`, ...). Every A5 instruction outside
those exact regions is still rejected; tests cover both sides of the regions
and altered constants.

## Validation

The image audit records current code/global budgets, dispatch headroom and
project DISP16 relocation margins in verification.json. Run make test-palmos-layout
with the Palm test Python to link early/late padding fixtures and execute the
compiled lifecycle/network checks. Linker errors remain fatal; free code bytes
alone do not establish reachability. No padding is included in normal builds.

## Further growth

Keep entries near the front, helpers central, and review the reported margins
when adding a service. Relink after layout changes (the linked image depends on
its Makefile). A failing signed relocation must be resolved by placement or a
properly validated forwarding mechanism, never by dropping an audit or ignoring
linker errors. The growth fixtures should continue to pass as features grow.

A single resource cannot grow past 64,720 bytes under this model. If expansion
approaches that ceiling, or the call graph no longer permits useful placement
headroom, additional code resources will require explicit library-owned loading,
locking, relocation and cross-resource calls. The current scheme preserves the
existing installed-library architecture and buys measurable room; it is not an
unlimited replacement for that future work.

These checks establish compiled-image and modeled execution evidence. Physical
installation and hardware behavior require separate validation.
