# BBC Beebium Test Scaffold

**Setup:** export `BEEBIUM_HOME`, `FUJINET_NIO_HOME`, and `FN_ROM_HOME`, then run
tests — [docs/DEVELOPMENT.md](../../docs/DEVELOPMENT.md). Use `./run_pytest.sh`
(not bare `uv run pytest`). No separate venv sync step.

This directory is the initial scaffold for BBC-focused integration testing of
`fujinet-nio-lib`.

The intended shape mirrors the testing split already used by `fn-rom`:

- `scripted/` for deterministic PTY/fake-device tests
- `real/` for Beebium + real `fujinet-nio` end-to-end tests

## Current scope

This first cut is intentionally small:

- the main repo `test-bbc` target builds the BBC library and BBC-supported examples
- this directory provides the dependency and fixture skeleton for future pytest lanes
- one real scripted smoke test is now checked in for a BBC library-linked C app run from SSD via `fn-rom`
- app-store CRUD coverage runs a BBC C app from SSD, validates the FujiBus wire
  payloads for stat/read/write/list/delete, and asserts the app's screen output

## Reuse source

The design is based on these `fn-rom` assets:

- `fn-rom/integration-tests/beebium/conftest.py`
- `fn-rom/integration-tests/beebium/helpers.py`
- `fn-rom/integration-tests/beebium/fujinet_runner.py`
- `fn-rom/integration-tests/beebium/scripted/test_network_device.py`
- `fn-rom/integration-tests/beebium/real/test_real_fujinet_e2e.py`

## Planned minimal port

### Scripted lane

- Launch Beebium with `fn-rom` installed
- Let Beebium create the PTY
- Attach a fake Fuji device to the PTY
- Run a BBC C smoke app or existing example linked against `fujinet-nio-lib`
- Assert emitted network `OPEN`, `READ`, `WRITE`, `CLOSE`, and translate packets

Current scripted coverage:

- HTTP GET smoke using `apps/http_get_smoke.c`
- BBC `fn_open_long()` short-delegation and long-URL OSWORD paths using `apps/http_get_long.c`
- TCP stream partial/no-probe smoke tests
- app-store CRUD using `apps/appstore_crud.c`

Recommended next scripted tests:

- JSON query smoke once a compact BBC test app is added

### Real lane

- Start an isolated real `fujinet-nio` instance via the same PTY topology used by `fn-rom`
- Launch Beebium attached to that PTY
- Run the BBC example or a small smoke app
- Assert on `fujinet-nio` stdout logs and, where useful, BBC screen output

Recommended first real tests:

- HTTP GET against a local `httpbin` service
- TCP echo against a local echo service

## Suggested follow-up layout

```text
integration-tests/beebium/
  pyproject.toml
  README.md
  conftest.py
  helpers.py
  fujinet_runner.py
  scripted/
    test_http_get.py
    test_tcp_get.py
  real/
    test_real_http_get.py
    test_real_tcp_get.py
  apps/
    http_get_smoke.c
    tcp_get_smoke.c
```

The `apps/` route is recommended over driving the verbose examples directly once
the test lane is implemented, because it keeps screen assertions much smaller.

## Environment

Two repo roots plus fn-rom for the sideways ROM image:

| Variable | Required | Meaning |
|----------|:--------:|---------|
| `BEEBIUM_HOME` | yes | beebium repo root |
| `FUJINET_NIO_HOME` | yes | fujinet-nio repo root (`fujinet_tools` derived from `py/`) |
| `FN_ROM_HOME` | yes | fn-rom repo root (`build/fujinet.rom` derived after `make`) |
| `BEEBIUM_SERVER` | derived | `beebium-model-b` |
| `BEEBIUM_MOS` / `BEEBIUM_BASIC` | derived | ROMs under `$BEEBIUM_HOME/roms/` |
| `BEEBIUM_PYTHON` | no | Python 3.12+ interpreter for the current Beebium client |
| `FN_ROM` | derived | `$FN_ROM_HOME/build/fujinet.rom` |
| `FUJINET_BIN` | no | real fujinet-nio binary (future `real/` tests) |

Preflight:

```bash
export BEEBIUM_HOME=/path/to/beebium
export FUJINET_NIO_HOME=/path/to/fujinet-nio
export FN_ROM_HOME=/path/to/fn-rom
cd integration-tests/beebium
./check_test_env.sh
```

Targeted app-store regression run:

```bash
BEEBIUM_PYTHON=/usr/bin/python3.14 \
  ./run_pytest.sh scripted/test_appstore_crud.py -q -s
```

## Running

The top-level make targets are:

```bash
make test-bbc
make test-bbc-scripted
make test-bbc-real
```

The scripted lane is populated; the real lane is still scaffolding.
