"""Real compiled m68k services/client + small mocked OS and CS1 peer.

No test substitutes service implementations or the upstream CS1 packet engine.
This is host evidence, not Palm/USB/Internet hardware qualification.
"""
import struct
import json
import unittest
import test_palmos_shared as foundation

try:
    from unicorn import UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_MEM_WRITE
    from unicorn.m68k_const import UC_M68K_REG_A0, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC
except ImportError:
    pass


def checksum(packet):
    result = 0
    for i, byte in enumerate(packet):
        if i != 4:
            result += byte
            result = (result & 255) + (result >> 8)
    return result


def reply(device, command, payload=b'', status=0):
    # Canonical one-byte StatusCode descriptor, plus service payload.
    p = bytearray(struct.pack('<BBHBB', device, command, 7 + len(payload), 0, 1))
    p += bytes([status]) + payload
    p[4] = checksum(p)
    return bytes(p)


class NetworkMachine(foundation.PalmMachine):
    CS1 = 0x29000000

    def __init__(self):
        self.preferences, self.features = {}, {}
        self.current_app, self.present, self.handspring = 1001, True, True
        self.card_base, self.card_size = 0x28000000, 0x1000000
        self.now = 0
        self.regs = {0: 0x464e, 2: 4, 4: 1, 6: 0, 8: 0, 10: 0, 12: 0, 14: 0}
        self.request, self.requests, self.sessions = bytearray(), [], {}
        self.next_handle, self.reply_data = 0x100, b''
        self.silent, self.bad_check, self.wrong_command = False, 0, False
        self.forced_result = 0
        self.wrong_handle, self.short_zone = False, False
        self.pref_write_failure, self.recorded_before_post = False, False
        self.pending_request, self.pending_seq = None, 0
        self.hs_calls, self.mmio, self.doorbells = 0, 0, 0
        self.sequences = []
        self.registered = False
        self.reentrant = None
        super().__init__()
        self.uc.mem_map(self.CS1, 0x100000)
        self.uc.hook_add(UC_HOOK_MEM_READ | UC_HOOK_MEM_WRITE, self.bus,
                         begin=self.CS1, end=self.CS1 + 0xfffff)
        # Independent app A5 image, initialized from actual linked relocations.
        app_blob = (foundation.OUT / 'NioLibTest').read_bytes()
        data_image, data_report = foundation.image.make_image(app_blob)
        data = bytearray(data_image[16:16 + data_report['data_bytes']])
        data += b'\0' * data_report['bss_bytes']
        self.app_data = 0x60000
        for off, kind in data_report['fixups']:
            value = struct.unpack_from('>I', data, off)[0]
            struct.pack_into('>I', data, off, value + (self.APP if kind == 2 else self.app_data))
        self.uc.mem_write(self.app_data, bytes(data))
        self.app_a5 = self.app_data + self.app_symbols['edata']

    def hook(self, uc, pc, size, context):
        self.stack_low = min(self.stack_low, uc.reg_read(UC_M68K_REG_A7))
        if bytes(uc.mem_read(pc, 2)) != b'\x4e\x4f': return
        trap, sp = self.read16(pc + 2), uc.reg_read(UC_M68K_REG_A7)
        if 0xa801 <= trap <= 0xa817:
            target = self.read32(self.ENTRY) + struct.unpack('>h', uc.mem_read(
                self.read32(self.ENTRY) + 2 * (trap - 0xa800), 2))[0]
            uc.mem_write(sp - 4, struct.pack('>I', pc + 4))
            uc.reg_write(UC_M68K_REG_A7, sp - 4)
            uc.reg_write(UC_M68K_REG_PC, target)
            return
        result = 0
        if trap == 0xa0ac:
            uc.mem_write(self.read32(sp), b'\0\0')
            uc.mem_write(self.read32(sp + 4), struct.pack('>I', self.current_app))
        elif trap == 0xa27b:
            key = (self.read32(sp), self.read16(sp + 4))
            if key == (0x68734578, 0):
                if self.handspring: uc.mem_write(self.read32(sp + 6), struct.pack('>I', 1))
                else: result = 1
            elif key in self.features:
                uc.mem_write(self.read32(sp + 6), struct.pack('>I', self.features[key]))
            else: result = 1
        elif trap == 0xa27c:
            self.features[self.read32(sp), self.read16(sp + 4)] = self.read32(sp + 6)
        elif trap == 0xa2d3:
            key = self.read32(sp), self.read16(sp + 4)
            if key not in self.preferences: result = 0xffff
            else:
                version, data = self.preferences[key]
                max_size = self.read16(self.read32(sp + 10))
                uc.mem_write(self.read32(sp + 6), data[:max_size])
                uc.mem_write(self.read32(sp + 10), struct.pack('>H', len(data)))
                result = version
        elif trap == 0xa2d4:
            if not self.pref_write_failure:
                self.preferences[self.read32(sp), self.read16(sp + 4)] = (
                    self.read16(sp + 6), bytes(uc.mem_read(self.read32(sp + 8), self.read16(sp + 12))))
        elif trap == 0xa0f7: result = self.now
        elif trap == 0xa2e9: result = 100
        elif trap == 0xa349:
            self.hs_calls += 1
            assert self.handspring, 'Handspring selector used without feature support'
            assert self.read16(sp) == 6
            card, attr, output = self.read16(sp + 2), self.read16(sp + 4), self.read32(sp + 6)
            if card != 1: result = 1
            elif attr in (0, 1): uc.mem_write(output, bytes([int(self.present)]))
            elif attr in (3, 4): uc.mem_write(output, struct.pack('>I', self.card_base if attr == 3 else self.card_size))
            else: result = 1
        elif trap == 0xa0ba:
            result = 0 if self.registered else 1
            if self.registered: uc.mem_write(self.read32(sp + 4), b'\0\x07')
        elif trap == 0xa2ac:
            assert not self.globals
            self.registered = True
            uc.mem_write(self.read32(sp + 8), b'\0\x07')
        elif trap == 0xa0b8:
            assert not self.globals, 'removal with live library globals'
            self.registered = False
        elif trap in (0xa0c7, 0xa0c5):
            pointer = self.read32(sp if trap == 0xa0c7 else sp + 4)
            text = bytearray()
            while self.uc.mem_read(pointer + len(text), 1)[0]:
                text += self.uc.mem_read(pointer + len(text), 1)
            if trap == 0xa0c7: result = len(text)
            else:
                target = self.read32(sp)
                self.uc.mem_write(target, bytes(text) + b'\0')
                uc.reg_write(UC_M68K_REG_A0, target)
        else:
            return super().hook(uc, pc, size, context)
        uc.reg_write(UC_M68K_REG_D0, result)
        uc.reg_write(UC_M68K_REG_PC, pc + 4)

    def bus(self, uc, access, address, size, value, _):
        assert self.present, 'MMIO after module invalidation'
        assert size == 2, 'peripheral access is not MOVE.W'
        self.mmio += 1
        off = address - self.CS1
        if access == UC_MEM_WRITE:
            if off == 0x10000: self.request = bytearray(); self.wanted = value
            elif off == 0: self.request += struct.pack('>H', value)
            elif off == 0x20000: self.check = value
            elif off == 0x30000:
                self.doorbells += 1
                self.sequences.append(value)
                p = self.preferences.get((0x4e694c31, 0x4e32))
                assert p and struct.unpack('>IIHH', p[1])[:3] == (0x4e4c5031, self.CS1, value)
                self.recorded_before_post = True
                self.regs[6] = value
                if self.bad_check:
                    self.bad_check -= 1
                    self.publish(value, b'', 4)
                elif self.silent:
                    self.regs[4] |= 2
                    self.pending_seq, self.pending_request = value, bytes(self.request[:self.wanted])
                elif self.forced_result: self.publish(value, b'', self.forced_result)
                else: self.publish(value, self.execute(bytes(self.request[:self.wanted])))
        else:
            if off == 8 and self.silent: self.now += 50
            if off >= 0x1000:
                index = off - 0x1000
                result = int.from_bytes(self.reply_data[index:index + 2].ljust(2, b'\0'), 'big')
            else: result = self.regs.get(off, 0)
            uc.mem_write(address, struct.pack('>H', result))

    def publish(self, seq, packet, result=0):
        if self.wrong_handle and packet[:2] == b'\xfd\x02':
            packet = bytearray(packet)
            packet[11:13] = b'\x01\x05'
            packet[4] = checksum(packet)
            packet = bytes(packet)
        self.reply_data = packet
        check = 0
        for i in range(0, len(packet), 2):
            w = int.from_bytes(packet[i:i + 2].ljust(2, b'\0'), 'big')
            check = (((check << 1) | (check >> 15)) + w) & 0xffff
        self.regs.update({4: 1, 8: seq, 10: len(packet), 12: result, 14: check})

    def execute(self, request):
        self.requests.append(request)
        assert len(request) == int.from_bytes(request[2:4], 'little')
        assert checksum(request) == request[4]
        dev, cmd = request[:2]
        if dev == 0x45:
            payload = bytes.fromhex('0100000000f1536500000000') if cmd == 1 else (
                b'\x01\xffUTC' if self.short_zone else b'\x01\x03UTC\x00')
            return reply(dev, 2 if self.wrong_command else cmd, payload)
        handle = int.from_bytes(request[7:9], 'little') if cmd != 1 else 0
        if cmd == 1:
            handle = self.next_handle
            self.next_handle += 1
            body = ('body-%04x' % handle).encode()
            self.sessions[handle] = body
            return reply(dev, cmd, struct.pack('<BBHHB', 1, 0, 0, handle, 3))
        if cmd == 4:
            self.sessions.pop(handle, None)
            return reply(dev, cmd)
        if cmd == 8:
            payload = b'\x01\0\0\0' + bytes([len(self.sessions)])
            payload += b''.join(struct.pack('<HBH', h, 3, len(data)) for h, data in self.sessions.items())
            return reply(dev, cmd, payload)
        if handle not in self.sessions: return reply(dev, cmd, status=0x44)
        if cmd == 5:
            return reply(dev, cmd, struct.pack('<BBHHHII', 1, 6, 0, handle, 200, len(self.sessions[handle]), 0))
        if cmd == 7:
            return reply(dev, cmd, struct.pack('<BBHHI', 1, 1, 0, handle, len(self.sessions[handle])))
        offset = int.from_bytes(request[9:13], 'little')
        length = int.from_bytes(request[13:15], 'little')
        if cmd == 2:
            body = self.sessions[handle][offset:offset + length]
            return reply(dev, cmd, struct.pack('<BBHHIH', 1, 1, 0, handle, offset, len(body)) + body)
        if cmd == 3:
            return reply(dev, cmd, struct.pack('<BBHHIH', 1, 0, 0, handle, offset, length))
        raise AssertionError('unexpected command ' + request.hex())

    def begin(self):
        self.uc.mem_write(0x8300, b'\0' * 8)
        err = self.call('NioLibBeginClient', struct.pack('>HIII', 7, 0x8300, 0x8304, 0x8306))
        return err, self.read32(0x8300), self.read16(0x8306)

    def end(self, token):
        self.uc.mem_write(0x8400, struct.pack('>H', 12) + b'\0' * 10)
        err = self.call('NioLibEndClient', struct.pack('>HIII', 7, token, 0x8400, 0x8410))
        return err, self.read16(0x8410), struct.unpack('>6H', self.uc.mem_read(0x8400, 12))

    def net_open(self, token):
        self.uc.mem_write(0x8500, b'http://example.com/\0')
        self.uc.mem_write(0x8600, struct.pack('>6HII', 20, 0, 0, 0, 0, 0, 0x8500, 0))
        err = self.call('NioLibNetOpen', struct.pack('>HIIII', 7, token, 0x8600, 0x8618, 0x861a))
        return err, self.read16(0x8618), self.read16(0x861a)

    def clock(self, token):
        err = self.call('NioLibClockGet', struct.pack('>HIII', 7, token, 0x8700, 0x8710))
        return err, self.read16(0x8710), bytes(self.uc.mem_read(0x8700, 8))

    def client(self, name, args=b''):
        return self.call(name, args, app=True, a5=self.app_a5) & 255


