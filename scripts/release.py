#!/usr/bin/env python3
"""Prepare or publish a FujiNet-NIO library release from committed sources."""
import argparse
import hashlib
import json
import re
import secrets
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGETS = ('atari-compact', 'atari-original', 'bbc', 'bbc-clib', 'c64',
           'msdos-serial', 'msdos-ioctl', 'msdos-f5', 'amiga', 'amiga-driver', 'linux')


# ADJECTIVES = ('amber', 'bold', 'bright', 'calm', 'clever', 'cosmic', 'daring', 'eager',
#               'gentle', 'golden', 'happy', 'lively', 'lucky', 'merry', 'nimble', 'radiant',
#               'rapid', 'silver', 'steady', 'stellar', 'sunny', 'swift', 'vivid', 'wandering')
# NOUNS = ('badger', 'beacon', 'comet', 'crane', 'dragon', 'falcon', 'firefly', 'fox',
#          'heron', 'lantern', 'lynx', 'meteor', 'orbit', 'otter', 'panda', 'phoenix',
#          'raven', 'rocket', 'sparrow', 'starling', 'tiger', 'voyager', 'willow', 'wren')


ADJECTIVES = (
    'arcane', 'ashen', 'azerite', 'blighted', 'bloodied',
    'celestial', 'chaotic', 'corrupted', 'crimson', 'cursed',
    'darkened', 'demonic', 'draconic', 'dwarven', 'elven',
    'enchanted', 'eternal', 'felbound', 'feral', 'forsaken',
    'frostbitten', 'ghostly', 'glorious', 'gnomish', 'hallowed',
    'haunted', 'heroic', 'holy', 'infernal', 'ironbound',
    'legendary', 'luminous', 'mighty', 'mystic', 'necrotic',
    'netherborn', 'primal', 'radiant', 'runic', 'savage',
    'shadowy', 'spectral', 'stormforged', 'thundering', 'titanic',
    'twilight', 'undying', 'vengeful', 'voidtouched', 'withered',
)

NOUNS = (
    'abomination', 'anubisath', 'banshee', 'basilisk', 'behemoth',
    'chimera', 'deathwing', 'demon', 'direwolf', 'drake',
    'dragonhawk', 'dreadlord', 'dryad', 'elemental', 'felguard',
    'furbolg', 'gargoyle', 'ghoul', 'gnoll', 'golem',
    'gryphon', 'harpy', 'hydra', 'imp', 'infernal',
    'keeper', 'kobold', 'kodo', 'lich', 'manticore',
    'murloc', 'naga', 'nerubian', 'ogre', 'oracle',
    'pandaren', 'phoenix', 'quilen', 'raptor', 'revenant',
    'satyr', 'scourge', 'spirit', 'tauren', 'treant',
    'valkyr', 'warg', 'whelp', 'wendigo', 'wisp',
)

LOCATIONS = (
    'azeroth', 'stormwind', 'ironforge', 'darnassus', 'exodar',
    'orgrimmar', 'thunderbluff', 'undercity', 'silvermoon', 'dalaran',
    'shattrath', 'suramar', 'boralus', 'zuldazar', 'valdrakken',
    'elwynn', 'westfall', 'redridge', 'duskwood', 'stranglethorn',
    'darkshore', 'ashenvale', 'felwood', 'winterspring', 'tanaris',
    'ungoro', 'silithus', 'desolace', 'feralas', 'mulgore',
    'durotar', 'barrens', 'thousandneedles', 'dustwallow', 'azshara',
    'northrend', 'icecrown', 'dragonblight', 'stormpeaks', 'grizzlyhills',
    'outland', 'nagrand', 'netherstorm', 'shadowmoon', 'hellfire',
    'pandaria', 'jadeforest', 'kunlai', 'timelessisle', 'nazjatar',
)

NOUNS = NOUNS + LOCATIONS

