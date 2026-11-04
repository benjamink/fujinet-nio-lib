#!/usr/bin/env python3
"""Link every main-header and BBC-header function independently, for both BBC archives.

Unlike discarded C address expressions, an assembly JMP is an unavoidable
relocation. Missing APIs are explicit debt, not silently skipped declarations.
This is link coverage only: unsupported stubs need behavioral tests too.
"""
from pathlib import Path
import json
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / 'tests/bbc_api_capabilities.json').read_text())
header = re.sub(r'/\*.*?\*/', '', ((ROOT / 'include/fujinet-nio.h').read_text() + (ROOT / 'include/fn_bbc.h').read_text()), flags=re.S)
names = set(re.findall(r'\b(fn_\w+)\s*\([^;{}]*\)\s*;', header))
assert names == set(manifest), f'Update BBC capability inventory: {names ^ set(manifest)}'
for target in ('bbc', 'bbc-clib'):
    out = ROOT / 'build/tests/bbc-api' / target
    out.mkdir(parents=True, exist_ok=True)
    for name, capability in manifest.items():
        source = out / (name + '.s')
        source.write_text(f'.export _main\n.import _{name}\n.code\n_main: jmp _{name}\n')
        result = subprocess.run(['cl65', '-t', 'bbc', '-m', str(out / (name + '.map')),
            '-o', str(out / name), str(source), str(ROOT / f'build/fujinet-nio-{target}.lib'),
            str(ROOT / f'obj/{target}/platform/bbc/initenv.o')], capture_output=True, text=True)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        if capability in ('available', 'unsupported-stub'):
            assert result.returncode == 0, f'{target}: {name}: {result.stderr}'
        else:
            assert result.returncode != 0 and re.search(r"Unresolved external [\'‘]_" + re.escape(name) + r"[\'’]", result.stderr), (
                f'{target}: {name} changed availability or failed for another reason: {result.stderr}')
    print(f'{target}: {len(names)} public API classifications verified')
