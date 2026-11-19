#!/usr/bin/env bash
# Classic Macintosh (Retro68) launcher: runs the given command with the Retro68 toolchain available.
#   MAC68K_BUILDER=auto (default)  use Retro68 from PATH if present, else the Docker image
#   MAC68K_BUILDER=native          require m68k-apple-macos-gcc/-ar on PATH
#   MAC68K_BUILDER=container       always use the Docker image (MAC68K_IMAGE, default ghcr.io/autc04/retro68)
# Docker pulls the image the first time (about 1.5 GB). The image uses the open-source multiversal
# interfaces, so no Apple files are needed.
set -euo pipefail
backend=${MAC68K_BUILDER:-auto}
image=${MAC68K_IMAGE:-ghcr.io/autc04/retro68}
case "$backend" in auto|native|container) ;; *) echo 'MAC68K_BUILDER must be auto, native, or container' >&2; exit 2 ;; esac
[ "$#" -gt 0 ] || { echo 'usage: mac68k-build.sh COMMAND [ARGS...]' >&2; exit 2; }
native=1
for tool in m68k-apple-macos-gcc m68k-apple-macos-ar; do
    command -v "$tool" >/dev/null || native=0
done
if [ "$backend" = native ] || { [ "$backend" = auto ] && [ "$native" = 1 ]; }; then
    [ "$native" = 1 ] || { echo 'Retro68 (m68k-apple-macos-gcc) is not on PATH; use MAC68K_BUILDER=container or see docs/building.md.' >&2; exit 1; }
    exec "$@"
fi
command -v docker >/dev/null || {
    echo 'Retro68 is not on PATH and Docker is not installed: install one of them (docs/building.md, "For classic Macintosh builds").' >&2
    exit 1
}
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
echo "Retro68 not on PATH: building in Docker image $image" >&2
# MAC68K_BUILDER=native inside the container stops the command from recursing into this launcher.
exec docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e MAC68K_BUILDER=native \
    --entrypoint /bin/bash -v "$project":/work -w /work "$image" \
    -c 'export PATH=/Retro68-build/toolchain/bin:$PATH; exec "$@"' mac68k-build "$@"
