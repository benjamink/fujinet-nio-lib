# Palm shared-library testing and operator tools

Build as described in the [guide](palmos-shared-library.md). Tests require Python,
a native C compiler and the test-only requirements in
[requirements-palmos-shared.txt](../tests/requirements-palmos-shared.txt):

```sh
python3 -m venv build/palmos-test-venv
build/palmos-test-venv/bin/pip install -r tests/requirements-palmos-shared.txt
make test-palmos-shared test-palmos-usb-preparation \
  PALMOS_SHARED_BUILD="$PWD/build/palmos-shared-usb" \
  PALMOS_SHARED_PYTHON="$PWD/build/palmos-test-venv/bin/python"
```

Run from the library repository; use absolute overrides from other directories.
Build first and inspect SKIP reports: compiled checks need linked artifacts and
Unicorn. Native image tests do not substitute for compiled execution. The build's
`verify` target also needs m68k nm/objdump; it audits resource identities, trap
ordinals, relocations, A4 entry/exit, absence of A5 accesses and unwanted CRT code.

The tests execute actual m68k library/client code with mocked Palm OS and
transport peers. They cover globals/pointers, caller registers, load/refcounts,
owner/handle isolation, two handles, response validation, configuration,
CS1 sequence/deadline handling, serial SLIP/partial I/O, USB counted I/O,
retained driver memory across app exit and later cleanup, readiness and cached
JSON selectors. Mocks do not qualify ROM timing or arbitrary physical recovery.
Existing native NIO tests and static Palm builds remain separate; see
[building](building.md) and the Makefile test targets.

## Optional application compatibility

Normal tests do not require another repository. To require compatibility checks
with real existing ISS/Weather linked artifacts, supply the owning source harness
and already-built artifact directory:

```sh
PALMOS_APPS_REPO=/path/to/springboard-nio \
PALMOS_APPS_BUILD=/path/to/existing-app-build \
PALMOS_REQUIRE_APPS=1 \
  make test-palmos-shared \
  PALMOS_SHARED_BUILD="$PWD/build/palmos-shared-usb" \
  PALMOS_SHARED_PYTHON="$PWD/build/palmos-test-venv/bin/python"
```

The directory must contain `isstracker/isstracker{,.symbols}` and
`weather/weather{,.symbols}`; the repository supplies `tests/test_shared_apps.py`.
Absent fixtures produce explicit SKIP normally and failure with REQUIRE_APPS=1.
Do not fabricate fixtures or rebuild old apps just to disguise a missing baseline.
The app repository separately tests retained-document field extraction and GET
counts. Library tests validate the selector's actual command, bounds and ownership.

## Short physical acceptance

After safe idle-library HotSync installation, select USB in NIO Config and Save.
Run the libusb NIO host after HotSync releases the cradle. Run NioLibTest Clock +
two HTTP once, then ISS and Weather normally. Return Home, select Springboard,
stop/release the USB host, start the existing RP2350 packet-link host and repeat
Clock + two HTTP once. Record Palm errors and host cleanup; stop at a failure.
No firmware flash or serial hardware test is needed. Host instructions are in
`fujinet-nio`, at `docs/visor-usb-host.md`.

## Optional bench tools

Ordinary builds/HotSync do not require a historical manifest. Retained scripts
support explicit paths; use `--help` for all options:

| Script | Role and safety boundary |
| --- | --- |
| `scripts/palmos_usb_bundle.py` | Offline fresh install/rollback/host copy; verifies previous manifest hashes and PRC identities; `--dry-run` validates only |
| `scripts/palmos_usb_install.py` | Checks bundle hashes; dry-run unless authorization and idle confirmation are explicit; optional `--collector-tools` supplies external HotSync workflow |
| `scripts/palmos_usb_session.py` | Checks host hashes; dry-run unless authorized; explicit `--bundle`, `--logs`, optional relocated `--executable`; offline `summary LOG` |
| `scripts/palmos_usb_access.py` | Linux-only, explicitly authorized finite access to one verified physical cradle path; restores surviving ACL/binding on exit |

**Tooling limitation:** the bundle writer still stamps manifest `abi: "1.3"`.
Do not treat it as an ABI detector or publish that field as the version of a newly
built 1.4 library. Verify ABI from the header/image and installed GetVersion;
correcting this metadata requires a separate tooling change. Hashes remain
artifact identities, not proof of hardware qualification. Existing frozen bundles
are historical evidence, not universal source/build defaults.

The access helper must not be run automatically. It requires separate operator
approval, a verified physical path, finite duration and explicit authorization;
it is not a udev installer or general USB permission policy. No driver detach,
privilege escalation or device reset belongs in an ordinary build/test command.
Keep raw application logs and rollback artifacts local. Log-summary request
counts alone do not prove successful delivery or replace Palm PASS results.
