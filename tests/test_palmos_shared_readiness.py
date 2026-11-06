"""Actual compatibility-client readiness across transports and old ABI."""
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_teardown import OwnedMachine

@unittest.skipUnless(f.HAVE_UNICORN and (f.OUT / 'usb.o').exists(), 'requires compiled USB library and Unicorn')
class ReadinessTests(unittest.TestCase):
    def test_readiness_selected_transport_and_old_abi_fallback(self):
        for transport in (1, 2, 3):
            m = OwnedMachine(); m.choose(transport)
            self.assertEqual(m.client('fn_is_ready'), 0)
            self.assertEqual(m.client('fn_init'), 0)
            self.assertEqual(m.client('fn_is_ready'), 1)
            self.assertEqual(m.client('fn_clock_get', struct.pack('>I', 0x9600)), 0)
            m.client('fn_shutdown'); self.assertEqual(m.client('fn_is_ready'), 0)
        m = OwnedMachine(); m.choose(3); m.usb_present = False
        self.assertNotEqual(m.client('fn_init'), 0); self.assertEqual(m.client('fn_is_ready'), 0)
        for minor in (0, 1):
            m = OwnedMachine(); m.abi_minor = minor
            result = m.client('fn_init')
            self.assertEqual(result == 0, minor == 1)
            self.assertEqual(m.client('fn_is_ready'), int(minor == 1))
            self.assertEqual(m.status_calls, 0)
            m.client('fn_shutdown')
