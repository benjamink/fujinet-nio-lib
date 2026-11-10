# Palm shared-library testing

Build as described in the [guide](palmos-shared-library.md). Required tests use
Python, a native C compiler and NioLib's pinned test-only
[requirements](../tests/requirements-palmos-shared.txt). Unicorn executes actual
compiled 68K code with mocked OS calls; it does not boot Palm OS.

```sh
python3 -m venv build/palmos-test-venv
build/palmos-test-venv/bin/pip install -r tests/requirements-palmos-shared.txt
make palmos-shared
make test-palmos-shared PALMOS_SHARED_PYTHON=build/palmos-test-venv/bin/python
```

Missing Unicorn, required artifacts or ABI audit entries **fails** the target.
Native image tests do not replace compiled execution. The build audit checks
resources, trap ordinals, relocations, A4 entry/exit, absence of A5 accesses and
unwanted CRT code. The full gate builds every configured target including Palm:

```sh
make check PALMOS_SHARED_PYTHON=build/palmos-test-venv/bin/python
```

The full check requires the other platform toolchains as well; the Palm image
alone is not an all-target environment. Non-Palm builds such as `make linux`
do not require Palm tooling.

Test the optional SDK build separately:

```sh
make palmos-shared PALMOS_SDK_VERSION=3.5
make test-palmos-shared PALMOS_SDK_VERSION=3.5 \
  PALMOS_SHARED_PYTHON=build/palmos-test-venv/bin/python
```

Coverage includes compiled library/client globals, ownership, handles,
validation, CS1 deadlines, serial SLIP/partial I/O, USB counted I/O, retained
driver memory, readiness, JSON selectors and ABI 1.5 confirmed recovery.
Recovery tests check idle/confirmation guards, failed preference writes, no
reset-time packets, reload persistence and reuse after modeled peer reset.
They do not establish real ROM timing, reset persistence or physical recovery.

## Optional application compatibility

ISS/Weather fixtures remain optional. To require checks with existing compiled
applications, supply their owning harness and artifacts:

```sh
PALMOS_APPS_REPO=/path/to/springboard-nio \
PALMOS_APPS_BUILD=/path/to/existing-app-build \
PALMOS_REQUIRE_APPS=1 make test-palmos-shared \
  PALMOS_SHARED_PYTHON=build/palmos-test-venv/bin/python
```

The build directory must contain `isstracker/isstracker{,.symbols}` and
`weather/weather{,.symbols}`; the repository supplies `tests/test_shared_apps.py`.
Missing fixtures explicitly skip normally and fail with REQUIRE_APPS=1.
Do not fabricate fixtures or rebuild old apps to disguise a missing baseline.

## Short physical acceptance

Install with your normal HotSync tool after ending NIO apps, returning Home,
and confirming the old library can safely be released. Release HotSync's cradle
ownership. Start the selected NIO host **before** the Palm client.
Host instructions are in `fujinet-nio/docs/visor-usb-host.md`.

First test SDK 3.1-built NioLib, NioLibTest and NIO Config on the user's Visor
Prism running Palm OS 3.5. Run offline lifecycle checks, then Clock + two HTTP
handles on USB and CS1, normal app exit and repeated launches. This checks
forward compatibility, not actual OS 3.1H support. SDK 3.1H hardware, serial
and macOS qualification remain separate.

For each transport, induce a timeout or malformed reply, end the client and
record RecoveryRequired and its unsaved NiL1 marker. Verify after library
unload/reload. In a separate controlled run perform a Palm soft reset and
inspect the marker before reset/Save/Test actions. Record whether the original
marker and RecoveryRequired survived; do not infer persistence from mocks.
Use a debugger/preference inspector for the raw record.

Reset the actual remote peer/link as described in the
[recovery procedure](palmos-shared-library.md#globals-cleanup-and-recovery),
then confirm Reset in NIO Config. Verify the flag clears, the next Clock passes
and the host has no old handles. Canceling Reset must preserve the marker.
Save changes before Reset. Preserve logs/Palm errors for failures; never replay
a state-changing operation merely to find out whether it succeeded.