def run(args, cwd=ROOT, capture=False, check=True):
    return subprocess.run(args, cwd=cwd, check=check, text=True,
                          stdout=subprocess.PIPE if capture else None,
                          stderr=subprocess.PIPE if capture else None)


def version_tuple(value):
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', value):
        raise ValueError('version must be MAJOR.MINOR.PATCH without a v prefix')
    return tuple(map(int, value.split('.')))


def validate_name(name):
    if not isinstance(name, str) or not name or len(name) > 80 or name != name.strip() or any(ord(c) < 32 or ord(c) == 127 for c in name):
        raise ValueError('release name must be 1–80 printable characters without surrounding whitespace')
    return name


def used_release_names():
    """Read every published release and accessible draft, including all pages."""
    try:
        result = run(['gh', 'api', '--paginate', '--slurp',
                      'repos/{owner}/{repo}/releases?per_page=100'], capture=True)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError('cannot check release name history on GitHub; check gh authentication and network') from exc
    pages = json.loads(result.stdout)
    names = set()
    for page in pages:
        for item in page:
            title = item.get('name') or ''
            match = (re.fullmatch(r'(.+) \(\d+\.\d+\.\d+\)', title) or
                     re.fullmatch(r'FujiNet-NIO library \d+\.\d+\.\d+ — (.+)', title))
            if match:
                names.add(match.group(1).casefold())
    return names


def ensure_unused(name, used):
    if name.casefold() in used:
        raise ValueError(f'release name {name!r} is already used on GitHub')
    return name


def generate_name(used):
    available = [f'{adjective}-{noun}' for adjective in ADJECTIVES for noun in NOUNS
                 if f'{adjective}-{noun}'.casefold() not in used]
    if not available:
        raise RuntimeError('all generated release names have been used; expand the name pool or specify a new name')
    return secrets.choice(available)


def prepared_name(output, used, requested=None):
    """Reuse the reviewed name; give previously prepared releases a stable name."""
    path = output / 'release.json'
    metadata = json.loads(path.read_text())
    current = metadata.get('name')
    if current is not None:
        validate_name(current)
        if requested is not None and requested != current:
            raise ValueError(f'prepared release is already named {current!r}')
        return ensure_unused(current, used)
    name = ensure_unused(validate_name(requested if requested is not None else generate_name(used)), used)
    metadata['name'] = name
    path.write_text(json.dumps(metadata, indent=2) + '\n')
    return name


def create_draft(tag, revision, version, name, output, assets):
    run(['gh', 'release', 'create', tag, '--target', revision, '--draft',
         '--title', f'{name} ({version})',
         '--notes-file', str(output / 'RELEASE_NOTES.md'), *assets])


def previous_tag():
    tags = run(['git', 'tag', '--merged', 'HEAD', '--list', 'v[0-9]*'], capture=True).stdout.splitlines()
    valid = []
    for tag in tags:
        try:
            valid.append((version_tuple(tag[1:]), tag))
        except ValueError:
            pass
    return max(valid)[1] if valid else None


def suggest(tag, messages):
    if not tag:
        return '0.1.0'
    if not messages:
        return None
    major, minor, patch = version_tuple(tag[1:])
    if any('BREAKING CHANGE:' in msg or re.search(r'(?m)^\w+(?:\([^\n]*\))?!:', msg) for msg in messages):
        return f'{major + 1}.0.0'
    if any(re.match(r'(?:feat(?:\([^)]*\))?:|add\b|support\b|implement\b|introduce\b|enable\b)', msg, re.I) for msg in messages):
        return f'{major}.{minor + 1}.0'
    return f'{major}.{minor}.{patch + 1}'


def commit_messages(base, revision):
    span = f'{base}..{revision}' if base else revision
    data = run(['git', 'log', '--reverse', '--no-merges', '--format=%H%x00%s%x00%b%x00', span], capture=True).stdout
    fields = data.split('\0')
    return [(fields[i].strip(), fields[i + 1], fields[i + 2].strip())
            for i in range(0, len(fields) - 2, 3) if fields[i].strip()]


