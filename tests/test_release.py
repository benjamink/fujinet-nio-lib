import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('nio_release', ROOT / 'scripts/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTest(unittest.TestCase):
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
