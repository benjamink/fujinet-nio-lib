"""Execute the actual M4 A4 library, stream session, SLIP and common NIO
services under Unicorn. Only Palm OS calls and the remote peer are mocked.
No serial hardware, host service, port or firmware is accessed.
"""
import json
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_network import NetworkMachine, checksum, reply

if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_D0, UC_M68K_REG_D2, UC_M68K_REG_A7, UC_M68K_REG_PC

HAS_M4 = ((f.OUT / 'verification.json').exists() and 'NioLibGetConfig' in
          json.loads((f.OUT / 'verification.json').read_text()).get('dispatch', {}))


def config(transport=1, baud=115200, options=0, version=1, size=24, reserved=0):
    return struct.pack('>4H4I', size, version, transport, options, baud, reserved, 0, 0)


def slip(data):
    return b'\xc0' + data.replace(b'\xdb', b'\xdb\xdd').replace(b'\xc0', b'\xdb\xdc') + b'\xc0'


def unslip(data):
    return data.strip(b'\xc0').replace(b'\xdb\xdc', b'\xc0').replace(b'\xdb\xdd', b'\xdb')


class TransportMachine(NetworkMachine):
    def __init__(self):
        self.serial_opened = False
        self.serial_available, self.new_manager = True, True
        self.serial_busy = self.fail_settings = self.fail_buffer = False
        self.serial_calls, self.serial_tx = [], bytearray()
        self.serial_rx, self.sent = bytearray(), bytearray()
        self.serial_mode, self.serial_reply = 'normal', None
        self.tx_short, self.tx_pending, self.rx_error = False, False, False
        self.send_counter = 0
        self.saved_preferences = {}
        self.zero_progress = False
        self.serial_flags = self.serial_cts = self.baud = None
        super().__init__()

    def hook(self, uc, pc, size, context):
        if bytes(uc.mem_read(pc, 2)) != b'\x4e\x4f':
            return super().hook(uc, pc, size, context)
        trap, sp = self.read16(pc + 2), uc.reg_read(UC_M68K_REG_A7)
        result = 0
        if trap == 0xa367:
            selector = uc.reg_read(UC_M68K_REG_D2)
            result = self.serial_api(selector, sp, False)
        elif trap == 0xa27b and self.read32(sp) == 0x736d6772:  # sysFileCSerialMgr='smgr'
            uc.mem_write(self.read32(sp + 6), struct.pack('>I', int(self.new_manager)))
        elif trap in (0xa2d3, 0xa2d4):
            # Keep saved configuration and unsaved safety markers independent.
            saved = uc.mem_read(sp + 14, 1)[0]
            if not saved:
                return super().hook(uc, pc, size, context)
            key = self.read32(sp), self.read16(sp + 4)
            if trap == 0xa2d3:
                if key not in self.saved_preferences: result = 0xffff
                else:
                    version, data = self.saved_preferences[key]
                    capacity = self.read16(self.read32(sp + 10))
                    uc.mem_write(self.read32(sp + 6), data[:capacity])
                    uc.mem_write(self.read32(sp + 10), struct.pack('>H', len(data)))
                    result = version
            elif not self.pref_write_failure:
                self.saved_preferences[key] = self.read16(sp + 6), bytes(
                    uc.mem_read(self.read32(sp + 8), self.read16(sp + 12)))
        elif trap == 0xa0ba and self.read32(sp) != 0 and bytes(uc.mem_read(self.read32(sp), 14)) == b'Serial Library':
            result = 0 if self.serial_available else 1
            uc.mem_write(self.read32(sp + 4), b'\x00\x09')
        elif 0xa801 <= trap <= 0xa818 and self.read16(sp) == 9:
            selector = {0xa801: 1, 0xa802: 3, 0xa806: 10, 0xa808: 9,
                        0xa80b: 13, 0xa80c: 14, 0xa810: 18, 0xa811: 19,
                        0xa817: 11, 0xa818: 15}[trap]
            result = self.serial_api(selector, sp, True)
        elif trap == 0xa0a0:
            self.now += self.read32(sp)
        else:
            return super().hook(uc, pc, size, context)
        uc.reg_write(UC_M68K_REG_D0, result)
        uc.reg_write(UC_M68K_REG_PC, pc + 4)

    def call(self, name, args, **kw):
        kw.setdefault("budget", 4000000)
        return super().call(name, args, **kw)

    def serial_api(self, selector, sp, old):
        self.serial_calls.append((old, selector))
        if selector == 1:
            if not self.serial_available: return 0x301
            if self.serial_busy: return 0x307
            self.serial_opened = True
            self.baud = self.read32(sp + (4 if old else 4))
            if not old: self.uc.mem_write(self.read32(sp + 8), b'\0\x09')
        elif selector == 3:
            self.serial_opened = False
        elif selector == 10:
            if self.fail_settings: return 1
            if old:
                self.baud = self.read32(self.read32(sp + 2))
                self.serial_flags = self.read32(self.read32(sp + 2) + 4)
                self.serial_cts = self.read32(self.read32(sp + 2) + 8)
            else:
                op, ptr = self.read16(sp + 2), self.read32(sp + 4)
                if op == 3: self.serial_flags = self.read32(ptr)
                elif op == 5: self.serial_cts = self.read32(ptr)
        elif selector == 19:
            if self.fail_buffer and self.read16(sp + 6): return 1
        elif selector == 18:
            self.serial_rx.clear()
        elif selector == 14:
            self.serial_tx.clear()
        elif selector == 13:
            self.uc.mem_write(self.read32(sp + 2), struct.pack('>I', int(self.tx_pending)))
        elif selector == 11:
            assert self.serial_opened
            assert self.read32(sp + 6) == 1
            self.send_counter += 1
            n = 0 if self.tx_short else 1
            self.uc.mem_write(self.read32(sp + 10), b'\0\0')
            if n:
                value = self.uc.mem_read(self.read32(sp + 2), 1)[0]
                self.sent.append(value)
                self.serial_tx.append(value)
                if value == 0xc0 and len(self.serial_tx) > 1:
                    packet = unslip(bytes(self.serial_tx))
                    self.serial_tx.clear()
                    answer = self.execute(packet)
                    if self.serial_reply is not None: answer = self.serial_reply
                    if self.serial_mode == 'silent': pass
                    elif self.serial_mode == 'wrong': self.serial_rx += slip(reply(0x45, 4))
                    elif self.serial_mode == 'badcheck':
                        answer = bytearray(answer); answer[4] ^= 1; self.serial_rx += slip(answer)
                    elif self.serial_mode == 'malformed': self.serial_rx += b'\xc0\xdb\x11\xc0'
                    elif self.serial_mode == 'truncated': self.serial_rx += slip(answer)[:-1]
                    elif self.serial_mode == 'overflow': self.serial_rx += b'\x01' * 2051
                    else: self.serial_rx += slip(answer)
            return n
        elif selector == 15:
            count, out, error = self.read32(sp + 6), self.read32(sp + 2), self.read32(sp + 14)
            assert count == 1
            if self.rx_error:
                self.uc.mem_write(error, b'\x03\x06'); return 0
            if self.serial_rx:
                value = self.serial_rx.pop(0)
                self.uc.mem_write(out, bytes([value])); self.uc.mem_write(error, b'\0\0'); return 1
            self.now += max(1, self.read32(sp + 10))
            self.uc.mem_write(error, b'\x03\x05'); return 0
        return 0

    def get_config(self):
        self.uc.mem_write(0x9000, config())
        err = self.call('NioLibGetConfig', struct.pack('>HII', 7, 0x9000, 0x9020))
        return err, bytes(self.uc.mem_read(0x9000, 24)), self.read16(0x9020)

    def set_config(self, data):
        self.uc.mem_write(0x9000, data)
        return self.call('NioLibSetConfig', struct.pack('>HI', 7, 0x9000))

    def status(self):
        self.uc.mem_write(0x9100, struct.pack('>8H', 16, 0, 0, 0, 0, 0, 0, 0))
        err = self.call('NioLibGetTransportStatus', struct.pack('>HI', 7, 0x9100))
        return err, struct.unpack('>8H', self.uc.mem_read(0x9100, 16))


