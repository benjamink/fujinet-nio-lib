import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch
import argparse
import tempfile
import json
import hashlib
import subprocess
import sys
import struct

spec = importlib.util.spec_from_file_location('session', Path(__file__).resolve().parents[1] / 'scripts/palmos_usb_session.py')
session = importlib.util.module_from_spec(spec); spec.loader.exec_module(session)

access_spec = importlib.util.spec_from_file_location('access', Path(__file__).resolve().parents[1] / 'scripts/palmos_usb_access.py')
access = importlib.util.module_from_spec(access_spec); access_spec.loader.exec_module(access)

class PreparationTests(unittest.TestCase):
    def test_access_helper_rejects_hotsync_and_ambiguous_records(self):
        self.assertTrue(access.generic_record(bytes.fromhex('020000010002')))
        for data in ('020000010202', '020000020202', '030000010002', '020000010003', '000000000000', '0000'):
            self.assertFalse(access.generic_record(bytes.fromhex(data)))
        self.assertEqual(access.valid_path('1-9.4'), '1-9.4')
        for path in ('../1-9.4', '1-9.4:1.0', '/sys/bus/usb', '0-0'):
            with self.assertRaises(argparse.ArgumentTypeError): access.valid_path(path)

    def test_summaries_do_not_claim_hardware_success(self):
        result = session.summarize('receive: id=1 dev=0x45 cmd=0x01 params=0\n'
            'send: dev=0x45 status=0 cmd=0x01 payload=12\n'
            'receive: id=2 dev=0xFD cmd=0x01 params=0\n'
            'receive: id=3 dev=0xFD cmd=0x04 params=0\nactive_sessions: 0\n'
            'active=1 generation=2\nUSB timeout\n')
        self.assertEqual(result['clock_get_requests'], 1)
        self.assertEqual(result['network_open_requests'], 1)
        self.assertEqual(result['network_close_requests'], 1)
        self.assertEqual(result['active_session_observations'], ['0'])
        self.assertEqual(result['usb_status_lines'], ['active=1 generation=2'])
        self.assertIn('Requires Palm PASS', result['acceptance'])
        self.assertEqual(result['warning_lines'], ['USB timeout'])

    def test_runner_lifecycle_with_fake_process_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fake = root / 'fake-host'
            fake.write_text("#!/usr/bin/env python3\nimport sys\nfor i in range(2):\n line=sys.stdin.readline()\n print('active_sessions: 0',flush=True)\n")
            fake.chmod(0o700)
            manifest = root / 'manifest.json'
            digest = hashlib.sha256(fake.read_bytes()).hexdigest()
            manifest.write_text(json.dumps({'hosts': {'usb': {'binary': str(fake), 'sha256': digest}},
                'sha256': {'bin/usb-fujinet-nio': digest}}))
            args = argparse.Namespace(transport='usb', port=None, logs=root/'logs', authorized=True,
                                      bundle=root, executable=fake)
            with patch.object(session.subprocess, 'check_output', return_value=''):
                session.run(args)
            self.assertTrue((root/'logs/host.log').exists())
            self.assertEqual(json.loads((root/'logs/summary.json').read_text())['active_session_observations'], ['0', '0'])

