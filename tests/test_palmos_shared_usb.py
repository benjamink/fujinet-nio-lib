"""Actual A4 m68k USB backend/services, mocked HsUs traps and remote peer."""
import os
import struct
import json
import importlib.util
from pathlib import Path
import unittest
import test_palmos_shared as f
from test_palmos_shared_transports import TransportMachine, config
from test_palmos_shared_network import reply, checksum
if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_A0, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC

class USBMachine(TransportMachine):
    def __init__(self):
        self.usb_present = True
        self.usb_loadable = False
        self.usb_api = True
        self.usb_busy = self.usb_close_fail = False
        self.usb_live = False
        self.usb_calls = []
        self.usb_rx = bytearray()
        self.usb_tx = bytearray()
        self.usb_limit = 3
        self.usb_mode = 'normal'
        self.usb_reply = None
        self.usb_send_fail = self.usb_pending_tx = False
        super().__init__()
        self.uc.mem_write(0x4900, struct.pack('>II', 0x4a00, 0))
        self.uc.mem_write(0x4a00, struct.pack('>H', 50))

    def hook(self, uc, pc, size, context):
        if bytes(uc.mem_read(pc, 2)) != b'\x4e\x4f':
            return super().hook(uc, pc, size, context)
        trap, sp = self.read16(pc + 2), uc.reg_read(UC_M68K_REG_A7)
        result = 0
        if trap == 0xa026:
            uc.mem_write(self.read32(sp), bytes(uc.mem_read(self.read32(sp + 4), self.read32(sp + 8))))
        elif trap == 0xa0ba and bytes(uc.mem_read(self.read32(sp), 12)) == b'USB Library\0':
            result = 0 if self.usb_present else 1
            uc.mem_write(self.read32(sp + 4), b'\0\x0a')
        elif trap == 0xa2ac and self.read32(sp + 4) == 0x48735573:
            result = 0 if self.usb_loadable else 1
            uc.mem_write(self.read32(sp + 8), b'\0\x0a')
        elif trap == 0xa0b9 and self.read16(sp) == 10:
            uc.reg_write(UC_M68K_REG_A0, 0x4900)
            uc.mem_write(0x4a00, struct.pack('>H', 50 if self.usb_api else 16))
        elif trap == 0xa0b8 and self.read16(sp) == 10:
            self.usb_calls.append('remove')
        elif 0xa801 <= trap <= 0xa818 and self.read16(sp) == 10:
            result = self.usb_call(trap, sp)
        else:
            return super().hook(uc, pc, size, context)
        uc.reg_write(UC_M68K_REG_D0, result)
        uc.reg_write(UC_M68K_REG_PC, pc + 4)

    def usb_call(self, trap, sp):
        self.usb_calls.append(trap)
        if trap == 0xa801:
            assert self.read16(sp + 2) == 2, 'must explicitly select Generic 2'
            if self.usb_busy: return 0x307
            self.usb_live = True
        elif trap == 0xa802:
            if self.usb_close_fail: return 1
            self.usb_live = False
        elif trap == 0xa80c:  # SendFlush
            self.usb_tx.clear()
        elif trap == 0xa80b:  # SendCheck
            self.uc.mem_write(self.read32(sp + 2), struct.pack('>I', int(self.usb_pending_tx)))
        elif trap == 0xa80f:  # ReceiveCheck
            self.uc.mem_write(self.read32(sp + 2), struct.pack('>I', len(self.usb_rx)))
        elif trap == 0xa817:
            assert self.usb_live
            pointer, count, error = self.read32(sp + 2), self.read32(sp + 6), self.read32(sp + 10)
            self.uc.mem_write(error, b'\0\0')
            if self.usb_send_fail: return 0
            n = min(count, self.usb_limit)
            self.usb_tx += self.uc.mem_read(pointer, n)
            if len(self.usb_tx) >= 6 and len(self.usb_tx) == int.from_bytes(self.usb_tx[2:4], 'little'):
                packet = bytes(self.usb_tx); self.usb_tx.clear()
                answer = self.execute(packet)
                if self.usb_reply is not None: answer = self.usb_reply
                if self.usb_mode == 'silent': answer = b''
                if self.usb_mode == 'header': answer = answer[:3]
                if self.usb_mode == 'oversize': answer = answer[:2] + b'\x01\x04' + answer[4:6]
                if self.usb_mode == 'badcheck': answer = answer[:4] + bytes([answer[4] ^ 1]) + answer[5:]
                if self.usb_mode == 'wrong': answer = reply(0x45, 4)
                if self.usb_mode == 'extra': answer += reply(0x45, 4)
                if self.usb_mode == 'descriptor':
                    answer = bytearray(b'\x45\x01\x06\x00\x00\x80'); answer[4] = checksum(answer)
                self.usb_rx += answer
            return n
        elif trap == 0xa818:
            assert self.read32(sp + 10) == 1
            pointer, count, error = self.read32(sp + 2), self.read32(sp + 6), self.read32(sp + 14)
            n = min(count, self.usb_limit, len(self.usb_rx))
            self.uc.mem_write(error, b'\0\0')
            if self.usb_mode == 'zero': return 0
            if n:
                self.uc.mem_write(pointer, bytes(self.usb_rx[:n])); del self.usb_rx[:n]
            return n
        else:
            raise AssertionError('unexpected USB trap ' + hex(trap))
        return 0

