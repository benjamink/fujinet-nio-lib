#!/usr/bin/env bash
# Palm-only launcher. Never clones repositories or installs tools/licenses.
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${PALMOS_SDK_VERSION:-3.1}
backend=${PALMOS_BUILDER:-auto}
toolchain=${PALMDEV_TOOLCHAIN:-${NIO_WORKSPACE:+$NIO_WORKSPACE/external/palmdev-toolchain}}
toolchain=${toolchain:-$root/../palmdev-toolchain}
case "$sdk" in 3.1|3.5) ;; *) echo 'PALMOS_SDK_VERSION must be 3.1 or 3.5' >&2; exit 2 ;; esac
case "$backend" in auto|native|container) ;; *) echo 'PALMOS_BUILDER must be auto, native, or container' >&2; exit 2 ;; esac
native=1
for tool in m68k-palmos-gcc m68k-palmos-ar m68k-palmos-objcopy m68k-palmos-nm m68k-palmos-objdump pilrc build-prc python3; do
    command -v "$tool" >/dev/null || native=0
done
if [ "$backend" = native ] || { [ "$backend" = auto ] && [ "$native" = 1 ]; }; then
    [ "$native" = 1 ] || { echo 'Palm tools are missing from PATH; use PALMOS_BUILDER=container or install the tools.' >&2; exit 1; }
    export PALMOS_SDK_VERSION="$sdk"
    exec "$@"
fi
if [ ! -x "$toolchain/palm-build" ]; then
    cat >&2 <<EOF
Palm build tools are unavailable. Install palmdev-toolchain explicitly:
  git clone https://github.com/mozzwald/palmdev-toolchain "$toolchain"
  (in the NIO workspace: git submodule update --init external/palmdev-toolchain)
  cd "$toolchain" && ./setup.sh
Select SDK $sdk and the Handspring SDK for the shared USB library.
Then retry, or set PALMDEV_TOOLCHAIN to an existing checkout.
No toolchain is required for non-Palm targets.
EOF
    exit 1
fi
project=$(pwd)
workspace=${NIO_WORKSPACE:-$(dirname "$project")}
exec "$toolchain/palm-build" --sdk "$sdk" --workspace "$workspace" --project "$project" -- "$@"
