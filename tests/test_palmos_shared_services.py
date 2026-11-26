"""Compiled common services and Palm client traps over a scripted FujiBus peer.
The peer supplies wire responses; library implementations are never replaced.
"""
import re
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_network import NetworkMachine, reply, checksum
from test_palmos_shared_transports import TransportMachine, config
from test_palmos_shared_usb import USBMachine
if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC


# Palm GCC passes byte-sized C arguments in the high byte of a two-byte
# stack slot. Trap arguments deliberately use UInt16 and remain full words.
BYTE_ARGS = {}
for source in [(f.ROOT / 'src/platform/palmos/shared/client.c'),
               *(f.ROOT / 'src/platform/palmos/shared/client').glob('*.c')]:
    for match in re.finditer(r'^(?:uint8_t|bool|void)\s+((?:fn_|fuji_)\w+)\((.*?)\)\s*\{',
                             source.read_text(), re.M | re.S):
        BYTE_ARGS[match[1]] = [i for i, param in enumerate(match[2].split(','))
            if '*' not in param and any(t in param for t in ('uint8_t', 'FnTimeFormat', 'enum AppKeySize'))]


def le16(data, at):
    return int.from_bytes(data[at:at + 2], 'little')


def text(data, at):
    size = le16(data, at)
    return data[at + 2:at + 2 + size], at + 2 + size


