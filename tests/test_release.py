import importlib.util
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('nio_release', ROOT / 'scripts/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTest(unittest.TestCase):
    def test_release_name_is_saved_and_reused(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            metadata = out / 'release.json'
            metadata.write_text(json.dumps({'version': '0.2.0'}))
            with patch.object(release, 'generate_name', return_value='bright-comet') as generate:
                self.assertEqual(release.prepared_name(out, set()), 'bright-comet')
                self.assertEqual(release.prepared_name(out, set()), 'bright-comet')
                generate.assert_called_once()
            self.assertEqual(json.loads(metadata.read_text())['name'], 'bright-comet')
            self.assertEqual(release.prepared_name(out, set(), 'bright-comet'), 'bright-comet')
            with self.assertRaises(ValueError):
                release.prepared_name(out, set(), 'other-name')
            with self.assertRaisesRegex(ValueError, 'already used'):
                release.prepared_name(out, {'bright-comet'})

    def test_custom_name_and_github_title(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            (out / 'release.json').write_text('{}')
            self.assertEqual(release.prepared_name(out, set(), 'My release'), 'My release')
            with patch.object(release, 'run') as run:
                release.create_draft('v0.2.0', 'abc123', '0.2.0', 'My release', out, ['artifact.zip'])
            args = run.call_args.args[0]
            self.assertEqual(args[args.index('--title') + 1], 'FujiNet-NIO library 0.2.0 — My release')
            self.assertEqual(args[args.index('--target') + 1], 'abc123')
            self.assertIn('artifact.zip', args)
        for invalid in ('', ' leading', 'trailing ', 'line\nbreak', 'x' * 81, 42):
            with self.assertRaises(ValueError):
                release.validate_name(invalid)

    def test_history_uses_all_pages_and_ignores_unnamed_releases(self):
        pages = [[{'name': 'FujiNet-NIO library 0.1.0'},
                  {'name': 'FujiNet-NIO library 0.2.0 — Feral-Dragon'}],
                 [{'name': 'FujiNet-NIO library 0.3.0 — Mystic-Wisp'}, {'name': None}]]
        with patch.object(release, 'run') as run:
            run.return_value.stdout = json.dumps(pages)
            self.assertEqual(release.used_release_names(), {'feral-dragon', 'mystic-wisp'})
        self.assertIn('--paginate', run.call_args.args[0])
        self.assertIn('--slurp', run.call_args.args[0])

    def test_generated_name_skips_used_and_fails_when_exhausted(self):
        with patch.object(release, 'ADJECTIVES', ('feral',)), \
             patch.object(release, 'NOUNS', ('dragon', 'wisp')):
            self.assertEqual(release.generate_name({'feral-dragon'}), 'feral-wisp')
            with self.assertRaisesRegex(RuntimeError, 'all generated release names'):
                release.generate_name({'feral-dragon', 'feral-wisp'})
        with self.assertRaisesRegex(ValueError, 'already used'):
            release.ensure_unused('FERAL-Dragon', {'feral-dragon'})

    def test_history_failure_does_not_assume_names_are_free(self):
        with patch.object(release, 'run', side_effect=subprocess.CalledProcessError(1, 'gh')):
            with self.assertRaisesRegex(RuntimeError, 'cannot check release name history'):
                release.used_release_names()

    def test_push_requires_explicit_publish(self):
        result = subprocess.run(['python3', str(ROOT / 'scripts/release.py'), '--push', '--suggest'],
                                cwd=ROOT, text=True, capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('--push requires --publish', result.stderr)

    def test_version_and_suggestions(self):
        self.assertEqual(release.version_tuple('0.1.0'), (0, 1, 0))
        for invalid in ('v0.1.0', '0.01.0', '1.2', '1.2.3-beta', '../1.2.3'):
            with self.assertRaises(ValueError): release.version_tuple(invalid)
        self.assertEqual(release.suggest(None, ['feat: first']), '0.1.0')
        self.assertIsNone(release.suggest('v0.1.0', []))
        self.assertEqual(release.suggest('v0.1.0', ['fix: bug']), '0.1.1')
        self.assertEqual(release.suggest('v0.1.0', ['feat(net): feature']), '0.2.0')
        self.assertEqual(release.suggest('v0.1.0', ['Add Atari support']), '0.2.0')
        self.assertEqual(release.suggest('v0.1.0', ['feat(api)!: change']), '1.0.0')
        self.assertEqual(release.suggest('v0.1.0', ['fix: change\n\nBREAKING CHANGE: API']), '1.0.0')

    def test_committed_history_and_notes(self):
        commits = release.commit_messages(None, 'HEAD')
        self.assertTrue(commits)
        self.assertTrue(all(len(sha) == 40 and subject for sha, subject, _ in commits))
        notes = release.release_notes('0.1.0', None, 'HEAD', commits)
        self.assertIn('## Changes', notes)
        self.assertIn('## Changed files', notes)
        self.assertIn(commits[-1][1], notes)

    def test_notes_ignore_always_colour_git_configuration(self):
        config = {'GIT_CONFIG_COUNT': '1', 'GIT_CONFIG_KEY_0': 'color.ui',
                  'GIT_CONFIG_VALUE_0': 'always'}
        with patch.dict(os.environ, config):
            raw = release.run(['git', 'diff', '--stat',
                               '4b825dc642cb6eb9a060e54bf8d69288fbee4904', 'HEAD'],
                              capture=True).stdout
            self.assertIn('\x1b[', raw)
            notes = release.release_notes('0.1.0', None, 'HEAD', [])
        self.assertIn('## Changed files', notes)
        self.assertNotIn('\x1b', notes)

    def test_prepared_assets_are_checked_before_publish(self):
        commits = release.commit_messages(None, 'HEAD')
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            checksums = []
            for target in release.TARGETS:
                name = f'fujinet-nio-lib-{target}-0.1.0.zip'
                archive = ('fujinet-nio-atari.lib' if target.startswith('atari-') else
                           f'fujinet-nio-{target}.{"a" if target in ("amiga", "amiga-driver", "linux") else "lib"}')
                payload = target.encode()
                manifest = {'version': '0.1.0', 'source_commit': 'HEAD', 'target': target,
                            'archive_sha256': hashlib.sha256(payload).hexdigest()}
                with zipfile.ZipFile(out / name, 'w') as z:
                    z.writestr(archive, payload)
                    z.writestr('manifest.json', json.dumps(manifest))
                checksums.append((name, hashlib.sha256((out / name).read_bytes()).hexdigest()))
            (out / 'SHA256SUMS').write_text(''.join(f'{digest}  {name}\n' for name, digest in sorted(checksums)))
            (out / 'release.json').write_text(json.dumps({'version': '0.1.0', 'source_commit': 'HEAD',
                'previous_tag': None, 'checks': ['make check'], 'assets': [name for name, _ in checksums]}))
            notes = release.release_notes('0.1.0', None, 'HEAD', commits)
            for name in ('CHANGELOG.md', 'RELEASE_NOTES.md'):
                (out / name).write_text(notes)
            release.verify_prepared(out, '0.1.0', 'HEAD', None, commits)
            with (out / checksums[0][0]).open('ab') as f:
                f.write(b'bad')
            with self.assertRaises(RuntimeError):
                release.verify_prepared(out, '0.1.0', 'HEAD', None, commits)


if __name__ == '__main__':
    unittest.main()
