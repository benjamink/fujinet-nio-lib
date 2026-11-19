"""Retro68 launcher contract tests; no Docker image pull and no Retro68 install needed."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/mac68k-build.sh'
ECHO = '#!/usr/bin/env python3\nimport json,sys\nprint(json.dumps(sys.argv[1:]))\n'


def tool(directory, name, body=ECHO):
    path = Path(directory) / name
    path.write_text(body)
    path.chmod(0o755)


class LauncherTests(unittest.TestCase):
    def launch(self, bin_dir, *args, **env):
        # A PATH of only the fake tools plus the system directories that hold bash, id and dirname.
        path = os.pathsep.join([str(bin_dir), '/usr/bin', '/bin'])
        return subprocess.run(['bash', str(SCRIPT), *args], cwd=ROOT, capture_output=True, text=True,
                              env=dict(os.environ, PATH=path, **env))

    def test_native_toolchain_runs_the_command_directly(self):
        with tempfile.TemporaryDirectory() as d:
            for name in ('m68k-apple-macos-gcc', 'm68k-apple-macos-ar'):
                tool(d, name)
            tool(d, 'fakemake')
            p = self.launch(d, 'fakemake', 'a=two words')
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertEqual(json.loads(p.stdout), ['a=two words'])

    def test_missing_toolchain_uses_docker_with_arguments_preserved(self):
        with tempfile.TemporaryDirectory() as d:
            tool(d, 'docker')
            p = self.launch(d, 'make', 'value=two words', 'literal=$HOME', MAC68K_IMAGE='example/retro68')
            self.assertEqual(p.returncode, 0, p.stderr)
            argv = json.loads(p.stdout)
            self.assertEqual(argv[0], 'run')
            self.assertIn('example/retro68', argv)
            self.assertIn(f'{ROOT}:/work', argv)
            self.assertEqual(argv[-3:], ['make', 'value=two words', 'literal=$HOME'])

    def test_no_toolchain_and_no_docker_fails_clearly(self):
        with tempfile.TemporaryDirectory() as d:
            p = self.launch(d, 'make')
            # /usr/bin may hold a real docker; only assert when it does not.
            if not Path('/usr/bin/docker').exists() and not Path('/bin/docker').exists():
                self.assertNotEqual(p.returncode, 0)
                self.assertIn('Docker', p.stderr)

    def test_native_backend_without_tools_fails_and_bad_backend_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = self.launch(d, 'make', MAC68K_BUILDER='native')
            self.assertNotEqual(p.returncode, 0)
            self.assertIn('not on PATH', p.stderr)
            p = self.launch(d, 'make', MAC68K_BUILDER='bogus')
            self.assertEqual(p.returncode, 2)


if __name__ == '__main__':
    unittest.main()
