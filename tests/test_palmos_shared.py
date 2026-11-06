"""Native image checks, plus optional actual m68k execution with mocked OS traps.

Build `make palmos-shared` first for compiled-code tests. Install unicorn==2.1.4
in an isolated venv to enable them; no emulator dependency is shipped in a PRC.
The mocks exercise our code, not Palm OS's loader, trap dispatcher or heap.
"""
import ctypes
import importlib.util
import os
import re
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(os.environ.get('PALMOS_SHARED_BUILD', ROOT / 'build/palmos-shared-usb'))
spec = importlib.util.spec_from_file_location('image', ROOT / 'scripts/palmos_shared_image.py')
image = importlib.util.module_from_spec(spec)
spec.loader.exec_module(image)


class ImageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        so = Path(cls.temp.name) / 'image.so'
        subprocess.run(['cc', '-std=c89', '-pedantic', '-Wall', '-Wextra', '-Werror',
                        '-shared', '-fPIC', str(ROOT / 'src/platform/palmos/shared/image.c'),
                        '-o', str(so)], check=True)
        cls.lib = ctypes.CDLL(str(so))
        cls.lib.nl_image_validate.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong]
        cls.lib.nl_image_copy.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                         ctypes.c_ulong, ctypes.c_ulong]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    @staticmethod
    def fixture():
        data = struct.pack('>4I', 0x13579bdf, 0x2468ace0, 0, 0x20)
        return struct.pack('>4sIII', b'NLG1', 16, 22, 2) + data + struct.pack('>4I', 8, 1, 12, 2)

    def test_relocation_at_different_bases_and_zero_bss(self):
        p = self.fixture()
        for db, cb in ((0x30000, 0x10000), (0x71100, 0x52000)):
            dest = ctypes.create_string_buffer(b'\xcc' * 22, 22)
            self.assertEqual(self.lib.nl_image_validate(p, len(p), 0x100), 1)
            self.lib.nl_image_copy(p, dest, db, cb)
            self.assertEqual(struct.unpack('>4I', dest.raw[:16]),
                             (0x13579bdf, 0x2468ace0, db, cb + 0x20))
            self.assertEqual(dest.raw[16:], b'\0' * 6)

    def test_malformed_images_are_rejected(self):
        valid = self.fixture()
        cases = [valid[:i] for i in range(len(valid))]
        for offset, value in ((0, 0), (4, 0), (4, 23), (8, 32768), (12, 3),
                              (32, 7), (32, 14), (36, 0), (36, 3), (40, 8),
                              (28, 0x100), (28, 0x21), (24, 22)):
            p = bytearray(valid)
            struct.pack_into('>I', p, offset, value)
            cases.append(bytes(p))
        cases.append(valid + b'\0')
        for p in cases:
            with self.subTest(image=p.hex()):
                self.assertEqual(self.lib.nl_image_validate(p, len(p), 0x100), 0)

    def test_bad_coff_and_prc(self):
        for bad in (b'', b'\0' * 80):
            with self.assertRaises(ValueError):
                image.coff_sections(bad)
            with self.assertRaises(ValueError):
                image.prc(bad)


try:
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE
    from unicorn import m68k_const
    from unicorn.m68k_const import (UC_CPU_M68K_M68000, UC_M68K_REG_PC,
                                   UC_M68K_REG_A0, UC_M68K_REG_A4,
                                   UC_M68K_REG_A5, UC_M68K_REG_A7,
                                   UC_M68K_REG_D0, UC_M68K_REG_SR)
    HAVE_UNICORN = True
except ImportError:
    HAVE_UNICORN = False


