#!/usr/bin/env python3
"""Install only after explicit session authorization and idle-library confirmation.
Uses the existing palm-sync HotSync workflow, never raw USB or Springboard firmware.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--rollback', action='store_true')
    parser.add_argument('--collector-tools', type=Path, default=ROOT.parent / 'fujinet-palm/tools',
                        help='directory containing prism_collect.py; only needed for authorized installation')
    parser.add_argument('--prc', action='append', choices=('NioLib.prc', 'NioLibTest.prc', 'NioConfig.prc'),
                        help='install only selected PRCs (repeatable); defaults to all three')
    parser.add_argument('--authorized', action='store_true')
    parser.add_argument('--library-idle-confirmed', action='store_true')
    args = parser.parse_args()
    bundle = args.bundle.resolve()
    try:
        manifest = json.loads((bundle / 'manifest.json').read_text())
        names = args.prc or ('NioLib.prc', 'NioLibTest.prc', 'NioConfig.prc')
        files = [bundle / ('rollback' if args.rollback else 'install') / name for name in names]
        for file in files:
            if hashlib.sha256(file.read_bytes()).hexdigest() != manifest['sha256'][str(file.relative_to(bundle))]:
                parser.error('Artifact hash mismatch: ' + file.name)
    except (OSError, ValueError, KeyError) as error:
        parser.error('Missing/invalid install bundle: %s; prepare and verify the bundle first.' % error)
    if not args.authorized or not args.library_idle_confirmed:
        print('DRY RUN: verified %d PRCs.' % len(files), ' Await authorization and old NioLibTest offline PASS, then Home.')
        return
    if not (args.collector_tools / 'prism_collect.py').is_file():
        parser.error('HotSync collector unavailable; supply --collector-tools or install PRCs with your normal HotSync tool.')
    sys.path.insert(0, str(args.collector_tools.resolve()))
    import prism_collect as collector
    output = bundle / ('hotsync-' + time.strftime('%Y%m%d-%H%M%S'))
    output.mkdir(mode=0o700)
    collector.transfer(output, output, '--install', files, 900, transport='palm-sync')
    print('HotSync transfer complete; confirm successful database installation before USB testing.')

if __name__ == '__main__': main()
