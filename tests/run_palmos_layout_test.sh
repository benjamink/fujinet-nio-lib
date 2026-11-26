#!/usr/bin/env bash
# Exercise the real linker/auditor with growth on both sides of the helper group.
set -euo pipefail
cd "$(dirname "$0")/.."
python=${PALMOS_SHARED_PYTHON:-python3}
make palmos-shared
for spec in early:4096 late:8192; do
    position=${spec%:*}
    bytes=${spec#*:}
    directory="${PALMOS_LAYOUT_BUILD_PREFIX:-build/palmos-layout}-${PALMOS_SDK_VERSION:-3.1}-$position"
    make palmos-shared PALMOS_SHARED_BUILD="$directory" \
        PALMOS_LAYOUT_PAD_BYTES="$bytes" PALMOS_LAYOUT_PAD_POSITION="$position"
    "$python" - "$directory" "$bytes" <<'CHECK'
import json
import os
from pathlib import Path
import sys
base = Path(os.environ.get('PALMOS_SHARED_BUILD',
    'build/palmos-shared-' + os.environ.get('PALMOS_SDK_VERSION', '3.1')))
a = json.loads((base / 'verification.json').read_text())
b = json.loads((Path(sys.argv[1]) / 'verification.json').read_text())
assert b['code_bytes'] == a['code_bytes'] + int(sys.argv[2]), 'padding was not linked'
assert b['global_bytes'] == a['global_bytes'], 'padding reserved globals'
assert b['dispatch'] == a['dispatch'], 'public ordinals changed'
assert b['code_bytes'] <= 64720 and b['dispatch_headroom'] >= 0
assert b['relative_ranges']['checked_project_disp16'] > 500
print('Growth fixture verified:', sys.argv[1], 'code:', b['code_bytes'])
CHECK
    PALMOS_SHARED_BUILD="$directory" "$python" scripts/palmos_test_preflight.py
    PALMOS_SHARED_BUILD="$directory" "$python" -m unittest discover \
        -s tests -p test_palmos_shared.py -v
    PALMOS_SHARED_BUILD="$directory" "$python" -m unittest discover \
        -s tests -p test_palmos_shared_network.py -v
done
