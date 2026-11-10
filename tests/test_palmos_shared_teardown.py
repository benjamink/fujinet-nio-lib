"""Pre-upstream lifecycle/readiness regressions: real m68k code, mocked drivers.
Driver error return injection is not a claim about physical ROM behavior.
"""
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_usb import USBMachine
from test_palmos_shared_transports import config
if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_D0, UC_M68K_REG_PC, UC_M68K_REG_A7


def new_app(m):
    """Discard the departing app's A5 state, retaining only OS/library state."""
    image, report = f.image.make_image((f.OUT / 'NioLibTest').read_bytes())
    data = bytearray(image[16:16 + report['data_bytes']]) + bytearray(report['bss_bytes'])
    for offset, kind in report['fixups']:
        value = struct.unpack_from('>I', data, offset)[0]
        struct.pack_into('>I', data, offset, value + (m.APP if kind == 2 else m.app_data))
    m.uc.mem_write(m.app_data, bytes(data))
    m.current_app += 1


class OwnedMachine(USBMachine):
    def __init__(self):
        self.restore_fail = self.port_close_fail = self.flush_fail = False
        self.rx_pointer = self.tx_pointer = 0
        self.legacy_still_open = False
        self.serial_closes = 0
        self.abi_minor = 3
        self.status_calls = 0
        super().__init__()

    def serial_api(self, selector, sp, old):
        if selector == 19:
            pointer = self.read32(sp + 2)
            if not pointer and self.restore_fail: return 1
            result = super().serial_api(selector, sp, old)
            if not result: self.rx_pointer = pointer
            return result
        if selector == 3:
            self.serial_closes += 1
            if self.port_close_fail: return 1
            if old and self.legacy_still_open: return 0x308
            result = super().serial_api(selector, sp, old)
            self.rx_pointer = self.tx_pointer = 0
            return result
        if selector == 14:
            if self.flush_fail: return 1
            self.tx_pointer = 0
        if selector == 11: self.tx_pointer = self.read32(sp + 2)
        if selector == 13 and not self.tx_pending: self.tx_pointer = 0
        return super().serial_api(selector, sp, old)

    def hook(self, uc, pc, size, context):
        if bytes(uc.mem_read(pc, 2)) == b'\x4e\x4f':
            trap = self.read16(pc + 2)
            if trap == 0xa815:
                self.status_calls += 1
                assert self.abi_minor >= 2, 'old ABI must not call TransportStatus'
            if trap == 0xa805 and self.abi_minor != 3:
                sp = uc.reg_read(UC_M68K_REG_A7)
                uc.mem_write(self.read32(sp + 2), struct.pack('>4H', 8, 1, self.abi_minor, 1))
                uc.reg_write(UC_M68K_REG_D0, 0); uc.reg_write(UC_M68K_REG_PC, pc + 4)
                return
        super().hook(uc, pc, size, context)

    def choose(self, transport):
        self.saved_preferences[0x4e694c31, 0x4e34] = 1, config(transport)

    def assert_storage_owned(self, test):
        for pointer in (self.rx_pointer, self.tx_pointer):
            if pointer:
                test.assertTrue(self.globals)
                test.assertNotIn(self.globals, self.frees)
                test.assertGreaterEqual(pointer, self.globals)
                test.assertLess(pointer, self.globals + 32768)