class ServiceMachine(NetworkMachine):
    def __init__(self):
        self.files, self.values = {}, {}
        self.bad_file_echo = False
        self.service_error = 0
        super().__init__()

    def hook(self, uc, pc, size, context):
        if (getattr(self, 'old_services', False) or hasattr(self, 'service_version')) and bytes(uc.mem_read(pc, 2)) == b'\x4e\x4f':
            trap = self.read16(pc + 2)
            sp = uc.reg_read(UC_M68K_REG_A7)
            if trap in (0xa805, 0xa806) and self.read16(sp) == 7:
                payload = (struct.pack('>4H', *getattr(self, 'service_version', (8, 1, 5, 8)))
                    if trap == 0xa805 else struct.pack('>I', getattr(self, 'service_caps', 0x800000ff)))
                uc.mem_write(self.read32(sp + 2), payload)
                uc.reg_write(UC_M68K_REG_D0, 0)
                uc.reg_write(UC_M68K_REG_PC, pc + 4)
                return
        return super().hook(uc, pc, size, context)

    def execute(self, request):
        dev, cmd = request[:2]
        if dev == 0xfd:
            response = super().execute(request)
            if cmd == 1 and getattr(self, 'bad_open', False):
                return reply(dev, cmd, b'\x01\0')
            return response
        self.requests.append(request)
        assert checksum(request) == request[4]
        p = request[6:]
        if self.service_error:
            return reply(dev, cmd, b'\x01\x0b' if dev == 0xfc else b'', self.service_error)
        if dev == 0x70:
            if cmd in (2, 3):
                if cmd == 3: self.hsio = p[1]
                return reply(dev, cmd, bytes([1, getattr(self, 'hsio', 0xff)]))
            assert p == b'\x01'  # highest supported negotiation version
            return reply(dev, cmd, b'\x01\x03v1!\x04test')
        if dev == 0xf3:
            if cmd == 1:
                out = b'\x01\x02\x01\x01\x01\xc8' + bytes(range(6))
                out += b'\x071.2.3.4\x00\x00\x00' + struct.pack('<HB', 31, 1)
            elif cmd == 2: out = b'\x01\x01\x01\x03net\x00'
            elif cmd == 3: out = b'\x01'
            elif cmd == 4: out = b'\x01\x00\x01\x03net' + bytes(range(6)) + b'\xc8\x06\x03'
            elif cmd == 5:
                assert p == b'\x01'
                out = b'\x01\x01' + bytes(range(6))
            elif cmd == 6: out = b'\x01'
            elif cmd == 7: out = b'\x01\x02'
            else: raise AssertionError(request.hex())
            return reply(dev, cmd, out)
        if dev == 0x45:
            if cmd in (1, 2, 7): out = b'\x01\0\0\0' + (p[1:9] if cmd == 2 else struct.pack('<Q', 0x123456789abcdef0))
            elif cmd == 3:
                out = bytes([1, p[1]]) + (b'2026-11-11T12:34:56+0000' if p[1] in (3, 4)
                    else bytes(range({0: 7, 1: 4, 2: 6, 5: 16}[p[1]])))
            elif cmd == 4: out = b'\x01\x03UTC\0'
            else: out = b'\x01'
            return reply(dev, cmd, out)
        if dev == 0xfe:
            uri, at = text(p, 1)
            if cmd == 1:
                out = b'\x01\x02\0\0' + struct.pack('<QQ', len(self.files.get(uri, b'')), 123)
            elif cmd == 6: out = b'\x01\0\0\0'
            elif cmd in (3, 4):
                offset, count = struct.unpack_from('<IH', p, at)
                if cmd == 4:
                    data = p[at + 6:at + 6 + getattr(self, 'write_accepted', count)]
                    old = self.files.get(uri, b'') if offset else b''
                    self.files[uri] = old[:offset] + data + old[offset + len(data):]
                else: data = self.files.get(uri, b'')[offset:offset + count]
                out = struct.pack('<BBHIH', 1, 1 if cmd == 3 else 0, 0,
                                  offset + int(self.bad_file_echo), len(data))
                if cmd == 3: out += data
                if cmd == 4:
                    mode = getattr(self, 'write_ack', '')
                    if mode == 'echo': out = out[:4] + struct.pack('<I', offset + 1) + out[8:]
                    elif mode == 'short': out = out[:9]
                    elif mode == 'version': out = b'\x02' + out[1:]
                    elif mode == 'reserved': out = out[:2] + b'\x01' + out[3:]
                    elif mode == 'overcount': out = out[:8] + struct.pack('<H', count + 1)
            elif cmd == 7:
                start, page_size = struct.unpack_from('<HB', p, at)
                entry = b'\0\x07App.prc' + struct.pack('<II', 3, 123)
                out = struct.pack('<BBHIHBB', 1, 0, 0, 1, start, page_size, 1)
                out += b'\x01' + struct.pack('<H', len(entry)) + entry
            elif cmd == 5:
                canonical, display = b'host:/App.prc', b'/App.prc'
                out = b'\x01\x02\0\0' + struct.pack('<H', len(canonical)) + canonical
                out += struct.pack('<H', len(display)) + display
            else: raise AssertionError(request.hex())
            return reply(dev, cmd, out)
        if dev == 0xf1:
            ns, at = text(p, 1)
            key, at = text(p, at)
            k = ns, key
            if cmd == 1: out = b'\x01\x01\0\0' + struct.pack('<QQ', len(self.values.get(k, b'')), 123)
            elif cmd in (2, 3):
                offset, count = struct.unpack_from('<IH', p, at)
                if cmd == 3:
                    data = p[at + 6:at + 6 + count]
                    self.values[k] = data
                else: data = self.values.get(k, b'')[offset:offset + count]
                out = struct.pack('<BBHIH', 1, 1, 0, offset, len(data))
                if cmd == 2: out += data
            elif cmd == 4:
                self.values.pop(k, None)
                out = b'\x01\x01\0\0'
            elif cmd == 5: out = struct.pack('<BBHHHH', 1, 0, 0, le16(p, at), 1, 3) + b'\x01\0k'
            else: raise AssertionError(request.hex())
            return reply(dev, cmd, out)
        if dev == 0xf2:
            if cmd in (1, 2): out = bytes([1, 1, p[1]]) + struct.pack('<H', 6) + b'host:/'
            elif cmd == 3: out = bytes([1, 1, p[1]])
            elif cmd == 4: out = struct.pack('<BBBBBH', 1, 0, p[3], 1, 0, 0) + b'\0'
            else: raise AssertionError(request.hex())
            return reply(dev, cmd, out)
        if dev == 0xfc:
            if cmd in (1, 5, 10): out = struct.pack('<BBHBBHI', 1, 3, 0, p[1], 4, 512, 720)
            elif cmd in (2, 6, 14): out = b'\x01\0\0\0' + p[1:2]
            elif cmd in (3, 4):
                offset, count = struct.unpack_from('<IH', p, 2)
                data = b'ABCD' if cmd == 3 else p[8:8 + count]
                out = struct.pack('<BBHBIH', 1, 0, 0, p[1], offset, len(data))
                if cmd == 3: out += data
            elif cmd == 15: out = struct.pack('<BBHIH', 1, 4, 512, 720, 3) + b'ABC'
            else: raise AssertionError(request.hex())
            return reply(dev, cmd, out)
        raise AssertionError(request.hex())


