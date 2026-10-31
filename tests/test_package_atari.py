import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]


class AtariPackageTest(unittest.TestCase):
    def test_committed_atari_package_and_manifest(self):
        revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        with tempfile.TemporaryDirectory(prefix='nio-package-test-') as tmp:
            output = Path(tmp) / 'package.zip'
            subprocess.run(['python3', 'scripts/package_atari.py', '--revision', revision,
                            '--version', '0.1.0-test', '--output', str(output)],
                           cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
            with zipfile.ZipFile(output) as package:
                names = set(package.namelist())
                self.assertTrue({'fujinet-nio-atari.lib', 'include/fujinet-nio.h',
                                 'LICENSE', 'THIRD_PARTY.md', 'manifest.json'} <= names)
                manifest = json.loads(package.read('manifest.json'))
                self.assertEqual(manifest['source_commit'], revision)
                self.assertEqual(manifest['version'], '0.1.0-test')
                self.assertEqual(manifest['backend'], 'compact')
                self.assertIn('ATARI_BACKEND=compact', manifest['build_command'])
                self.assertTrue(manifest['cc65'])
                self.assertEqual(manifest['archive_sha256'],
                                 hashlib.sha256(package.read('fujinet-nio-atari.lib')).hexdigest())


if __name__ == '__main__':
    unittest.main()