class PalmMachine:
    """Small trap stub set for the compiled M1 library, not a Palm OS emulator."""
    CODE, APP, ENTRY, IMAGE, HEAP, STACK, STOP = 0x10000, 0x20000, 0x4000, 0x5000, 0x30000, 0xf0000, 0x1000

    def __init__(self):
        self.uc = Uc(UC_ARCH_M68K, UC_MODE_BIG_ENDIAN)
        self.uc.ctl_set_cpu_model(UC_CPU_M68K_M68000)
        self.uc.mem_map(0x1000, 0xff000)
        self.code = next(s['data'] for s in image.coff_sections((OUT / 'NioLib').read_bytes())
                         if s['name'] == '.text')
        self.uc.mem_write(self.CODE, self.code)
        app = next(s['data'] for s in image.coff_sections((OUT / 'NioLibTest').read_bytes())
                   if s['name'] == '.text')
        self.uc.mem_write(self.APP, app)
        self.payload = (OUT / 'nDat0000.bin').read_bytes()
        self.uc.mem_write(self.IMAGE, self.payload)
        self.symbols = self.read_symbols('NioLib')
        self.app_symbols = self.read_symbols('NioLibTest')
        self.allocations, self.frees, self.releases = [], [], []
        self.allocation_sizes = {}
        self.fail_alloc = self.fail_owner = self.missing_image = False
        self.locks = 0
        self.uc.hook_add(UC_HOOK_CODE, self.hook)
        self.call('start', struct.pack('>HI', 7, self.ENTRY))
        self.uc.mem_write(self.ENTRY + 8, struct.pack('>I', 123))

    @staticmethod
    def read_symbols(name):
        return {n: int(a, 16) for a, _, n in re.findall(
            r'^([0-9a-f]+)\s+(\S)\s+(\S+)$', (OUT / (name + '.symbols')).read_text(), re.M)}

    def read16(self, address):
        return struct.unpack('>H', self.uc.mem_read(address, 2))[0]

    def read32(self, address):
        return struct.unpack('>I', self.uc.mem_read(address, 4))[0]

    @property
    def globals(self):
        return self.read32(self.ENTRY + 4)

    def hook(self, uc, pc, size, _):
        self.stack_low = min(self.stack_low, uc.reg_read(UC_M68K_REG_A7))
        if bytes(uc.mem_read(pc, 2)) != b'\x4e\x4f':
            return
        trap = self.read16(pc + 2)
        sp = uc.reg_read(UC_M68K_REG_A7)
        if trap == 0xa807:
            # Model the dispatcher's tail call, with a normal return to caller.
            uc.mem_write(sp - 4, struct.pack('>I', pc + 4))
            uc.reg_write(UC_M68K_REG_A7, sp - 4)
            uc.reg_write(UC_M68K_REG_PC, self.CODE + self.symbols['NioLibTestGlobals'])
            return
        result = 0
        if trap == 0xa0b9:
            uc.reg_write(UC_M68K_REG_A0, self.ENTRY)
        elif trap == 0xa049:
            assert self.read16(sp) == 0 and self.read32(sp + 2) == 123
            uc.reg_write(UC_M68K_REG_A0, 0x7000)
        elif trap == 0xa027:
            uc.mem_write(self.read32(sp), bytes(uc.mem_read(sp + 8, 1)) * self.read32(sp + 4))
        elif trap == 0xa2d7:
            first = bytes(uc.mem_read(self.read32(sp), self.read32(sp + 8)))
            second = bytes(uc.mem_read(self.read32(sp + 4), self.read32(sp + 8)))
            result = 0 if first == second else (1 if first > second else 0xffff)
        elif trap == 0xa065:
            kind = bytes(uc.mem_read(sp + 4, 4))
            result = (0xffff if self.missing_image else 0) if kind == b'nDat' else 1
        elif trap == 0xa06e:
            uc.reg_write(UC_M68K_REG_A0, 0x6000 + self.read16(sp + 4) * 4)
        elif trap == 0xa021:
            self.locks += 1
            uc.reg_write(UC_M68K_REG_A0, self.IMAGE)
        elif trap == 0xa02d:
            result = len(self.payload) if self.read32(sp) == 0x6000 else len(self.code)
        elif trap == 0xa013:
            size = self.read32(sp)
            ptr = self.HEAP + len(self.allocations) * 0x4000
            if self.fail_alloc:
                ptr = 0
            else:
                self.allocations.append(ptr)
                self.allocation_sizes[ptr] = size
                uc.mem_write(ptr, b'\xcc' * size)
            uc.reg_write(UC_M68K_REG_A0, ptr)
        elif trap == 0xa01b:
            assert self.read16(sp + 4) == 0
            result = 1 if self.fail_owner else 0
        elif trap == 0xa012:
            ptr = self.read32(sp)
            assert ptr in self.allocations and ptr not in self.frees
            self.frees.append(ptr)
            uc.mem_write(ptr, b'\xdd' * self.allocation_sizes[ptr])
        elif trap == 0xa022:
            self.locks -= 1
        elif trap == 0xa061:
            self.releases.append(self.read32(sp))
        elif trap != 0xa04a:
            raise AssertionError('unexpected OS trap ' + hex(trap))
        uc.reg_write(UC_M68K_REG_D0, result)
        uc.reg_write(UC_M68K_REG_PC, pc + 4)

    def call(self, name, args, app=False, a5=0x5a5a5678, a4=0x4a4a1234, budget=200000):
        uc = self.uc
        uc.reg_write(UC_M68K_REG_SR, 0)
        # Palm GCC explicitly makes D2 caller-clobbered (unlike SysV m68k).
        # CALL_USED_REGISTERS in gcc/config/m68k/m68kpalmos.h is authoritative.
        preserved = {getattr(m68k_const, 'UC_M68K_REG_' + r): 0x12340000 + i
                     for i, r in enumerate(('D3', 'D4', 'D5', 'D6', 'D7',
                                            'A2', 'A3', 'A6'))}
        for reg, value in preserved.items():
            uc.reg_write(reg, value)
        uc.reg_write(UC_M68K_REG_A7, self.STACK)
        uc.reg_write(UC_M68K_REG_A4, a4)
        uc.reg_write(UC_M68K_REG_A5, a5)
        uc.mem_write(self.STACK, struct.pack('>I', self.STOP) + args)
        self.stack_low = self.STACK
        base, symbols = (self.APP, self.app_symbols) if app else (self.CODE, self.symbols)
        uc.emu_start(base + symbols[name], self.STOP, count=budget)
        assert uc.reg_read(UC_M68K_REG_PC) == self.STOP, 'instruction budget exceeded'
        assert uc.reg_read(UC_M68K_REG_A4) == a4, 'caller A4 corrupted'
        assert uc.reg_read(UC_M68K_REG_A5) == a5, 'caller A5 corrupted'
        for reg, value in preserved.items():
            assert uc.reg_read(reg) == value, 'callee-saved register corrupted'
        assert uc.reg_read(UC_M68K_REG_A7) == self.STACK + 4, 'unbalanced stack'
        type(self).peak_stack = max(getattr(type(self), 'peak_stack', 0), self.STACK - self.stack_low)
        return uc.reg_read(UC_M68K_REG_D0) & 0xffff

    def open(self):
        return self.call('NioLibOpen', struct.pack('>H', 7))

    def close(self):
        err = self.call('NioLibClose', struct.pack('>HI', 7, 0x8000))
        return err, self.read16(0x8000)

    def test_globals(self, delta):
        self.uc.mem_write(0x8100, struct.pack('>H', 32) + b'\x00' * 30)
        args = struct.pack('>HIII', 7, delta, 0x8100, 0x8200)
        err = self.call('NioTestGuardedGlobals', args, app=True)
        assert self.read16(0x8200) == 1
        return err, struct.unpack('>HHIIIIIIHH', self.uc.mem_read(0x8100, 32))


