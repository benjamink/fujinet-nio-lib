"""Launcher contract tests; no Docker, SDK acquisition or license acceptance."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherTests(unittest.TestCase):
    def launch(self, toolchain, *args, **env):
        return subprocess.run(['bash', str(ROOT / 'scripts/palmos-build.sh'), *args],
                              cwd=ROOT, capture_output=True, text=True,
                              env=dict(os.environ, PALMOS_BUILDER='container',
                                       PALMDEV_TOOLCHAIN=str(toolchain), **env))

    def test_missing_checkout_fails_without_installing(self):
        with tempfile.TemporaryDirectory() as directory:
            missing = Path(directory) / 'not installed'
            p = self.launch(missing, 'make', PALMOS_SDK_VERSION='3.1')
            self.assertNotEqual(p.returncode, 0)
            self.assertIn('setup.sh', p.stderr)
            self.assertFalse(missing.exists())

    def test_sdk_workspace_and_command_arguments_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            toolchain = Path(directory)
            wrapper = toolchain / 'palm-build'
            wrapper.write_text('#!/usr/bin/env python3\nimport json,sys\nprint(json.dumps(sys.argv[1:]))\n')
            wrapper.chmod(0o755)
            p = self.launch(toolchain, 'make', 'value=two words', 'literal=$HOME',
                            PALMOS_SDK_VERSION='3.5', NIO_WORKSPACE=str(ROOT.parent))
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(json.loads(p.stdout), ['--sdk', '3.5', '--workspace', str(ROOT.parent),
                             '--project', str(ROOT), '--', 'make', 'value=two words', 'literal=$HOME'])
            p = self.launch(toolchain, 'make', PALMOS_SDK_VERSION='9')
            self.assertNotEqual(p.returncode, 0)


if __name__ == '__main__':
    unittest.main()