def release_notes(version, base, revision, commits):
    lines = [f'# FujiNet-NIO library {version}', '',
             f'Source commit: `{revision}`.',
             f'Changes since `{base}`.' if base else 'Initial release; changes from the beginning of the repository.',
             '', '## Changes', '']
    lines += [f'- {subject} (`{sha[:10]}`)' for sha, subject, _ in commits]
    stats = run(['git', 'diff', '--no-color', '--stat', base or '4b825dc642cb6eb9a060e54bf8d69288fbee4904', revision], capture=True).stdout.rstrip()
    lines += ['', '## Changed files', '', '```', stats, '```', '',
              'Each platform archive includes its toolchain, build command, source revision and checksum in manifest.json.',
              'Atari HSIO get/set requires FujiNet-NIO firmware implementing FujiDevice GetHsio and SetHsio.', '']
    return '\n'.join(lines)


def tool_version(tool, args):
    result = run([tool, *args], capture=True, check=False)
    lines = (result.stdout + result.stderr).strip().splitlines()
    if not lines:
        raise RuntimeError(f'Could not determine {tool} version')
    return ' / '.join(lines[:2]) if tool == 'wcc' else lines[0]


def export_git(repo, revision, destination):
    tar = destination.parent / (destination.name + '.tar')
    destination.mkdir()
    with tar.open('wb') as f:
        subprocess.run(['git', '-C', str(repo), 'archive', '--format=tar', revision], stdout=f, check=True)
    run(['tar', '-xf', str(tar), '-C', str(destination)])