@unittest.skipUnless(f.HAVE_UNICORN and (f.OUT / 'usb.o').exists(), 'requires compiled USB library and Unicorn')
class TeardownTests(unittest.TestCase):
    def test_usb_failed_close_cross_app_recovery_and_no_open_leak(self):
        m = OwnedMachine(); m.choose(3)
        self.assertEqual(m.client('fn_init'), 0)
        address = m.globals
        m.usb_close_fail = True; m.client('fn_shutdown')
        self.assertTrue(m.usb_live); self.assertEqual(m.globals, address)
        self.assertEqual(m.read16(address + m.symbols['nl_opens']), 0)
        self.assertEqual(m.read16(m.app_data + m.app_symbols['client_open']), 0)
        for _ in range(3):
            new_app(m)
            allocations, closes = len(m.allocations), m.usb_calls.count(0xa802)
            self.assertNotEqual(m.client('fn_init'), 0)
            self.assertEqual(m.usb_calls.count(0xa802), closes + 1)
            m.client('fn_shutdown')
            self.assertEqual(len(m.allocations), allocations)
            self.assertEqual(m.globals, address)
            self.assertEqual(m.read16(address + m.symbols['nl_opens']), 0)
        m.usb_close_fail = False; new_app(m)
        self.assertEqual(m.client('fn_init'), 0)
        self.assertEqual(m.read16(address + m.symbols['nl_opens']), 1)
        self.assertEqual(m.client('fn_clock_get', struct.pack('>I', 0x9600)), 0)
        m.client('fn_shutdown')
        self.assertFalse(m.usb_live); self.assertFalse(m.globals)
        # The later app found (did not load) the existing registration. It
        # leaves that empty table entry for the normal SysLibRemove owner;
        # all app Opens and physical resources/globals are nevertheless gone.
        self.assertEqual(len(m.allocations), len(m.frees))

    def test_usb_resource_recovery_never_clears_ambiguous_command(self):
        m = OwnedMachine(); m.choose(3); self.assertEqual(m.client('fn_init'), 0)
        m.usb_close_fail = True; m.usb_mode = 'silent'
        self.assertNotEqual(m.client('fn_clock_get', struct.pack('>I', 0x9600)), 0)
        m.client('fn_shutdown'); marker = dict(m.preferences); requests = len(m.requests)
        m.usb_close_fail = False; new_app(m)
        self.assertNotEqual(m.client('fn_init'), 0)
        self.assertEqual(m.preferences, marker); self.assertEqual(len(m.requests), requests)
        self.assertFalse(m.usb_live); self.assertFalse(m.globals)

    def test_serial_teardown_driver_ownership_and_cross_app_retry(self):
        for old in (False, True):
            for restore, close, flush in ((True, False, False), (False, True, False),
                                           (True, True, False), (True, True, True)):
                with self.subTest(old=old, restore=restore, close=close, flush=flush):
                    m = OwnedMachine(); m.new_manager = not old; m.choose(2)
                    self.assertEqual(m.client('fn_init'), 0)
                    address = m.globals
                    if flush:
                        m.tx_pending = True; m.flush_fail = True
                        self.assertNotEqual(m.client('fn_clock_get', struct.pack('>I', 0x9600)), 0)
                        m.assert_storage_owned(self)
                    m.restore_fail, m.port_close_fail, m.flush_fail = restore, close, flush
                    m.client('fn_shutdown'); m.assert_storage_owned(self)
                    marker = dict(m.preferences)
                    if close:
                        self.assertTrue(m.serial_opened); self.assertEqual(m.globals, address)
                        self.assertEqual(m.read16(address + m.symbols['nl_opens']), 0)
                        for _ in range(2):
                            new_app(m); self.assertNotEqual(m.client('fn_init'), 0)
                            self.assertEqual(m.read16(address + m.symbols['nl_opens']), 0)
                            m.assert_storage_owned(self)
                    else:
                        self.assertFalse(m.serial_opened); self.assertFalse(m.globals)
                    m.restore_fail = m.port_close_fail = m.flush_fail = m.tx_pending = False
                    new_app(m)
                    if flush:
                        self.assertNotEqual(m.client('fn_init'), 0)  # ambiguity persists
                        self.assertEqual(m.preferences, marker)
                    else:
                        self.assertEqual(m.client('fn_init'), 0)
                        self.assertEqual(m.client('fn_clock_get', struct.pack('>I', 0x9600)), 0)
                        m.client('fn_shutdown')
                    self.assertFalse(m.serial_opened); self.assertFalse(m.globals)
                    self.assertEqual((m.rx_pointer, m.tx_pointer), (0, 0))
                    self.assertEqual(len(m.allocations), len(m.frees))

    def test_partial_serial_initialization_retains_driver_until_retry(self):
        for old in (False, True):
            m = OwnedMachine(); m.new_manager = not old; m.choose(2)
            m.fail_settings = m.port_close_fail = True
            self.assertNotEqual(m.client('fn_init'), 0)
            self.assertTrue(m.serial_opened); self.assertTrue(m.globals)
            self.assertEqual(m.read16(m.globals + m.symbols['nl_opens']), 0)
            m.fail_settings = m.port_close_fail = False; new_app(m)
            self.assertEqual(m.client('fn_init'), 0)
            m.client('fn_shutdown'); self.assertFalse(m.serial_opened)
            self.assertFalse(m.globals)

    def test_legacy_still_open_releases_our_reference_without_closing_another(self):
        m = OwnedMachine(); m.new_manager = False; m.choose(2)
        m.serial_busy = m.legacy_still_open = True
        self.assertNotEqual(m.client('fn_init'), 0)
        self.assertEqual(m.serial_closes, 1)
        self.assertFalse(m.globals)
        self.assertFalse(any(selector in (14, 19) for old, selector in m.serial_calls))

        m = OwnedMachine(); m.new_manager = False; m.choose(2)
        self.assertEqual(m.client('fn_init'), 0)
        m.restore_fail = m.legacy_still_open = True
        m.client('fn_shutdown')
        self.assertEqual(m.serial_closes, 1)  # subsequent reaping only detaches RX
        self.assertTrue(m.globals); self.assertTrue(m.rx_pointer)
        m.assert_storage_owned(self)
        m.restore_fail = False
        # First Open reconciles the old RX pointer, without repeating SerClose.
        self.assertEqual(m.open(), 0)
        self.assertEqual(m.serial_closes, 1)
        self.assertEqual(m.rx_pointer, 0)
        self.assertEqual(m.close(), (0, 0)); self.assertFalse(m.globals)
