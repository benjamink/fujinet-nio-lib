#!/usr/bin/env python3
"""Freeze a verified install/rollback/host bundle offline. Never deploys it."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parent
NAMES = ('NioLib.prc', 'NioLibTest.prc', 'NioConfig.prc')


def revision(repo):
    return {'path': str(repo), 'commit': subprocess.check_output(['git', '-C', str(repo), 'rev-parse', 'HEAD'], text=True).strip(),
            'branch': subprocess.check_output(['git', '-C', str(repo), 'branch', '--show-current'], text=True).strip()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='fresh output directory; never overwrites evidence')
    parser.add_argument('--library-build', type=Path, default=ROOT / 'build/palmos-shared-usb')
    parser.add_argument('--config-repo', type=Path, default=WORKSPACE / 'nio-config')
    parser.add_argument('--config-build', type=Path, help='defaults to CONFIG_REPO/build/palmos-usb')
    parser.add_argument('--host-repo', type=Path, default=WORKSPACE / 'fujinet-nio')
    parser.add_argument('--usb-host', type=Path, help='defaults to HOST_REPO/build/fujibus-visor-usb-debug/fujinet-nio')
    parser.add_argument('--springboard-host', type=Path, help='defaults to HOST_REPO/build/fujibus-link-serial-debug/fujinet-nio')
    parser.add_argument('--rollback-bundle', type=Path, required=True,
                        help='previous verified bundle; use its install PRCs and manifest hashes, never guess rollback files')
    parser.add_argument('--dry-run', action='store_true', help='validate all inputs without copying or deploying')
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists(): parser.error('Choose a fresh output directory; existing evidence is preserved.')
    spec = importlib.util.spec_from_file_location('image', ROOT / 'scripts/palmos_shared_image.py')
    image = importlib.util.module_from_spec(spec); spec.loader.exec_module(image)
    config = args.config_repo.resolve(); host_repo = args.host_repo.resolve()
    config_build = (args.config_build or config / 'build/palmos-usb').resolve()
    hosts = {'usb': (args.usb_host or host_repo / 'build/fujibus-visor-usb-debug/fujinet-nio').resolve(),
             'springboard': (args.springboard_host or host_repo / 'build/fujibus-link-serial-debug/fujinet-nio').resolve()}
    try:
        previous = json.loads((args.rollback_bundle / 'manifest.json').read_text())
        manifest = {'status': 'automated verification only; physical testing pending', 'abi': '1.3',
                    'repositories': {'library': revision(ROOT), 'config': revision(config)},
                    'hosts': {}, 'sha256': {}, 'databases': {}}
        sources = {}
        for name in NAMES:
            sources['install/' + name] = config_build / name if name == 'NioConfig.prc' else args.library_build / name
            old = args.rollback_bundle / 'install' / name
            if hashlib.sha256(old.read_bytes()).hexdigest() != previous['sha256']['install/' + name]:
                parser.error('Rollback artifact mismatch: ' + name)
            sources['rollback/' + name] = old
        expected = {'NioLib.prc': ('libr', 'NiL1', 'NioLib'), 'NioLibTest.prc': ('appl', 'NiT1', 'NioLibTest'),
                    'NioConfig.prc': ('appl', 'NiC1', 'NIO Config')}
        for relative, source in sources.items():
            metadata, resources = image.prc(source.read_bytes())
            if (metadata['type'], metadata['creator'], metadata['name']) != expected[source.name]:
                parser.error('Unexpected PRC identity: ' + str(source))
            if source.name == 'NioLib.prc' and not {(b'libr', 0), (b'nDat', 0)} <= resources.keys():
                parser.error('Not the verified system-library layout')
            manifest['databases'][relative] = metadata
        for transport, binary in hosts.items():
            digest = hashlib.sha256(binary.read_bytes()).hexdigest()
            known = previous.get('hosts', {}).get(transport, {})
            # A copied, previously qualified binary keeps its original source
            # provenance, even when that checkout was archived after testing.
            identity = dict(known) if known.get('sha256') == digest else revision(host_repo)
            identity['binary'] = str(binary); identity['sha256'] = digest
            manifest['hosts'][transport] = identity
            sources['bin/' + transport + '-fujinet-nio'] = binary
        for relative, source in sources.items():
            manifest['sha256'][relative] = hashlib.sha256(source.read_bytes()).hexdigest()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.error('Missing/invalid input: %s. Supply explicit build/repository/rollback paths.' % error)
    if args.dry_run:
        print(json.dumps(manifest, indent=2)); return
    output.mkdir(parents=True, mode=0o700)
    for relative, source in sources.items():
        target = output / relative; target.parent.mkdir(exist_ok=True)
        shutil.copy2(source, target)
        if hashlib.sha256(target.read_bytes()).hexdigest() != manifest['sha256'][relative]:
            raise SystemExit('Source changed while freezing: ' + relative)
        target.chmod(0o555 if relative.startswith('bin/') else 0o444)
    (output / 'logs').mkdir(mode=0o700)
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (output / 'SHA256SUMS').write_text(''.join('%s  %s\n' % (value, name) for name, value in sorted(manifest['sha256'].items())))
    print(output)

if __name__ == '__main__': main()