@unittest.skipUnless(f.HAVE_UNICORN and HAS_M4, 'requires M4 artifacts and Unicorn')
class TransportTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        (f.OUT / 'modeled-serial-stack.json').write_text(json.dumps({
            'maximum_observed_call_stack_bytes': TransportMachine.peak_stack,
            'scope': 'compiled M4 serial/config/client paths; excludes real OS trap stack',
            'physical_measurement': False}, indent=2) + '\n')

    def machine(self, serial=True):
        m = TransportMachine(); self.assertEqual(m.open(), 0)
        if serial: self.assertEqual(m.set_config(config(2)), 0)
        return m

    def test_default_config_persistence_and_no_io_inspection(self):
        m = self.machine(False)
        self.assertEqual(m.get_config(), (0, config(), 1))
        self.assertEqual(m.status()[1][1:3], (1, 0))
        self.assertEqual(m.serial_calls, []); self.assertEqual(m.mmio, 0)
        self.assertFalse(m.saved_preferences)
        self.assertEqual(m.set_config(config(2, 57600, 1)), 0)
        self.assertEqual(m.close(), (0, 0)); m.open()
        self.assertEqual(m.get_config(), (0, config(2, 57600, 1), 0))
        self.assertEqual(m.begin()[2], 0)
        self.assertEqual((m.serial_flags, m.serial_cts, m.baud), (0xf0, 1, 57600))
        self.assertEqual(m.mmio, 0)

    def test_invalid_and_unsupported_settings_never_open_backend(self):
        m = self.machine(False)
        for data in (config(4), config(0), config(99), config(2, 0), config(2, 999999),
                     config(options=2), config(version=2), config(size=22), config(reserved=1)):
            self.assertEqual(m.set_config(data), 0x8001)
        self.assertFalse(m.saved_preferences)
        for version, data in ((2, config(2)), (1, config(4)), (1, b'bad'), (1, config(reserved=1))):
            m.saved_preferences[0x4e694c31, 0x4e34] = version, data
            self.assertEqual(m.get_config(), (0, config(), 2))
            self.assertNotEqual(m.begin()[2], 0)
            self.assertEqual(m.mmio, 0); self.assertFalse(m.serial_calls)
        m.pref_write_failure = True
        self.assertEqual(m.set_config(config()), 0x8005)

    def test_active_selection_ownership_and_reopen(self):
        m = self.machine(); _, token, result = m.begin(); self.assertEqual(result, 0)
        self.assertEqual(m.set_config(config()), 0x8006)
        # Even externally changed settings cannot reroute a running session.
        m.saved_preferences[0x4e694c31, 0x4e34] = 1, config()
        self.assertEqual(m.clock(token)[:2], (0, 0)); self.assertEqual(m.mmio, 0)
        self.assertEqual(m.end(token)[:2], (0, 0)); self.assertFalse(m.serial_opened)
        self.assertEqual(m.begin()[2], 0); self.assertGreater(m.mmio, 0)
        self.assertFalse(m.serial_opened)

    def test_serial_real_common_clock_two_handles_and_cleanup(self):
        m = self.machine(); _, token, result = m.begin(); self.assertEqual(result, 0)
        self.assertEqual(m.clock(token), (0, 0, bytes.fromhex('00f1536500000000')))
        self.assertEqual(bytes(m.sent), slip(bytes.fromhex('450106004c00')))
        a, b = m.net_open(token), m.net_open(token)
        self.assertEqual((a[2], b[2]), (0, 0)); self.assertNotEqual(a[1], b[1])
        for h in (b[1], a[1]):
            args = struct.pack('>HIHIIHIII', 7, token, h, 0, 0x8900, 32, 0x8940, 0x8942, 0x8944)
            self.assertEqual(m.call('NioLibNetRead', args), 0)
            self.assertEqual(m.read16(0x8944), 0)
            self.assertEqual(bytes(m.uc.mem_read(0x8900, m.read16(0x8940))), ('body-%04x' % h).encode())
        self.assertEqual(m.end(token)[1:], (0, (12, 2, 2, 0, 0, 0)))
        self.assertFalse(m.sessions); self.assertFalse(m.serial_opened)
        self.assertEqual(m.mmio, 0)
        self.assertEqual(m.begin()[2], 0)

    def test_old_and_new_manager_settings_release_and_failures(self):
        for old in (False, True):
            for mode in ('normal', 'missing', 'busy', 'settings', 'buffer'):
                m = self.machine(); m.new_manager = not old
                m.serial_available = mode != 'missing'; m.serial_busy = mode == 'busy'
                m.fail_settings = mode == 'settings'; m.fail_buffer = mode == 'buffer'
                _, token, result = m.begin()
                if mode == 'normal':
                    self.assertEqual(result, 0)
                    self.assertEqual((m.serial_flags, m.serial_cts), (0xc0, 1))
                    self.assertEqual(m.clock(token)[:2], (0, 0))
                    self.assertEqual(m.end(token)[:2], (0, 0))
                else: self.assertNotEqual(result, 0)
                self.assertFalse(m.serial_opened)
                self.assertEqual(m.mmio, 0)

    def test_fail_closed_ambiguous_response_and_stale_after_unload(self):
        for mode in ('silent', 'wrong', 'badcheck', 'malformed', 'truncated', 'overflow'):
            m = self.machine(); _, token, _ = m.begin(); m.serial_mode = mode
            self.assertNotEqual(m.clock(token)[1], 0, mode)
            requests = len(m.requests)
            self.assertEqual(m.status()[1][3] & 8, 8)
            self.assertEqual(m.end(token)[2][4], 1)
            self.assertEqual(m.close(), (0, 0)); m.open()
            m.serial_rx += slip(reply(0x45, 1, bytes.fromhex('0100000000f1536500000000')))
            self.assertNotEqual(m.begin()[2], 0)
            self.assertEqual(len(m.requests), requests)  # no retry/new doorbell
            self.assertFalse(m.serial_opened)

    def test_short_tx_line_error_pending_send_and_marker_failure(self):
        for mode in ('txshort', 'line', 'pending', 'pref'):
            m = self.machine(); _, token, _ = m.begin()
            m.tx_short = mode == 'txshort'; m.rx_error = mode == 'line'
            m.tx_pending = mode == 'pending'; m.pref_write_failure = mode == 'pref'
            self.assertNotEqual(m.clock(token)[1], 0)
            if mode == 'pref': self.assertEqual(m.send_counter, 0)
            m.end(token)
            self.assertFalse(m.serial_opened)

    def test_maximum_escaping_capacity_and_oversize_rejection(self):
        m = self.machine(); _, token, _ = m.begin()
        # Explicit-buffer transport is actual compiled code, under same A4.
        request = bytearray(b'\xdb\xc0' * 512)
        request[:6] = bytes([0x45, 1, 0, 4, 0, 0]); request[4] = checksum(request)
        m.serial_reply = reply(0x45, 1, b'\xc0\xdb' * 508 + b'\0')  # 1024 bytes
        m.uc.mem_write(0x9200, bytes(request))
        def exchange(length=1024, capacity=1024):
            return m.call('fn_transport_exchange_buffers', struct.pack('>IHIHI',
                0x9200, length, 0x9800, capacity, 0x9c10), a4=m.globals, budget=4000000) & 255
        self.assertEqual(exchange(), 0)
        self.assertEqual(m.read16(0x9c10), 1024)
        self.assertEqual(bytes(m.uc.mem_read(0x9800, 1024)), m.serial_reply)
        self.assertEqual(len(m.sent), len(slip(bytes(request))))
        before = m.send_counter
        self.assertNotEqual(exchange(1025), 0); self.assertNotEqual(exchange(capacity=1025), 0)
        self.assertEqual(m.send_counter, before)
        # Caller capacity overflow is an error, never a truncated success.
        self.assertNotEqual(exchange(capacity=1000), 0)
        self.assertEqual(m.read16(0x9c10), 0)

    def test_existing_trap_ordinals_and_usb_reserved(self):
        manifest = json.loads((f.OUT / 'verification.json').read_text())['dispatch']
        self.assertEqual(manifest['NioLibClockTimezone'], '0xa812')
        self.assertEqual(manifest['NioLibGetConfig'], '0xa813')
        self.assertEqual(manifest['NioLibSetConfig'], '0xa814')
        self.assertEqual(manifest['NioLibGetTransportStatus'], '0xa815')
        m = self.machine(False)
        has_usb = 'nl_usb_open' in m.symbols
        self.assertEqual(m.set_config(config(3)), 0 if has_usb else 0x8001)
        self.assertEqual(m.get_config()[1], config(3) if has_usb else config())

    def test_developer_screen_uses_real_configuration_traps(self):
        m = TransportMachine()
        self.assertEqual(m.client('config_read'), 0)
        self.assertTrue(m.registered)
        def app_word(name): return m.read16(m.app_data + m.app_symbols[name])
        self.assertEqual(app_word('config_error'), 0)
        m.client('config_action', struct.pack('>H', 1013))  # choose Serial
        m.client('config_action', struct.pack('>H', 1014))  # cycle baud
        m.client('config_action', struct.pack('>H', 1017))  # flow
        m.client('config_action', struct.pack('>H', 1015))  # save/read back
        self.assertEqual(app_word('config_error'), 0)
        self.assertEqual(m.saved_preferences[0x4e694c31, 0x4e34], (1, config(2, 9600, 1)))
        self.assertFalse(m.serial_calls); self.assertEqual(m.mmio, 0)
        m.client('config_close')
        self.assertFalse(m.registered); self.assertFalse(m.globals)
        m.client('config_read')
        m.client('config_action', struct.pack('>H', 1016))  # restore/save default
        self.assertEqual(m.saved_preferences[0x4e694c31, 0x4e34], (1, config()))
        m.client('config_close')
        self.assertEqual(len(m.allocations), len(m.frees))

    def test_serial_response_echo_rejection_and_reentrancy(self):
        m = self.machine(); _, token, _ = m.begin()
        m.uc.mem_write(m.globals + m.symbols['busy'], b'\0\1')
        self.assertEqual(m.clock(token)[0], 0x8006)
        self.assertEqual(m.set_config(config()), 0x8006)
        m.uc.mem_write(m.globals + m.symbols['busy'], b'\0\0')
        _, handle, result = m.net_open(token); self.assertEqual(result, 0)
        # Correct checksum and packet type but a different handle: no copy.
        m.serial_reply = reply(0xfd, 2, struct.pack('<BBHHIH', 1, 1, 0, handle+1, 0, 3)+b'bad')
        m.uc.mem_write(0x8900, b'guard')
        args = struct.pack('>HIHIIHIII', 7, token, handle, 0, 0x8900, 32, 0x8940, 0x8942, 0x8944)
        self.assertEqual(m.call('NioLibNetRead', args), 0)
        self.assertNotEqual(m.read16(0x8944), 0)
        self.assertEqual(bytes(m.uc.mem_read(0x8900, 5)), b'guard')
        m.end(token)

    def test_queued_noise_is_flushed_and_successful_lifetimes_are_fresh(self):
        m = self.machine()
        for i in range(3):
            _, token, result = m.begin(); self.assertEqual(result, 0)
            m.serial_rx += b'garbage\xc0\xdb' + slip(reply(0x45, 4))
            self.assertEqual(m.clock(token)[:2], (0, 0))
            self.assertEqual(m.end(token)[:2], (0, 0))
            self.assertFalse(m.serial_opened)
            self.assertFalse(m.sessions)
            for name in ('wire', 'rx_buffer'):
                address = m.globals + m.symbols[name]
                self.assertEqual(bytes(m.uc.mem_read(address, 2050)), bytes(2050))
            m.current_app += 1
        self.assertEqual(m.mmio, 0)

    def test_read_only_serial_probe_and_all_baud_settings(self):
        m = self.machine()
        m.uc.mem_write(0x9400, struct.pack('>6H', 12, 0, 0, 0, 0, 0))
        self.assertEqual(m.call('NioLibProbe', struct.pack('>HII', 7, 0x9400, 0x9420)), 0)
        self.assertFalse(m.serial_calls); self.assertEqual(m.mmio, 0)
        self.assertEqual(m.read16(0x9402), 0)  # no claim of a Springboard module
        self.assertEqual(bool(m.status()[1][3] & 32), 'nl_usb_open' not in m.symbols)
        for baud in (9600, 19200, 38400, 57600, 115200):
            self.assertEqual(m.set_config(config(2, baud)), 0)
            _, token, result = m.begin(); self.assertEqual(result, 0)
            self.assertEqual(m.baud, baud)
            self.assertEqual(m.end(token)[:2], (0, 0))

    def test_ambiguous_serial_write_is_sent_once_and_exit_is_bounded(self):
        m = self.machine(); _, token, _ = m.begin()
        _, handle, result = m.net_open(token); self.assertEqual(result, 0)
        m.serial_mode = 'silent'
        m.uc.mem_write(0x9500, b'payload')
        args = struct.pack('>HIHIIHII', 7, token, handle, 0, 0x9500, 7, 0x9510, 0x9512)
        self.assertEqual(m.call('NioLibNetWrite', args), 0)
        self.assertNotEqual(m.read16(0x9512), 0)
        self.assertEqual(sum(p[:2] == b'\xfd\x03' for p in m.requests), 1)
        before = m.now
        self.assertEqual(m.end(token)[2][4], 1)
        self.assertLessEqual(m.now - before, 201)
        self.assertFalse(m.serial_opened)

    def test_cleanup_deadline_inside_serial_polls_and_unchanged_client_archive(self):
        m = self.machine(); m.close()
        self.assertEqual(m.client('fn_init'), 0)
        allocations = len(m.allocations)
        self.assertEqual(m.client('fn_init'), 0)
        self.assertEqual(len(m.allocations), allocations)
        self.assertEqual(m.client('fn_clock_get', struct.pack('>I', 0x9600)), 0)
        self.assertEqual(bytes(m.uc.mem_read(0x9600, 8)), bytes.fromhex('000000006553f100'))
        # Run the ordinary client Open; leave the handle for EndClient.
        m.uc.mem_write(0x9700, b'http://example.com/\0')
        self.assertEqual(m.client('fn_open', struct.pack('>IBxIBx', 0x9720, 0, 0x9700, 0)), 0)
        m.serial_mode = 'silent'; before = m.now
        m.client('fn_shutdown')
        self.assertLessEqual(m.now - before, 201)
        self.assertFalse(m.serial_opened)
        self.assertFalse(m.globals); self.assertFalse(m.registered)
        self.assertEqual(len(m.allocations), len(m.frees))