@unittest.skipUnless(HAVE_UNICORN and (OUT / 'verification.json').exists(),
                     'requires built PRCs and test-only unicorn==2.1.4')
class CompiledM68KTests(unittest.TestCase):
    def test_globals_references_reopen_and_register_guard(self):
        m = PalmMachine()
        self.assertEqual(m.open(), 0)
        first = m.globals
        self.assertEqual(m.open(), 0)
        err, values = m.test_globals(3)
        self.assertEqual(err, 0)
        self.assertEqual(values, (32, 2, 0x13579bdf, 0x2468ace0, 0, 3,
                                  0x13579bdf, 0x2468ace0 ^ 0xa55a5aa5, 1, 1))
        self.assertEqual(m.test_globals(5)[1][4:6], (3, 8))
        self.assertEqual(m.close(), (0, 1))
        self.assertEqual(m.globals, first)
        self.assertEqual(m.test_globals(0)[1][1], 1)
        for name in ('NioLibSleep', 'NioLibWake'):
            self.assertEqual(m.call(name, struct.pack('>H', 7)), 0)
        self.assertEqual(m.close(), (0, 0))
        self.assertEqual(m.globals, 0)
        self.assertEqual(m.frees, [first])
        self.assertEqual(m.open(), 0)
        self.assertNotEqual(m.globals, first)
        self.assertEqual(m.test_globals(0)[1][4:6], (0, 0))
        self.assertEqual(m.close(), (0, 0))
        self.assertEqual(m.locks, 0)

    def test_underflow_overflow_and_not_open(self):
        m = PalmMachine()
        self.assertEqual(m.close(), (0x8002, 0))
        self.assertEqual(m.call('NioLibTestGlobals', struct.pack('>HII', 7, 0, 0)), 0x8001)
        self.assertEqual(m.open(), 0)
        m.uc.mem_write(m.globals + m.symbols['nl_opens'], b'\xff\xff')
        self.assertEqual(m.open(), 0x8003)
        self.assertEqual(m.read16(m.globals + m.symbols['nl_opens']), 0xffff)
        m.uc.mem_write(m.globals + m.symbols['nl_opens'], b'\x00\x01')
        self.assertEqual(m.close(), (0, 0))
        self.assertEqual(m.close(), (0x8002, 0))
        self.assertEqual(len(m.frees), 1)

    def test_failed_initialization_cleans_up_and_can_retry(self):
        for failure in ('fail_alloc', 'fail_owner', 'missing_image'):
            with self.subTest(failure=failure):
                m = PalmMachine()
                setattr(m, failure, True)
                self.assertNotEqual(m.open(), 0)
                self.assertEqual(m.globals, 0)
                self.assertEqual(m.locks, 0)
                self.assertEqual(len(m.frees), len(m.allocations))
                setattr(m, failure, False)
                self.assertEqual(m.open(), 0)
                self.assertEqual(m.test_globals(0)[1][4], 0)
                self.assertEqual(m.close(), (0, 0))

    def test_version_before_open_and_abi_size_checks(self):
        m = PalmMachine()
        m.uc.mem_write(0x8000, struct.pack('>H', 8) + b'\0' * 6)
        self.assertEqual(m.call('NioLibGetVersion', struct.pack('>HI', 7, 0x8000)), 0)
        version = (8, 1, 1, 2) if 'NioLibBeginClient' in m.symbols else (8, 1, 0, 1)
        if 'NioLibGetConfig' in m.symbols: version = (8, 1, 2, 4)
        if 'nl_usb_open' in m.symbols: version = (8, 1, 3, 6)
        if 'NioLibNetJsonQuery' in m.symbols: version = (8, 1, 4, 7)
        self.assertEqual(struct.unpack('>4H', m.uc.mem_read(0x8000, 8)), version)
        self.assertEqual(m.globals, 0)
        self.assertFalse(m.allocations)
        m.uc.mem_write(0x8000, b'\x00\x02')
        self.assertEqual(m.call('NioLibGetVersion', struct.pack('>HI', 7, 0x8000)), 0x8001)
        self.assertEqual(m.call('NioLibGetCapabilities', struct.pack('>HI', 7, 0x8000)), 0)
        caps = 0x8000000f if 'NioLibBeginClient' in m.symbols else 0x80000001
        if 'NioLibGetConfig' in m.symbols: caps = 0x8000001f
        if 'nl_usb_open' in m.symbols: caps = 0x8000003f
        if 'NioLibNetJsonQuery' in m.symbols: caps = 0x8000007f
        self.assertEqual(m.read32(0x8000), caps)

    def test_rejected_resource_leaves_no_allocations_or_locks(self):
        m = PalmMachine()
        m.uc.mem_write(m.IMAGE, b'BAD!')
        self.assertEqual(m.open(), 0x8004)
        self.assertEqual(m.globals, 0)
        self.assertFalse(m.allocations)
        self.assertEqual(m.locks, 0)
        m.uc.mem_write(m.IMAGE, m.payload)
        self.assertEqual(m.open(), 0)
        self.assertEqual(m.close(), (0, 0))

    def test_bad_parameters_do_not_close_live_reference(self):
        m = PalmMachine()
        m.uc.mem_write(0x8100, struct.pack('>H', 32) + b'\0' * 30)
        args = struct.pack('>HII', 7, 0, 0x8100)
        self.assertEqual(m.call('NioLibTestGlobals', args), 0x8002)
        self.assertEqual(m.open(), 0)
        self.assertEqual(m.call('NioLibClose', struct.pack('>HI', 7, 0)), 0x8001)
        self.assertEqual(m.test_globals(0)[1][1], 1)
        self.assertEqual(m.close(), (0, 0))


if __name__ == '__main__':
    unittest.main()
