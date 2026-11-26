#!/usr/bin/env python3
"""Build an Atari distribution from a committed revision, without the workspace."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def run(*args, cwd=ROOT, capture=False):
    return subprocess.run(args, cwd=cwd, check=True, text=True,
                          stdout=subprocess.PIPE if capture else None).stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--revision', default='HEAD', help='committed source revision')
    parser.add_argument('--version', required=True, help='release or candidate version')
    parser.add_argument('--backend', choices=('compact', 'original'), default='compact')
    parser.add_argument('--output', type=Path, required=True, help='output ZIP path')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.+_-]*', args.version):
        parser.error('version must contain only letters, digits, dot, plus, underscore or hyphen')
    commit = run('git', 'rev-parse', '--verify', args.revision + '^{commit}', capture=True).strip()
    version_output = subprocess.run(['cc65', '--version'], check=True, text=True, capture_output=True)
    compiler = (version_output.stdout + version_output.stderr).strip()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='nio-atari-package-') as tmp:
        source = Path(tmp) / 'source'
        source.mkdir()
        tar = Path(tmp) / 'source.tar'
        with tar.open('wb') as f:
            subprocess.run(['git', 'archive', '--format=tar', commit], cwd=ROOT, stdout=f, check=True)
        subprocess.run(['tar', '-xf', str(tar), '-C', str(source)], check=True)
        run('make', '-f', 'makefiles/build.mk', 'TARGET=atari', 'PROGRAM=fujinet-nio',
            'ATARI_BACKEND=' + args.backend, 'lib', cwd=source)
        stage = Path(tmp) / 'package'
        stage.mkdir()
        shutil.copytree(source / 'include', stage / 'include')
        shutil.copy2(source / 'build/fujinet-nio-atari.lib', stage / 'fujinet-nio-atari.lib')
        for name in ('LICENSE', 'THIRD_PARTY.md'):
            shutil.copy2(source / name, stage / name)
        manifest = {'version': args.version, 'source_commit': commit, 'target': 'atari',
                    'backend': args.backend, 'cc65': compiler,
                    'build_command': 'make -f makefiles/build.mk TARGET=atari PROGRAM=fujinet-nio ATARI_BACKEND=' + args.backend + ' lib',
                    'archive_sha256': hashlib.sha256((stage / 'fujinet-nio-atari.lib').read_bytes()).hexdigest()}
        (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
        (stage / 'README.txt').write_text(
            'FujiNet-NIO Atari client library\n\n'
            'Compile with -I<package>/include and link fujinet-nio-atari.lib.\n'
            'See manifest.json for the exact source revision, backend and compiler.\n'
            'Requires compatible FujiNet-NIO firmware; the HSIO get/set API needs\n'
            'firmware with the FujiDevice GetHsio and SetHsio commands.\n'
            'Source: https://github.com/fujinet-nio/fujinet-nio-lib\n')
        # Stable member order and timestamps. Compiler output is recorded in the manifest.
        with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as z:
            for path in sorted(stage.rglob('*')):
                if path.is_file():
                    info = zipfile.ZipInfo(path.relative_to(stage).as_posix(), (1980, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    info.external_attr = 0o100644 << 16
                    z.writestr(info, path.read_bytes())
    print(f'{output}: sha256 {hashlib.sha256(output.read_bytes()).hexdigest()}')


if __name__ == '__main__':
    main()