def write_package(stage, output):
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                info = zipfile.ZipInfo(path.relative_to(stage).as_posix(), (1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o100644 << 16
                z.writestr(info, path.read_bytes())


def prepare(version, revision, base, output, driver, name):
    output.mkdir(parents=True, exist_ok=False)
    commits = commit_messages(base, revision)
    notes = release_notes(version, base, revision, commits)
    (output / 'RELEASE_NOTES.md').write_text(notes)
    (output / 'CHANGELOG.md').write_text(notes)
    driver_commit = run(['git', '-C', str(driver), 'rev-parse', '--verify', 'HEAD^{commit}'], capture=True).stdout.strip()
    tools = {'cc65': tool_version('cc65', ['--version']),
             'watcom': tool_version('wcc', []),
             'amiga': tool_version('m68k-amigaos-gcc', ['--version']),
             'linux': tool_version('gcc', ['--version'])}
    with tempfile.TemporaryDirectory(prefix='nio-release-') as tmp:
        tmp = Path(tmp)
        source = tmp / 'fujinet-nio-lib'
        export_git(ROOT, revision, source)
        export_git(driver, driver_commit, tmp / 'fujinet-nio-driver')
        for target in TARGETS:
            make_target = 'atari' if target.startswith('atari-') else target
            backend = target.split('-', 1)[1] if target.startswith('atari-') else None
            command = ['make', '-f', 'makefiles/build.mk', f'TARGET={make_target}', 'PROGRAM=fujinet-nio']
            if backend:
                command.append(f'ATARI_BACKEND={backend}')
            command.append('lib')
            run(command, cwd=source)
            ext = 'a' if target in ('amiga', 'amiga-driver', 'linux') else 'lib'
            build_name = f'fujinet-nio-{target}.{ext}'
            archive_name = 'fujinet-nio-atari.lib' if backend else build_name
            archive = source / 'build' / build_name
            stage = tmp / ('package-' + target)
            stage.mkdir()
            shutil.copytree(source / 'include', stage / 'include')
            shutil.copy2(archive, stage / archive_name)
            for notice in ('LICENSE', 'THIRD_PARTY.md'):
                shutil.copy2(source / notice, stage / notice)
            tool = 'cc65' if target.startswith(('atari-', 'bbc', 'c64')) else ('watcom' if target.startswith('msdos-') else ('amiga' if target.startswith('amiga') else 'linux'))
            manifest = {'version': version, 'target': target, 'source_commit': revision,
                        'compiler': tools[tool], 'build_command': ' '.join(command),
                        'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest()}
            if target.startswith('amiga'):
                manifest['amiga_driver_commit'] = driver_commit
            (stage / 'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')
            (stage / 'README.txt').write_text('FujiNet-NIO library: ' + target + '\nSee manifest.json for build provenance.\n'
                'Requires compatible FujiNet-NIO firmware. Source: https://github.com/markjfisher/fujinet-nio-lib\n')
            write_package(stage, output / f'fujinet-nio-lib-{target}-{version}.zip')
    assets = sorted(output.glob('*.zip'))
    (output / 'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in assets))
    (output / 'release.json').write_text(json.dumps({'version': version, 'name': name, 'source_commit': revision,
        'previous_tag': base, 'amiga_driver_commit': driver_commit, 'checks': ['make check'],
        'assets': [p.name for p in assets]}, indent=2) + '\n')


def verify_prepared(output, version, revision, base, commits):
    metadata = json.loads((output / 'release.json').read_text())
    if (metadata.get('version') != version or metadata.get('source_commit') != revision or
            metadata.get('previous_tag') != base or metadata.get('checks') != ['make check']):
        raise RuntimeError('prepared metadata does not match the current release')
    expected = sorted(f'fujinet-nio-lib-{target}-{version}.zip' for target in TARGETS)
    if sorted(metadata.get('assets', [])) != expected or sorted(p.name for p in output.glob('*.zip')) != expected:
        raise RuntimeError('prepared release has missing or unexpected packages')
    checksums = []
    for target in TARGETS:
        name = f'fujinet-nio-lib-{target}-{version}.zip'
        data = (output / name).read_bytes()
        checksums.append((name, hashlib.sha256(data).hexdigest()))
        with zipfile.ZipFile(output / name) as z:
            manifest = json.loads(z.read('manifest.json'))
            if manifest.get('version') != version or manifest.get('source_commit') != revision or manifest.get('target') != target:
                raise RuntimeError(f'{name}: manifest does not match the release')
            archive_name = 'fujinet-nio-atari.lib' if target.startswith('atari-') else f'fujinet-nio-{target}.{"a" if target in ("amiga", "amiga-driver", "linux") else "lib"}'
            if hashlib.sha256(z.read(archive_name)).hexdigest() != manifest.get('archive_sha256'):
                raise RuntimeError(f'{name}: archive checksum mismatch')
    expected_sums = ''.join(f'{digest}  {name}\n' for name, digest in sorted(checksums))
    if (output / 'SHA256SUMS').read_text() != expected_sums:
        raise RuntimeError('package checksum list does not match')
    notes = release_notes(version, base, revision, commits)
    if (output / 'RELEASE_NOTES.md').read_text() != notes or (output / 'CHANGELOG.md').read_text() != notes:
        raise RuntimeError('release notes do not match committed history')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('version', nargs='?', help='release version, e.g. 0.1.0')
    p.add_argument('--suggest', action='store_true', help='suggest a version from conventional commit subjects')
    p.add_argument('--since', help='override previous tag for release notes')
    p.add_argument('--output', type=Path, help='new output directory (default build/releases/vVERSION)')
    p.add_argument('--name', help='release name (default: generate a random adjective-noun name)')
    p.add_argument('--driver', type=Path, default=ROOT.parent / 'fujinet-nio-driver')
    p.add_argument('--publish', action='store_true', help='publish prepared assets and create the GitHub tag')
    p.add_argument('--push', action='store_true', help='push the checked master commit before publishing')
    args = p.parse_args()
    if args.name is not None:
        try:
            validate_name(args.name)
        except ValueError as exc:
            p.error(str(exc))
    if args.push and not args.publish:
        p.error('--push requires --publish')
    base = args.since or previous_tag()
    revision = run(['git', 'rev-parse', '--verify', 'HEAD^{commit}'], capture=True).stdout.strip()
    commits = commit_messages(base, revision)
    if args.suggest:
        print(suggest(base, [subject + '\n' + body for _, subject, body in commits]) or f'No changes since {base}')
        return
    if not args.version:
        p.error('provide a version, or use --suggest')
    try:
        number = version_tuple(args.version)
    except ValueError as exc:
        p.error(str(exc))
    if base and not args.since and number <= version_tuple(base[1:]):
        p.error('version must be newer than the latest reachable version tag')
    if base and not commits:
        p.error(f'no commits since {base}')
    if run(['git', 'status', '--porcelain'], capture=True).stdout.strip():
        p.error('commit or remove working-tree changes before preparing a release')
    tag = 'v' + args.version
    if run(['git', 'rev-parse', '--verify', '--quiet', 'refs/tags/' + tag], capture=True, check=False).returncode == 0:
        p.error(f'tag already exists: {tag}')
    output = (args.output or ROOT / 'build' / 'releases' / tag).resolve()
    if output.exists():
        if not args.publish:
            p.error(f'output directory already exists: {output}')
    else:
        used = used_release_names()
        name = ensure_unused(args.name, used) if args.name is not None else generate_name(used)
        output.parent.mkdir(parents=True, exist_ok=True)
        print('Running make check...', flush=True)
        run(['make', 'check'])
        with tempfile.TemporaryDirectory(prefix='.nio-release-', dir=output.parent) as temporary:
            stage = Path(temporary) / 'artifacts'
            prepare(args.version, revision, base, stage, args.driver.resolve(), name)
            stage.rename(output)
    verify_prepared(output, args.version, revision, base, commits)
    name = prepared_name(output, used_release_names(), args.name)
    print(f'Prepared {tag} — {name} at {output}', flush=True)
    if args.publish:
        if args.push:
            branch = run(['git', 'branch', '--show-current'], capture=True).stdout.strip()
            if branch != 'master':
                raise RuntimeError('--push requires the master branch')
            run(['git', 'push', 'origin', 'HEAD:refs/heads/master'])
        # Publish only the exact current default branch, not an unpushed commit.
        remote = run(['git', 'ls-remote', '--exit-code', 'origin', 'refs/heads/master'], capture=True).stdout.split()
        if not remote or remote[0] != revision:
            raise RuntimeError('origin/master does not point to the release commit; push it first')
        assets = sorted(str(p) for p in output.glob('*.zip')) + [str(output / 'SHA256SUMS'), str(output / 'CHANGELOG.md'), str(output / 'release.json')]
        ensure_unused(name, used_release_names())
        create_draft(tag, revision, args.version, name, output, assets)
        run(['gh', 'release', 'edit', tag, '--draft=false'])
        with tempfile.TemporaryDirectory(prefix='nio-release-download-') as downloaded:
            run(['gh', 'release', 'download', tag, '--dir', downloaded])
            for asset in assets:
                path = Path(asset)
                if hashlib.sha256(path.read_bytes()).digest() != hashlib.sha256((Path(downloaded) / path.name).read_bytes()).digest():
                    raise RuntimeError(f'published asset differs from prepared file: {path.name}')
        run(['git', 'fetch', 'origin', f'refs/tags/{tag}:refs/tags/{tag}'])
        if run(['git', 'rev-parse', f'{tag}^{{commit}}'], capture=True).stdout.strip() != revision:
            raise RuntimeError('published tag does not point to the prepared commit')
        print(f'Published and verified {tag}')


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, subprocess.CalledProcessError, OSError, KeyError, ValueError, zipfile.BadZipFile) as exc:
        print(f'release: {exc}', file=sys.stderr)
        sys.exit(1)
