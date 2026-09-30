# Developer setup (fujinet-nio-lib)

## Run BBC Beebium tests

Build the library and fn-rom sideways ROM, set three paths, run tests:

```bash
make bbc
make -C /path/to/fn-rom       # produces build/fujinet.rom

export BEEBIUM_HOME=/path/to/beebium
export FUJINET_NIO_HOME=/path/to/fujinet-nio
export FN_ROM_HOME=/path/to/fn-rom

make test-bbc-scripted
```

No separate venv sync step — `run_pytest.sh` attaches the Beebium client from
your checkout automatically via `uv run --with-editable`.

Everything else is derived:

| Derived | From |
|---------|------|
| `beebium-model-b` | `$BEEBIUM_HOME/build-release/...` or `$BEEBIUM_HOME/build/...` |
| MOS / BASIC ROMs | `$BEEBIUM_HOME/roms/` |
| `fujinet_tools` | `$FUJINET_NIO_HOME/py` |
| `FN_ROM` | `$FN_ROM_HOME/build/fujinet.rom` |

Override any derived path with the usual env var if autodetection fails.

## Prerequisites

- **cc65** (`cl65`) — compile BBC smoke apps in tests
- **beebium** built (`beebium-model-b` under your `BEEBIUM_HOME` checkout)
- **dfstool** — SSD images for disc-based smoke tests
- **fn-rom** built (`make` in your `FN_ROM_HOME` checkout)

Optional for future `real/` interop tests: build fujinet-nio and set `FUJINET_BIN`.

## Verify environment

```bash
cd integration-tests/beebium
./check_test_env.sh          # quick preflight + pytest collect smoke
./run_pytest.sh scripted/ -q   # run scripted lane directly
```

Set `CHECK_TEST_ENV_SMOKE=0` to skip the collect-only smoke test in
`check_test_env.sh`.

## Common commands

```bash
make bbc
make test-bbc-scripted
cd integration-tests/beebium && ./run_pytest.sh scripted/test_http_get.py -q
```

## Further reading

- [integration-tests/beebium/README.md](../integration-tests/beebium/README.md) — scaffold layout
- [docs/building.md](building.md) — all platform builds
- fn-rom [docs/DEVELOPMENT.md](https://github.com/FujiNetWIFI/fn-rom/blob/master/docs/DEVELOPMENT.md) — fuller Beebium matrix (same patterns)