@unittest.skipUnless(f.HAVE_UNICORN and (f.OUT / 'usb.o').exists(), 'requires USB artifacts and Unicorn')
class USBTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        (f.OUT / 'modeled-usb-stack.json').write_text(json.dumps({
            'maximum_observed_call_stack_bytes': getattr(USBMachine, 'peak_stack', 0),
            'scope': 'compiled USB library/core paths, excludes real ROM trap stack',
            'physical_measurement': False}, indent=2) + '\n')

    def machine(self):
        m = USBMachine(); self.assertEqual(m.open(), 0)
        self.assertEqual(m.set_config(config(3)), 0)
        return m

    def test_raw_clock_two_handles_and_clean_owner_handoff(self):
        m = self.machine()
        for _ in range(3):
            _, token, rc = m.begin(); self.assertEqual(rc, 0)
            self.assertEqual(m.clock(token)[:2], (0, 0))
            a, b = m.net_open(token), m.net_open(token)
            self.assertEqual((a[2], b[2]), (0, 0)); self.assertNotEqual(a[1], b[1])
            for h in (a[1], b[1]):
                args = struct.pack('>HIHIIHIII', 7, token, h, 0, 0x8900, 32, 0x8940, 0x8942, 0x8944)
                self.assertEqual(m.call('NioLibNetRead', args), 0)
                self.assertEqual(m.read16(0x8944), 0)
            self.assertEqual(m.end(token)[1:], (0, (12, 2, 2, 0, 0, 0)))
            self.assertFalse(m.usb_live); self.assertFalse(m.sessions)
            m.current_app += 1
        self.assertEqual(m.requests[0], bytes.fromhex('450106004c00'))
        self.assertEqual(m.mmio, 0); self.assertFalse(m.serial_calls)
        self.assertEqual(m.close(), (0, 0))

    def test_missing_unsupported_busy_and_loaded_reference_ownership(self):
        for mode in ('missing', 'unsupported', 'not_handspring', 'busy', 'loaded'):
            m = self.machine()
            m.usb_present = mode != 'missing' and mode != 'loaded'
            m.usb_loadable = mode == 'loaded'
            m.handspring = mode != 'not_handspring'
            m.usb_api = mode != 'unsupported'; m.usb_busy = mode == 'busy'
            _, token, rc = m.begin()
            if mode == 'loaded':
                self.assertEqual(rc, 0); self.assertEqual(m.end(token)[:2], (0, 0))
                self.assertIn('remove', m.usb_calls)
            else: self.assertNotEqual(rc, 0)
            self.assertFalse(m.usb_live)
            if mode != 'loaded': self.assertNotIn('remove', m.usb_calls)

    def test_fail_closed_partial_timeout_invalid_and_unsolicited(self):
        for mode in ('silent', 'header', 'oversize', 'badcheck', 'wrong', 'descriptor', 'zero', 'extra'):
            with self.subTest(mode=mode):
                m = self.machine(); _, token, rc = m.begin(); self.assertEqual(rc, 0)
                m.usb_mode = mode
                self.assertNotEqual(m.clock(token)[1], 0)
                requests = len(m.requests)
                self.assertTrue(m.status()[1][3] & 8)
                self.assertEqual(m.end(token)[2][4], 1)
                self.assertFalse(m.usb_live)
                self.assertEqual(m.close(), (0, 0)); m.open()
                m.usb_rx += reply(0x45, 1)
                self.assertNotEqual(m.begin()[2], 0)
                self.assertEqual(len(m.requests), requests)

    def test_pending_send_deadline_close_failure_and_marker_failure(self):
        for mode in ('send', 'pending', 'pref', 'close'):
            m = self.machine(); _, token, _ = m.begin()
            m.usb_send_fail = mode == 'send'; m.usb_pending_tx = mode == 'pending'
            m.pref_write_failure = mode == 'pref'; m.usb_close_fail = mode == 'close'
            if mode != 'close': self.assertNotEqual(m.clock(token)[1], 0)
            m.end(token)
            if mode == 'close':
                # App Close is balanced; remaining=1 is a library-owned
                # residency hold, not an app reference that must survive exit.
                self.assertEqual(m.close(), (0, 1))
                self.assertTrue(m.globals); self.assertTrue(m.usb_live)
                self.assertEqual(m.read16(m.globals + m.symbols['nl_opens']), 0)
                m.usb_close_fail = False
                self.assertEqual(m.open(), 0); self.assertFalse(m.usb_live)
                self.assertEqual(m.close(), (0, 0))
            else: self.assertFalse(m.usb_live)

    def test_maximum_binary_packet_and_request_validation(self):
        m = self.machine(); _, token, _ = m.begin()
        data = b'\x00\xc0\xdb' * 339
        m.usb_reply = reply(0x45, 1, data)
        self.assertEqual(len(m.usb_reply), 1024)
        request = bytearray(bytes.fromhex('450106004c00'))
        m.uc.mem_write(0x9200, bytes(request))
        def exchange(length=6, capacity=1024):
            return m.call('fn_transport_exchange_buffers', struct.pack('>IHIHI',
                0x9200, length, 0x9800, capacity, 0x9c10), a4=m.globals) & 255
        self.assertEqual(exchange(), 0)
        self.assertEqual(m.read16(0x9c10), 1024)
        self.assertEqual(bytes(m.uc.mem_read(0x9800, 1024)), m.usb_reply)
        before = len(m.requests)
        request[4] ^= 1; m.uc.mem_write(0x9200, bytes(request))
        self.assertNotEqual(exchange(), 0)
        self.assertNotEqual(exchange(1025), 0)
        self.assertEqual(len(m.requests), before)
        # Caller capacity overflow is failure, never a truncated successful read.
        request[4] ^= 1; m.uc.mem_write(0x9200, bytes(request))
        self.assertNotEqual(exchange(capacity=1000), 0)
        self.assertEqual(m.read16(0x9c10), 0)

    def test_read_metadata_reservation_and_cleanup_deadline(self):
        m = self.machine(); _, token, _ = m.begin()
        _, handle, result = m.net_open(token); self.assertEqual(result, 0)
        args = struct.pack('>HIHIIHIII', 7, token, handle, 0, 0x9800, 2000, 0xa100, 0xa102, 0xa104)
        self.assertEqual(m.call('NioLibNetRead', args), 0)
        self.assertEqual(m.read16(0xa104), 0)
        request = next(p for p in m.requests if p[:2] == b'\xfd\x02')
        self.assertEqual(int.from_bytes(request[13:15], 'little'), 1002)
        m.usb_mode = 'silent'; before = m.now
        m.end(token)
        self.assertLessEqual(m.now - before, 201)
        self.assertFalse(m.usb_live)

    def test_existing_compiled_iss_weather_iss_over_usb(self):
        reference = Path(os.environ.get('PALMOS_APPS_REPO', Path(__file__).resolve().parents[2] / 'springboard-nio')).resolve()
        artifacts = Path(os.environ.get('PALMOS_APPS_BUILD', reference / 'palm/build/shared')).resolve()
        harness = reference / 'tests/test_shared_apps.py'
        required = [harness] + [artifacts / app / (app + suffix)
                               for app in ('isstracker', 'weather') for suffix in ('', '.symbols')]
        missing = [str(path) for path in required if not path.is_file()]
        if missing:
            message = 'optional old-app fixtures missing: ' + ', '.join(missing) + '; set PALMOS_APPS_REPO/PALMOS_APPS_BUILD'
            if os.environ.get('PALMOS_REQUIRE_APPS') == '1': self.fail(message)
            self.skipTest(message)
        # The external, unchanged app harness honors this same artifact override.
        os.environ['PALMOS_APPS_BUILD'] = str(artifacts)
        os.environ.setdefault('NIOLIB_ROOT', str(f.ROOT))
        spec = importlib.util.spec_from_file_location('shared_apps', reference / 'tests/test_shared_apps.py')
        apps = importlib.util.module_from_spec(spec); spec.loader.exec_module(apps)
        class USBApps(USBMachine, apps.AppMachine):
            pass
        m = USBApps(); m.present = False  # USB must not depend on the module.
        m.saved_preferences[0x4e694c31, 0x4e34] = 1, config(3)
        for name, identity in [('isstracker', 101), ('weather', 202), ('isstracker', 101)]:
            m.load_app(name, identity)
            self.assertEqual(m.open_url(), 0)
            self.assertEqual(m.client('network_json_parse', struct.pack('>I', 0x9100)), 0)
            self.assertGreater(m.query(), 0)
            m.client('fuji_palmos_close')
            self.assertFalse(m.usb_live); self.assertFalse(m.sessions)
            self.assertFalse(m.globals); self.assertFalse(m.registered)
        self.assertEqual(m.mmio, 0)
        self.assertEqual(len(m.allocations), len(m.frees))

    def test_idle_config_and_active_selection_fixed(self):
        m = self.machine(); self.assertEqual(m.get_config()[1], config(3))
        self.assertFalse(m.usb_calls)
        _, token, _ = m.begin()
        self.assertEqual(m.set_config(config()), 0x8006)
        m.saved_preferences[0x4e694c31, 0x4e34] = 1, config()
        self.assertEqual(m.clock(token)[:2], (0, 0)); self.assertEqual(m.mmio, 0)
        self.assertEqual(m.end(token)[:2], (0, 0))
        self.assertEqual(m.begin()[2], 0); self.assertGreater(m.mmio, 0)

if __name__ == '__main__': unittest.main()
