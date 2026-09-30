"""Environment resolution for fujinet-nio-lib Beebium integration tests.

Required:

  BEEBIUM_HOME       beebium repository root
  FUJINET_NIO_HOME   fujinet-nio repository root
  FN_ROM_HOME        fn-rom repository root (build/fujinet.rom is loaded in Beebium)

Everything else is derived automatically. Override BEEBIUM_SERVER, FN_ROM, etc.
only when autodetection is wrong for your machine.

Run tests via integration-tests/beebium/run_pytest.sh (not bare ``uv run pytest``).
There is no setup_tests.sh or other venv sync step.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

_CONFIGURED = False

_RUNNER = "integration-tests/beebium/run_pytest.sh"


def _exit(message: str) -> None:
    pytest.exit(
        f"{message}\n"
        f"Required: BEEBIUM_HOME, FUJINET_NIO_HOME, FN_ROM_HOME. "
        f"Run tests via {_RUNNER}. See docs/DEVELOPMENT.md."
    )


def _env_path(name: str) -> Path | None:
    value = os.environ.get(name)
    if value is None or not value.strip():
        return None
    return Path(value.strip()).expanduser().resolve()


def _first_file(*paths: Path) -> Path | None:
    for path in paths:
        if path.is_file():
            return path
    return None


def _first_executable(*paths: Path) -> Path | None:
    for path in paths:
        if path.is_file() and os.access(path, os.X_OK):
            return path
    return None


def resolve_beebium_home() -> Path:
    home = _env_path("BEEBIUM_HOME")
    if home is None:
        _exit("BEEBIUM_HOME must be set")
    if not home.is_dir():
        _exit(f"BEEBIUM_HOME is not a directory: {home}")
    return home


def resolve_beebium_rom_dir(home: Path) -> Path:
    rom_dir = _env_path("BEEBIUM_ROM_DIR")
    if rom_dir is not None:
        if not rom_dir.is_dir():
            _exit(f"BEEBIUM_ROM_DIR is not a directory: {rom_dir}")
        return rom_dir
    default = home / "roms"
    if default.is_dir():
        return default
    _exit(f"No ROM directory found (tried {default}); set BEEBIUM_ROM_DIR")


def resolve_beebium_server(home: Path) -> Path:
    explicit = _env_path("BEEBIUM_SERVER")
    if explicit is not None:
        if not explicit.is_file() or not os.access(explicit, os.X_OK):
            _exit(f"BEEBIUM_SERVER is not an executable file: {explicit}")
        return explicit
    found = _first_executable(
        home / "build-release" / "src" / "server" / "beebium-model-b",
        home / "build" / "src" / "server" / "beebium-model-b",
    )
    if found is not None:
        return found
    _exit(
        "BEEBIUM_SERVER not set and beebium-model-b not found under "
        f"{home}/build-release or {home}/build — build beebium or set BEEBIUM_SERVER"
    )


def resolve_beebium_mos(home: Path) -> Path:
    explicit = _env_path("BEEBIUM_MOS")
    if explicit is not None:
        if not explicit.is_file():
            _exit(f"BEEBIUM_MOS is not a file: {explicit}")
        return explicit
    rom_dir = resolve_beebium_rom_dir(home)
    found = _first_file(
        rom_dir / "acorn-mos_1_20.rom",
        rom_dir / "OS12.ROM",
        rom_dir / "mos.rom",
    )
    if found is not None:
        return found
    _exit(f"No MOS ROM found under {rom_dir}; set BEEBIUM_MOS")


def resolve_beebium_basic(home: Path) -> Path:
    explicit = _env_path("BEEBIUM_BASIC")
    if explicit is not None:
        if not explicit.is_file():
            _exit(f"BEEBIUM_BASIC is not a file: {explicit}")
        return explicit
    rom_dir = resolve_beebium_rom_dir(home)
    found = _first_file(
        rom_dir / "bbc-basic_2.rom",
        rom_dir / "BASIC2.ROM",
        rom_dir / "basic.rom",
    )
    if found is not None:
        return found
    _exit(f"No BASIC ROM found under {rom_dir}; set BEEBIUM_BASIC")


def resolve_fujinet_nio_home() -> Path:
    home = _env_path("FUJINET_NIO_HOME")
    if home is None:
        _exit("FUJINET_NIO_HOME must be set")
    if not home.is_dir():
        _exit(f"FUJINET_NIO_HOME is not a directory: {home}")
    return home


def resolve_fujinet_tools_root() -> Path:
    explicit = _env_path("FUJINET_TOOLS")
    if explicit is not None:
        if explicit.name == "fujinet_tools":
            return explicit.parent
        if (explicit / "fujinet_tools").is_dir():
            return explicit
        if (explicit / "__init__.py").is_file():
            return explicit.parent
        if explicit.is_dir():
            return explicit
        _exit(f"FUJINET_TOOLS is not usable: {explicit}")

    nio = resolve_fujinet_nio_home()
    py_root = nio / "py"
    if (py_root / "fujinet_tools").is_dir():
        return py_root
    _exit(f"fujinet_tools not found under {py_root}")


def resolve_fn_rom_home() -> Path:
    home = _env_path("FN_ROM_HOME")
    if home is None:
        _exit("FN_ROM_HOME must be set to the fn-rom repository root")
    if not home.is_dir():
        _exit(f"FN_ROM_HOME is not a directory: {home}")
    return home


def resolve_fn_rom() -> Path:
    explicit = _env_path("FN_ROM")
    if explicit is not None:
        if not explicit.is_file():
            _exit(f"FN_ROM is not a file: {explicit}")
        return explicit
    fn_rom_home = resolve_fn_rom_home()
    found = fn_rom_home / "build" / "fujinet.rom"
    if found.is_file():
        return found
    _exit(
        f"FN_ROM not set and {found} not found — build fn-rom (make) or set FN_ROM"
    )


def ensure_environment() -> None:
    global _CONFIGURED
    if _CONFIGURED:
        return

    home = resolve_beebium_home()
    nio = resolve_fujinet_nio_home()
    os.environ.setdefault("BEEBIUM_HOME", str(home))
    os.environ.setdefault("BEEBIUM_SERVER", str(resolve_beebium_server(home)))
    os.environ.setdefault("BEEBIUM_MOS", str(resolve_beebium_mos(home)))
    os.environ.setdefault("BEEBIUM_ROM_DIR", str(resolve_beebium_rom_dir(home)))
    os.environ.setdefault("BEEBIUM_BASIC", str(resolve_beebium_basic(home)))
    os.environ.setdefault("FUJINET_NIO_HOME", str(nio))
    os.environ["FUJINET_TOOLS"] = str(resolve_fujinet_tools_root())
    os.environ.setdefault("FN_ROM_HOME", str(resolve_fn_rom_home()))
    os.environ.setdefault("FN_ROM", str(resolve_fn_rom()))

    _CONFIGURED = True


def add_fujinet_tools_to_path() -> Path:
    root = resolve_fujinet_tools_root()
    os.environ["FUJINET_TOOLS"] = str(root)
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))
    return root