@unittest.skipUnless(f.HAVE_UNICORN and (f.OUT / 'verification.json').exists(), 'requires compiled Palm services')
class ServiceTests(unittest.TestCase):
    def setUp(self):
        self.m = ServiceMachine()
        self.cursor = 0x70000
        self.assertEqual(self.m.call('fn_init', b'', app=True, a5=self.m.app_a5), 0)

    def tearDown(self):
        self.m.call('fn_shutdown', b'', app=True, a5=self.m.app_a5)

    def alloc(self, data=b'', size=1024):
        address = self.cursor
        self.cursor += (max(size, len(data)) + 1) & ~1
        self.m.uc.mem_write(address, data + b'\0' * (max(size, len(data)) - len(data)))
        return address

    def io(self):
        return self.alloc(struct.pack('>IH', self.alloc(), 1024))

    def call(self, name, fmt='', *args):
        packed = [value << 8 if i in BYTE_ARGS.get(name, []) else value
                  for i, value in enumerate(args)]
        return self.m.call(name, struct.pack('>' + fmt, *packed), app=True, a5=self.m.app_a5)

    def service_client(self, transport):
        if transport == 1:
            m = ServiceMachine()
        else:
            backend = TransportMachine if transport == 2 else USBMachine
            m = type('RegressionServiceMachine', (ServiceMachine, backend), {})()
            self.assertEqual(m.open(), 0)
            m.uc.mem_write(0x8000, config(transport))
            self.assertEqual(m.call('NioLibSetConfig', struct.pack('>HI', 7, 0x8000)), 0)
            self.assertEqual(m.close(), (0, 0))
        def app(name, fmt='', *args):
            return m.call(name, struct.pack('>' + fmt, *args), app=True, a5=m.app_a5)
        self.assertEqual(app('fn_init'), 0)
        m.uc.mem_write(0x71000, struct.pack('>IH', 0x70000, 1024))
        m.uc.mem_write(0x72000, b'host:/App.prc\0')
        m.uc.mem_write(0x73000, b'PRC')
        m.uc.mem_write(0x74000, b'\xa5' * 16)
        return m, app

    def finish_client(self, m, app, expected):
        app('fn_shutdown')
        app('NioLibClientCleanupReport', 'I', 0x76000)
        self.assertEqual(app('NioLibClientLastResult'), expected)
        self.assertEqual(struct.unpack('>6H', m.uc.mem_read(0x76000, 12)),
                         (12, 0, 0, 0, int(expected != 0), 0))
        self.assertEqual(m.globals, 0)

    def test_malformed_write_ack_latches_uncertainty_on_every_transport(self):
        for transport in (1, 2, 3):
            for mode in ('echo', 'short', 'version', 'reserved', 'overcount'):
                with self.subTest(transport=transport, mode=mode):
                    m, app = self.service_client(transport)
                    m.write_ack = mode
                    self.assertEqual(app('fn_file_write', 'IIIIHI', 0x71000, 0x72000,
                                         0, 0x73000, 3, 0x74000), 5)
                    self.assertEqual(m.files[b'host:/App.prc'], b'PRC')
                    self.assertEqual(bytes(m.uc.mem_read(0x74000, 8)), b'\xa5' * 8)
                    self.assertEqual(len(m.requests), 1)  # never replay
                    self.assertEqual(app('fn_fuji_get_info', 'I', 0x75000), 0)
                    self.finish_client(m, app, 6)  # a later query cannot clear it
                    self.assertEqual(len([r for r in m.requests if r[:2] == b'\xfe\x04']), 1)

    def test_local_write_failures_and_rejected_requests_have_clean_cleanup(self):
        for transport in (1, 2, 3):
            with self.subTest(transport=transport):
                m, app = self.service_client(transport)
                # Seed an earlier successful result: it cannot authorize this call.
                self.assertEqual(app('fn_fuji_get_info', 'I', 0x75000), 0)
                before = len(m.requests)
                for index in (0, 1, 3, 5):
                    args = [0x71000, 0x72000, 0, 0x73000, 3, 0x74000]
                    args[index] = 0
                    self.assertEqual(app('fn_file_write', 'IIIIHI', *args), 2)
                self.assertEqual(len(m.requests), before)
                m.service_error = 5
                self.assertEqual(app('fn_file_write', 'IIIIHI', 0x71000, 0x72000,
                                     0, 0x73000, 3, 0x74000), 5)
                self.assertNotIn(b'host:/App.prc', m.files)
                self.assertEqual(len(m.requests), before + 1)
                self.finish_client(m, app, 0)

    def test_valid_partial_and_zero_write_acks_have_clean_cleanup(self):
        for transport in (1, 2, 3):
            for accepted in (0, 2, 3):
                with self.subTest(transport=transport, accepted=accepted):
                    m, app = self.service_client(transport)
                    m.write_accepted = accepted
                    self.assertEqual(app('fn_file_write', 'IIIIHI', 0x71000, 0x72000,
                                         0, 0x73000, 3, 0x74000), 0)
                    self.assertEqual(m.read16(0x74004), accepted)
                    self.assertEqual(m.files[b'host:/App.prc'], b'PRC'[:accepted])
                    self.assertEqual(len(m.requests), 1)
                    self.finish_client(m, app, 0)

    def test_listpages_null_arguments_fail_before_io_on_every_transport(self):
        for transport in (1, 2, 3):
            with self.subTest(transport=transport):
                m, app = self.service_client(transport)
                m.uc.mem_write(0x75000, struct.pack('>HBBHBBI', 0, 1, 0, 256, 1, 64, 0))
                for index in range(4):
                    args = [0x71000, 0x72000, 0x75000, 0x74000]
                    args[index] = 0
                    self.assertEqual(app('fn_file_list_pages', 'IIII', *args), 2)
                m.uc.mem_write(0x71000, struct.pack('>IH', 0, 1024))
                self.assertEqual(app('fn_file_list_pages', 'IIII', 0x71000, 0x72000,
                                     0x75000, 0x74000), 2)
                self.assertEqual(bytes(m.uc.mem_read(0x74000, 16)), b'\xa5' * 16)
                self.assertEqual(len(m.requests), 0)
                # Invalid arguments release the owner guard; the next call works.
                m.uc.mem_write(0x71000, struct.pack('>IH', 0x70000, 1024))
                self.assertEqual(app('fn_file_list_pages', 'IIII', 0x71000, 0x72000,
                                     0x75000, 0x74000), 0)
                self.assertEqual(len(m.requests), 1)
                self.finish_client(m, app, 0)

    def test_files_browse_install_stream_and_raw_guard(self):
        io, uri, data, out = self.io(), self.alloc(b'host:/App.prc\0'), self.alloc(b'PRC'), self.alloc()
        self.assertEqual(self.call('fn_file_mkdir', 'IIH', io, uri, 3), 0)
        self.assertEqual(self.call('fn_file_write', 'III IHI'.replace(' ', ''), io, uri, 0, data, 3, out), 0)
        self.assertEqual(self.m.read16(out + 4), 3)
        self.assertEqual(self.call('fn_file_stat', 'III', io, uri, out), 0)
        self.assertEqual(self.m.read32(out + 2), 3)
        self.assertEqual(self.call('fn_file_read', 'IIIIHI', io, uri, 0, data, 8, out), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(data, 3)), b'PRC')
        self.assertEqual(self.m.read16(out + 6), 3)
        self.m.bad_file_echo = True
        self.m.uc.mem_write(data, b'KEEP')
        self.assertEqual(self.call('fn_file_read', 'IIIIHI', io, uri, 0, data, 8, out), 5)
        self.assertEqual(bytes(self.m.uc.mem_read(data, 4)), b'KEEP')
        self.m.bad_file_echo = False
        req = self.alloc(struct.pack('>HBBHBBI', 0, 1, 0, 256, 1, 64, 0))
        self.assertEqual(self.call('fn_file_list_pages', 'IIII', io, uri, req, out), 0)
        self.assertEqual(self.m.read32(out + 2), 1)
        before = len(self.m.requests)
        self.assertEqual(self.call('fn_raw_call', 'HHIHIHI', 0xfd, 1, 0, 0, data, 8, out), 8)
        self.assertEqual(len(self.m.requests), before)

    def test_wifi_and_device_negotiation(self):
        out, state = self.alloc(), self.alloc()
        self.assertEqual(self.call('fn_fuji_get_info', 'I', out), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(out, 4)), b'v1!\0')
        self.assertEqual(self.call('fn_fuji_get_hsio', 'I', state), 0)
        self.assertEqual(self.call('fn_fuji_set_hsio', 'H', 3), 0)
        self.assertEqual(self.call('fn_fuji_get_hsio', 'I', state), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(state, 1)), b'\3')
        for name in ['fn_wifi_get_status', 'fn_wifi_get_config', 'fn_wifi_get_adapter_info']:
            self.assertEqual(self.call(name, 'I', out), 0, name)
        self.assertEqual(bytes(self.m.uc.mem_read(out, 7)), bytes(range(6)) + b'\1')
        update = self.alloc(struct.pack('>BBIII', 1, 1, 0, 0, 0))
        self.assertEqual(self.call('fn_wifi_set_config', 'I', update), 0)
        ssid, password = self.alloc(b'net\0'), self.alloc(b'pw\0')
        self.assertEqual(self.call('fn_wifi_try_join', 'II', ssid, password), 0)
        self.assertEqual(self.call('fn_wifi_get_join_status', 'I', state), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(state, 1)), b'\2')
        count, more, buffer = self.alloc(), self.alloc(), self.alloc()
        self.assertEqual(self.call('fn_wifi_scan', 'HHIHIIIH', 0, 1, out, 1, count, more, buffer, 512), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(count, 1)), b'\1')
        self.assertEqual(bytes(self.m.uc.mem_read(out, 4)), b'net\0')
        self.m.service_error = 8
        self.assertEqual(self.call('fn_fuji_get_info', 'I', out), 8)

    def test_complete_clock_api_and_binary_buffer_bounds(self):
        time = self.alloc(struct.pack('>Q', 0x123456789abcdef0))
        self.assertEqual(self.call('fn_clock_set', 'I', time), 0)
        self.assertEqual(self.m.requests[-1][6:], b'\1' + struct.pack('<Q', 0x123456789abcdef0))
        self.assertEqual(self.call('fn_clock_sync_network_time', 'I', time), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(time, 8)), struct.pack('>Q', 0x123456789abcdef0))
        zone = self.alloc(b'UTC\0')
        for name in ['fn_clock_set_timezone', 'fn_clock_set_timezone_save']:
            self.assertEqual(self.call(name, 'I', zone), 0)
        for format, size in [(0, 7), (1, 4), (2, 6), (3, 32), (4, 32), (5, 16)]:
            out = self.alloc(b'\xa5' * 64)
            self.assertEqual(self.call('fn_clock_get_format', 'IH', out, format), 0)
            self.assertEqual(bytes(self.m.uc.mem_read(out + size, 1)), b'\xa5')
            self.assertEqual(self.call('fn_clock_get_tz', 'IIH', out, zone, format), 0)

    def test_appstore_and_legacy_appkeys(self):
        io, ns, key = self.io(), self.alloc(b'Palm\0'), self.alloc(b'k\0')
        data, out = self.alloc(b'abc'), self.alloc()
        self.assertEqual(self.call('fn_appstore_write', 'IIIIIHI', io, ns, key, 0, data, 3, out), 0)
        self.assertEqual(self.call('fn_appstore_read', 'IIIIIHI', io, ns, key, 0, data, 8, out), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(data, 3)), b'abc')
        self.assertEqual(self.call('fn_appstore_stat', 'IIII', io, ns, key, out), 0)
        self.assertEqual(self.m.read32(out + 2), 3)
        self.assertEqual(self.call('fn_appstore_list', 'IIHIHI', io, ns, 0, data, 128, out), 0)
        self.assertEqual(self.call('fn_appstore_delete', 'IIII', io, ns, key, out), 0)
        # Details set before init survive initialization and preserve exact legacy URI.
        self.call('fn_shutdown')
        self.call('fuji_set_appkey_details', 'HHH', 0xfe0c, 1, 1)
        self.assertEqual(self.call('fn_init'), 0)
        self.assertEqual(self.call('fuji_write_appkey', 'HHI', 1, 3, data), 1)
        self.assertIn(b'persist:///FujiNet/fe0c0101.key', self.m.files)
        count, scratch = self.alloc(), self.alloc()
        self.assertEqual(self.call('fuji_read_appkey_buffered', 'HIIIH', 1, count, data, scratch, 512), 1)
        self.assertEqual(self.m.read16(count), 3)
        self.assertEqual(self.call('fuji_read_appkey', 'HII', 1, count, data), 1)

    def test_mounts_disks_and_http_headers(self):
        io, uri, out, data = self.io(), self.alloc(b'host:/\0'), self.alloc(), self.alloc(b'DATA')
        for name, fmt, args in [
            ('fn_slot_catalog_get', 'IHI', (io, 1, out)),
            ('fn_slot_catalog_put', 'IHHII', (io, 1, 1, uri, out)),
            ('fn_slot_catalog_delete', 'IHI', (io, 1, out)),
            ('fn_slot_catalog_range', 'IHHHHHHI', (io, 1, 3, 1, 0, 64, 128, out)),
            ('fn_disk_mount', 'HIHHHI', (1, uri, 1, 4, 512, out)),
            ('fn_disk_info', 'HI', (1, out)),
            ('fn_disk_restore_boot', 'HI', (1, out)),
            ('fn_disk_read_sector', 'HIIHI', (1, 7, data, 512, out)),
            ('fn_disk_write_sector', 'HIIH', (1, 7, data, 4)),
            ('fn_disk_flush', 'H', (1,)),
            ('fn_disk_clear_changed', 'H', (1,)),
            ('fn_disk_unmount', 'H', (1,)),
            ('fn_disk_inspect', 'IIHHI', (io, uri, 4, 512, out)),
        ]:
            self.assertEqual(self.call(name, fmt, *args), 0, name)
        self.assertEqual(self.m.read16(out + 12), 3)
        self.m.service_error = 5
        self.assertEqual(self.call('fn_disk_info', 'HI', 1, out), 5)
        self.assertEqual(self.call('fn_disk_last_error'), 11)
        self.m.service_error = 0
        display, flags = self.alloc(), self.alloc()
        self.assertEqual(self.call('fn_format_mount_display', 'IIIHI', io, uri, display, 128, flags), 0)
        self.assertEqual(bytes(self.m.uc.mem_read(display, 9)), b'/App.prc\0')
        host, path, leaf = self.alloc(b'host:\0'), self.alloc(b'/\0'), self.alloc(b'App.prc\0')
        self.assertEqual(self.call('fn_resolve_mount_target', 'IIIIIHIHI', io, host, path, leaf, uri, 128, display, 128, flags), 0)
        url, key, value = self.alloc(b'http://example/\0'), self.alloc(b'Authorization\0'), self.alloc(b'Bearer test\0')
        headers = self.alloc(struct.pack('>II', key, value))
        self.assertEqual(self.call('fn_open_headers', 'IHIHIHI', out, 2, url, 0, headers, 1, 3), 0)
        packet = self.m.requests[-1]
        self.assertIn(b'Authorization', packet)
        self.assertIn(b'Bearer test', packet)
        self.assertEqual(self.call('fn_close', 'H', self.m.read16(out)), 0)
        hostname = self.alloc(b'example\0')
        self.assertEqual(self.call('fn_tcp_open', 'IIH', out, hostname, 1234), 0)
        self.assertIn(b'tcp://example:1234', self.m.requests[-1])
        self.assertEqual(self.call('fn_close', 'H', self.m.read16(out)), 0)

    def test_every_new_trap_is_guarded_and_old_library_is_negotiated(self):
        # Outer guard must work without A4 globals for every new entry.
        self.call('fn_shutdown')
        header = (f.ROOT / 'include/palmos/NioLibServices.h').read_text()
        names = re.findall(r'Err (NioLib\w+)\(', header)
        names += ['NioLibGetNetLibStatus', 'NioLibNetLibHangup']  # ABI 1.7, in NioLib.h
        for name in names:
            self.assertEqual(self.m.call(name, struct.pack('>H', 7)), 0x8002, name)
        self.assertEqual(self.call('fn_init'), 0)
        out = self.alloc()
        self.m.current_app += 1
        self.assertEqual(self.call('fn_fuji_get_info', 'I', out), 4)
        self.m.current_app -= 1
        self.call('fn_shutdown')
        # Negotiate an actual older version/capability response.
        self.m.old_services = True
        self.assertEqual(self.call('fn_init'), 0)
        before = len(self.m.requests)
        self.assertEqual(self.call('fn_fuji_get_info', 'I', out), 8)
        self.assertEqual(len(self.m.requests), before)

    def test_services_require_both_version_and_capability(self):
        out = self.alloc()
        self.call('fn_shutdown')
        # The last: an unpublished Network-transport build, ABI 1.6 with bit 0x200, whose traps from 19 on differ.
        for version, caps in (((8, 1, 6, 9), 0x800000ff),
                              ((8, 1, 5, 8), 0x800001ff),
                              ((8, 1, 6, 12), 0x800003ff)):
            self.m.service_version, self.m.service_caps = version, caps
            self.assertEqual(self.call('fn_init'), 0)
            before = len(self.m.requests)
            self.assertEqual(self.call('fn_fuji_get_info', 'I', out), 8)
            self.assertEqual(len(self.m.requests), before)
            self.call('fn_shutdown')

    def test_clock_trap_bounds_and_wide_arguments_fail_without_io(self):
        token = self.m.read32(self.m.app_data + self.m.app_symbols['nl_client_token'])
        out, result = self.alloc(b'\xa5' * 64), self.alloc()
        before = len(self.m.requests)
        for format, capacity in ((256, 32), (3, 31), (4, 0)):
            args = struct.pack('>HIIHHI', 7, token, out, format, capacity, result)
            self.assertEqual(self.m.call('NioLibClockGetFormat', args), 0x8001)
        self.assertEqual(bytes(self.m.uc.mem_read(out, 64)), b'\xa5' * 64)
        self.assertEqual(len(self.m.requests), before)

    def test_file_and_device_services_use_all_selected_transports(self):
        for transport, backend in [(2, TransportMachine), (3, USBMachine)]:
            with self.subTest(transport=transport):
                machine = type('TransportServiceMachine', (ServiceMachine, backend), {})
                m = machine()
                self.assertEqual(m.open(), 0)
                m.uc.mem_write(0x8000, config(transport))
                self.assertEqual(m.call('NioLibSetConfig', struct.pack('>HI', 7, 0x8000)), 0)
                self.assertEqual(m.close(), (0, 0))
                def app(name, fmt='', *args):
                    return m.call(name, struct.pack('>' + fmt, *args), app=True, a5=m.app_a5)
                self.assertEqual(app('fn_init'), 0)
                m.uc.mem_write(0x71000, struct.pack('>IH', 0x70000, 1024))
                m.uc.mem_write(0x72000, b'host:/App.prc\0')
                m.uc.mem_write(0x73000, b'PRC')
                self.assertEqual(app('fn_file_write', 'IIIIHI', 0x71000, 0x72000, 0,
                                     0x73000, 3, 0x74000), 0)
                self.assertEqual(app('fn_file_read', 'IIIIHI', 0x71000, 0x72000, 0,
                                     0x73000, 8, 0x74000), 0)
                self.assertEqual(bytes(m.uc.mem_read(0x73000, 3)), b'PRC')
                self.assertEqual(app('fn_fuji_get_info', 'I', 0x74000), 0)
                app('fn_shutdown')
                self.assertEqual(m.globals, 0)

    def test_header_open_malformed_reply_reports_uncertain_allocation(self):
        self.m.bad_open = True
        handle, url = self.alloc(), self.alloc(b'http://example/\0')
        self.assertEqual(self.call('fn_open_headers', 'IHIHIHI', handle, 1,
                                  url, 0, 0, 0, 0), 5)
        self.assertEqual(len([r for r in self.m.requests if r[:2] == b'\xfd\x01']), 1)
        self.call('fn_shutdown')
        cleanup = self.alloc(struct.pack('>H', 12))
        self.call('NioLibClientCleanupReport', 'I', cleanup)
        self.assertEqual(self.m.read16(cleanup + 8), 1)

    def test_readonly_failures_do_not_clear_serial_or_usb_pending_markers(self):
        for transport, backend, pref in ((2, TransportMachine, 0x4e35), (3, USBMachine, 0x4e36)):
            with self.subTest(transport=transport):
                machine = type('PendingReadonlyMachine', (ServiceMachine, backend), {})
                m = machine()
                self.assertEqual(m.open(), 0)
                m.uc.mem_write(0x8000, config(transport))
                self.assertEqual(m.call('NioLibSetConfig', struct.pack('>HI', 7, 0x8000)), 0)
                err, token, result = m.begin()
                self.assertEqual((err, result), (0, 0))
                m.files[b'host:/App.prc'] = b'PRC'
                m.uc.mem_write(0x71000, struct.pack('>IH', 0x70000, 1024))
                m.uc.mem_write(0x72000, b'host:/App.prc\0')
                m.serial_mode = m.usb_mode = 'badcheck'
                args = struct.pack('>HIIIIIHII', 7, token, 0x71000, 0x72000,
                                   0, 0x73000, 3, 0x74000, 0x75000)
                self.assertEqual(m.call('NioLibFileRead', args), 0)
                self.assertNotEqual(m.read16(0x75000), 0)
                marker = m.preferences[0x4e694c31, pref]
                self.assertEqual(m.end(token)[1], 6)
                self.assertEqual(m.preferences[0x4e694c31, pref], marker)
                self.assertEqual(len(m.requests), 1)