class PortableBundleTests(unittest.TestCase):
    def test_explicit_paths_and_clean_layout_without_historical_worktrees(self):
        scripts = Path(__file__).resolve().parents[1] / 'scripts'
        spec = importlib.util.spec_from_file_location('bundle', scripts/'palmos_usb_bundle.py')
        bundle = importlib.util.module_from_spec(spec); spec.loader.exec_module(bundle)
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); old = root/'previous'; (old/'install').mkdir(parents=True)
            lib = root/'lib-build'; lib.mkdir(); cfg = root/'nio-config'; (cfg/'build/palmos-usb').mkdir(parents=True)
            host = root/'fujinet-nio'
            hashes = {}
            # Minimal metadata-only test resources, never executable app or
            # compatibility fixtures. This tooling test requires no other repo.
            for name, kind, creator, database in (
                    ('NioLib.prc', b'libr', b'NiL1', b'NioLib'),
                    ('NioLibTest.prc', b'appl', b'NiT1', b'NioLibTest'),
                    ('NioConfig.prc', b'appl', b'NiC1', b'NIO Config')):
                resources = [(b'libr', 0), (b'nDat', 0)] if kind == b'libr' else [(b'code', 1)]
                header = bytearray(78); header[:len(database)] = database
                struct.pack_into('>H', header, 32, 1)
                header[60:68] = kind + creator
                struct.pack_into('>H', header, 76, len(resources))
                table = b''.join(struct.pack('>4sHI', typ, ident, 80 + 10*len(resources) + n)
                                 for n, (typ, ident) in enumerate(resources))
                data = bytes(header) + table + b'\0\0' + bytes(len(resources))
                (old/'install'/name).write_bytes(data)
                hashes['install/'+name] = hashlib.sha256(data).hexdigest()
                ((cfg/'build/palmos-usb' if name=='NioConfig.prc' else lib)/name).write_bytes(data)
            (old/'manifest.json').write_text(json.dumps({'sha256': hashes, 'hosts': {
                'springboard': {'commit': 'previously-qualified', 'branch': 'historical',
                    'path': '/retired/checkout/not-required',
                    'sha256': hashlib.sha256(b'offline test executable identity').hexdigest()}}}))
            for profile in ('fujibus-visor-usb-debug', 'fujibus-link-serial-debug'):
                binary = host/'build'/profile/'fujinet-nio'; binary.parent.mkdir(parents=True)
                binary.write_bytes(b'offline test executable identity')
            args = ['bundle', '--library-build', str(lib), '--config-repo', str(cfg),
                    '--host-repo', str(host), '--rollback-bundle', str(old), '--output', str(root/'frozen')]
            # Metadata is mocked; all paths/artifact bytes/checksums are real.
            with patch.object(bundle, 'revision', side_effect=lambda repo: {'commit':'fixture', 'branch':'fixture'}):
                with patch.object(sys, 'argv', args+['--dry-run']): bundle.main()
                self.assertFalse((root/'frozen').exists())
                with patch.object(sys, 'argv', args): bundle.main()
            frozen = root/'frozen'; self.assertTrue((frozen/'bin/usb-fujinet-nio').is_file())
            frozen_manifest = json.loads((frozen/'manifest.json').read_text())
            self.assertEqual(frozen_manifest['hosts']['springboard']['commit'], 'previously-qualified')
            self.assertEqual(frozen_manifest['hosts']['usb']['commit'], 'fixture')
            self.assertEqual((frozen/'bin/usb-fujinet-nio').stat().st_mode & 0o222, 0)
            result = subprocess.run([sys.executable, '-B', str(scripts/'palmos_usb_session.py'),
                'host', 'usb', '--bundle', str(frozen), '--logs', str(root/'logs')], capture_output=True, text=True)
            self.assertEqual(result.returncode,0,result.stderr); self.assertIn('DRY RUN',result.stdout)
            self.assertFalse((root/'logs').exists())
            install = subprocess.run([sys.executable, '-B', str(scripts/'palmos_usb_install.py'),
                str(frozen), '--prc', 'NioLib.prc', '--collector-tools', str(root/'missing')], capture_output=True,text=True)
            self.assertEqual(install.returncode,0,install.stderr); self.assertIn('DRY RUN',install.stdout)
            (frozen/'bin/usb-fujinet-nio').chmod(0o700)  # deliberate tampering test
            (frozen/'bin/usb-fujinet-nio').write_bytes(b'tampered')
            bad = subprocess.run([sys.executable, '-B', str(scripts/'palmos_usb_session.py'),
                'host','usb','--bundle',str(frozen),'--logs',str(root/'logs')],capture_output=True,text=True)
            self.assertNotEqual(bad.returncode,0); self.assertIn('hash',bad.stderr)
            # Rebuilding arbitrary rollback bytes cannot silently replace proof.
            (old/'install/NioLib.prc').write_bytes(b'tampered')
            with patch.object(bundle,'revision',return_value={}):
                with patch.object(sys,'argv',args[:-1]+[str(root/'bad'), '--dry-run']):
                    with self.assertRaises(SystemExit): bundle.main()

if __name__ == '__main__': unittest.main()