HAS_NETWORK = ((foundation.OUT / 'verification.json').exists() and 'NioLibBeginClient' in
               json.loads((foundation.OUT / 'verification.json').read_text()).get('dispatch', {}))
@unittest.skipUnless(foundation.HAVE_UNICORN and HAS_NETWORK,
                     'requires M2 artifacts and unicorn==2.1.4')
class NetworkTests(unittest.TestCase):
    @classmethod
    def tearDownClass(cls):
        (foundation.OUT / 'modeled-stack.json').write_text(json.dumps({
            'maximum_observed_call_stack_bytes': NetworkMachine.peak_stack,
            'scope': 'covered compiled library/client paths; mocked OS has no real OS stack cost',
            'physical_measurement': False}, indent=2) + '\n')

    def test_canonical_clock_two_handles_and_cleanup(self):
        m = NetworkMachine(); self.assertEqual(m.open(), 0)
        err, token, status = m.begin(); self.assertEqual((err, status), (0, 0))
        self.assertEqual(m.clock(token), (0, 0, bytes.fromhex('00f1536500000000')))
        self.assertEqual(m.requests[0].hex(), '450106004c00')
        first, second = m.net_open(token), m.net_open(token)
        self.assertEqual((first[0], first[2], second[0], second[2]), (0, 0, 0, 0))
        self.assertNotEqual(first[1], second[1])
        for handle in (first[1], second[1]):
            args = struct.pack('>HIHIIII', 7, token, handle, 0x8800, 0x8804, 0x8808, 0x880a)
            self.assertEqual(m.call('NioLibNetInfo', args), 0)
            self.assertEqual((m.read16(0x8800), m.read16(0x880a)), (200, 0))
            args = struct.pack('>HIHIIHIII', 7, token, handle, 0, 0x8900, 32, 0x8940, 0x8942, 0x8944)
            self.assertEqual(m.call('NioLibNetRead', args), 0)
            self.assertEqual(m.read16(0x8944), 0)
            self.assertEqual(bytes(m.uc.mem_read(0x8900, m.read16(0x8940))), ('body-%04x' % handle).encode())
        self.assertEqual(m.end(token)[:2], (0, 0))
        self.assertFalse(m.sessions)
        self.assertEqual(m.close(), (0, 0))
        self.assertEqual(len(m.allocations), len(m.frees))

    def test_foreground_owner_busy_stale_tokens_and_last_close(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin()
        self.assertEqual(m.begin()[0], 0x8006)
        self.assertEqual(m.close()[0], 0x8006)
        m.current_app = 1002
        self.assertEqual(m.clock(token)[0], 0x8007)
        self.assertEqual(m.end(token)[0], 0x8007)
        m.current_app = 1001
        self.assertEqual(m.clock(token + 1)[0], 0x8007)
        m.uc.mem_write(m.globals + m.symbols['busy'], b'\x00\x01')
        self.assertEqual(m.clock(token)[0], 0x8006)
        m.uc.mem_write(m.globals + m.symbols['busy'], b'\x00\x00')
        self.assertEqual(m.end(token)[:2], (0, 0))
        _, next_token, _ = m.begin()
        self.assertNotEqual(next_token, token)
        self.assertEqual(m.clock(token)[0], 0x8007)
        m.end(next_token); m.close()

    def test_no_handspring_and_removed_module_never_use_mmio(self):
        m = NetworkMachine(); m.open(); m.handspring = False
        self.assertNotEqual(m.begin()[2], 0)
        self.assertEqual((m.hs_calls, m.mmio), (0, 0))
        m.handspring = True; _, token, _ = m.begin()
        count = m.mmio; m.present = False
        self.assertNotEqual(m.clock(token)[1], 0)
        self.assertEqual(m.mmio, count)
        m.end(token); m.close()

    def test_timeout_tombstone_survives_close_reopen_and_late_reply(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin(); m.silent = True
        self.assertNotEqual(m.clock(token)[1], 0)
        self.assertEqual(m.doorbells, 1)
        self.assertTrue(m.recorded_before_post)
        self.assertEqual(m.end(token)[0], 0)
        self.assertEqual(m.close(), (0, 0))
        m.open()
        self.assertNotEqual(m.begin()[2], 0)
        self.assertEqual(m.doorbells, 1)
        m.silent = False
        m.publish(m.pending_seq, m.execute(m.pending_request))
        _, new_token, result = m.begin()
        self.assertEqual(result, 0)
        self.assertEqual(m.clock(new_token)[1], 0)
        self.assertEqual(m.doorbells, 2)
        m.end(new_token); m.close()

    def test_checksum_rejection_retry_is_safe_and_wrong_command_is_not_replayed(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin(); m.bad_check = 1
        self.assertEqual(m.clock(token)[1], 0)
        self.assertEqual((m.doorbells, len(m.requests)), (2, 1))
        m.wrong_command = True
        self.assertNotEqual(m.clock(token)[1], 0)
        self.assertEqual((m.doorbells, len(m.requests)), (3, 2))
        m.end(token); m.close()

    def test_cleanup_total_deadline_and_uncertain_remote_handles(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin()
        m.net_open(token); m.net_open(token); m.silent = True
        start = m.now
        err, status, report = m.end(token)
        self.assertEqual(err, 0)
        self.assertNotEqual(status, 0)
        self.assertLessEqual(m.now - start, 250) # 200 tick budget + one polling step
        self.assertEqual((report[1], report[2], report[3], report[4]), (2, 0, 2, 1))
        self.assertEqual(m.close(), (0, 0))

    def test_failed_tombstone_publication_never_posts_doorbell(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin(); m.pref_write_failure = True
        self.assertNotEqual(m.clock(token)[1], 0)
        self.assertEqual(m.doorbells, 0)
        m.pref_write_failure = False; m.end(token); m.close()

    def test_compatibility_client_idempotent_init_and_shutdown(self):
        m = NetworkMachine()
        self.assertEqual(m.client('fn_init'), 0)
        self.assertEqual(m.client('fn_init'), 0)
        self.assertEqual(m.read16(m.globals + m.symbols['nl_opens']), 1)
        self.assertEqual(m.client('fn_clock_get', struct.pack('>I', 0x9000)), 0)
        self.assertEqual(m.read32(0x9004), 0x6553f100) # Palm uint64 native big-endian
        m.uc.mem_write(0x9100, b'http://example.com/\0')
        # Palm GCC passes uint8_t at the first byte of its two-byte stack
        # slot; UInt16 trap arguments, in contrast, are full big-endian words.
        args = struct.pack('>IBxIBx', 0x9200, 0, 0x9100, 0)
        self.assertEqual(m.client('fn_open', args), 0)
        m.client('fn_shutdown')
        self.assertFalse(m.globals)
        self.assertFalse(m.registered)
        self.assertFalse(m.sessions)

    def test_timezone_write_ready_translation_and_pool_capacity(self):
        m = NetworkMachine(); self.assertEqual(m.client('fn_init'), 0)
        self.assertEqual(m.client('fn_clock_get_timezone', struct.pack('>I', 0x9300)), 0)
        self.assertEqual(bytes(m.uc.mem_read(0x9300, 4)), b'UTC\0')
        self.assertEqual(m.requests[-1].hex(), '450406004f00')
        self.assertEqual(m.reply_data.hex(), '45040d00480100010355544300')
        m.uc.mem_write(0x9100, b'http://example.com/\0')
        m.uc.mem_write(0x9400, b'.latitude\0')
        args = struct.pack('>IBxIBxBxBxI', 0x9200, 0, 0x9100, 0, 1, 0, 0x9400)
        self.assertEqual(m.client('fn_open_translated', args), 0)
        handle = m.read16(0x9200)
        self.assertIn(b'.latitude', m.requests[-1])
        m.uc.mem_write(0x9500, b'payload')
        args = struct.pack('>HIIHI', handle, 0, 0x9500, 7, 0x9600)
        self.assertEqual(m.client('fn_write', args), 0)
        self.assertEqual(m.read16(0x9600), 7)
        self.assertEqual(m.client('fn_ready', struct.pack('>IBxI', 0x9700, 4, 0x9800)), 0)
        self.assertEqual(m.uc.mem_read(0x9800, 1)[0], 1)
        self.assertEqual(m.read16(0x9700), handle)
        # Local pool capacity is discovered from Begin, not an ABI constant.
        cap = m.client('NioLibClientCapacity')
        for _ in range(cap - 1):
            self.assertEqual(m.client('fn_open', struct.pack('>IBxIBx', 0x9202, 0, 0x9100, 0)), 0)
        before = m.doorbells
        self.assertNotEqual(m.client('fn_open', struct.pack('>IBxIBx', 0x9202, 0, 0x9100, 0)), 0)
        self.assertEqual(m.doorbells, before)
        m.client('fn_shutdown'); self.assertFalse(m.sessions)
        self.assertEqual(m.client('fn_init'), 0)
        before = m.doorbells
        self.assertNotEqual(m.client('fn_close', struct.pack('>H', handle)), 0)
        self.assertEqual(m.doorbells, before)
        m.client('fn_shutdown')

    def test_foreign_ready_entries_are_filtered_and_loopback_rejected(self):
        m = NetworkMachine(); m.open(); m.regs[4] = 5
        self.assertNotEqual(m.begin()[2], 0)
        m.regs[4] = 1
        _, token, _ = m.begin()
        _, handle, _ = m.net_open(token)
        m.sessions[0x501] = b'other application orphan'
        args = struct.pack('>HIIHII', 7, token, 0x9000, 8, 0x9050, 0x9052)
        self.assertEqual(m.call('NioLibNetReady', args), 0)
        self.assertEqual(m.read16(0x9050), 1)
        self.assertEqual(m.read16(0x9000), handle)
        m.end(token); m.close()

    def test_no_host_is_not_retried_or_claimed_definitely_unsent(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin()
        m.forced_result = 1
        self.assertNotEqual(m.clock(token)[1], 0)
        self.assertEqual(m.doorbells, 1)
        err, status, report = m.end(token)
        self.assertEqual(err, 0)
        self.assertNotEqual(status, 0)
        self.assertEqual(report[4], 1)
        m.close()

    def test_changed_mapping_and_malformed_pending_metadata_fail_closed(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin()
        before = m.mmio; m.card_base += 0x1000000
        self.assertNotEqual(m.clock(token)[1], 0)
        self.assertEqual(m.mmio, before)
        m.end(token); m.close()
        m = NetworkMachine(); m.open()
        m.preferences[0x4e694c31, 0x4e32] = (99, b'incompatible')
        self.assertNotEqual(m.begin()[2], 0)
        self.assertEqual(m.doorbells, 0)
        self.assertEqual(m.close(), (0, 0))

    def test_bad_echo_and_timezone_length_rejected_before_client_copy(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin()
        _, handle, _ = m.net_open(token)
        m.wrong_handle = True; m.uc.mem_write(0x8900, b'\xee' * 32)
        args = struct.pack('>HIHIIHIII', 7, token, handle, 0, 0x8900, 32, 0x8940, 0x8942, 0x8944)
        self.assertEqual(m.call('NioLibNetRead', args), 0)
        self.assertNotEqual(m.read16(0x8944), 0)
        self.assertEqual(bytes(m.uc.mem_read(0x8900, 32)), b'\xee' * 32)
        m.wrong_handle = False; m.short_zone = True
        m.uc.mem_write(0x9000, b'\xee' * 64)
        args = struct.pack('>HIIHI', 7, token, 0x9000, 64, 0x9050)
        self.assertEqual(m.call('NioLibClockTimezone', args), 0)
        self.assertNotEqual(m.read16(0x9050), 0)
        self.assertEqual(bytes(m.uc.mem_read(0x9000, 64)), b'\xee' * 64)
        m.end(token); m.close()

    def test_pending_ack_and_response_must_both_match(self):
        m = NetworkMachine(); m.open(); _, token, _ = m.begin(); m.silent = True
        self.assertNotEqual(m.clock(token)[1], 0)
        m.end(token); m.close(); m.open(); m.silent = False
        m.publish(m.pending_seq + 1, m.execute(m.pending_request))
        self.assertNotEqual(m.begin()[2], 0)
        self.assertEqual(m.doorbells, 1)
        m.publish(m.pending_seq, m.execute(m.pending_request))
        _, token, result = m.begin()
        self.assertEqual(result, 0)
        self.assertEqual(m.clock(token)[1], 0)
        m.end(token); m.close()

    def test_ambiguous_write_is_not_replayed_by_common_write_loop(self):
        m = NetworkMachine(); self.assertEqual(m.client('fn_init'), 0)
        m.uc.mem_write(0x9100, b'http://example.com/\0')
        self.assertEqual(m.client('fn_open', struct.pack('>IBxIBx', 0x9200, 0, 0x9100, 0)), 0)
        handle = m.read16(0x9200)
        m.uc.mem_write(0x9500, b'payload')
        before = m.doorbells; m.silent = True
        result = m.client('fn_write', struct.pack('>HIIHI', handle, 0, 0x9500, 7, 0x9600))
        self.assertNotEqual(result, 0)
        self.assertEqual(m.doorbells, before + 1)
        m.client('fn_shutdown')
        self.assertEqual(m.doorbells, before + 1)

    def test_failed_generation_allocation_does_not_create_owner(self):
        m = NetworkMachine(); m.open()
        m.features[0x4e694c31, 0x4e33] = 0xffffffff
        self.assertEqual(m.begin()[0], 0x8003)
        self.assertEqual(m.mmio, 0)
        self.assertEqual(m.close(), (0, 0))

    def test_sequence_wrap_skips_zero_and_keeps_independent_completions(self):
        m = NetworkMachine(); m.open(); m.regs[6] = m.regs[8] = 254
        _, token, result = m.begin(); self.assertEqual(result, 0)
        for _ in range(3): self.assertEqual(m.clock(token)[1], 0)
        self.assertEqual(m.sequences, [255, 1, 2])
        self.assertEqual(m.end(token)[:2], (0, 0))
        self.assertEqual(m.close(), (0, 0))


if __name__ == '__main__':
    unittest.main()
