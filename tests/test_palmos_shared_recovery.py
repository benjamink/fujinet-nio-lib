"""Actual 68K recovery ABI, persistent markers, and backend-neutral Open errors."""
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_transports import TransportMachine, config
from test_palmos_shared_usb import USBMachine
from test_palmos_shared_network import NetworkMachine


def reset(machine, transport, confirmation=1):
    return machine.call('NioLibResetTransport', struct.pack('>3H', 7, transport, confirmation))


class RecoveryTests(unittest.TestCase):
    def test_serial_and_usb_require_confirmation_and_idle_client(self):
        for machine_type, transport in ((TransportMachine, 2), (USBMachine, 3)):
            with self.subTest(transport=transport):
                m = machine_type(); self.assertEqual(m.open(), 0)
                self.assertEqual(m.set_config(config(transport)), 0)
                _, token, rc = m.begin(); self.assertEqual(rc, 0)
                self.assertEqual(reset(m, transport), 0x8006)
                if transport == 2: m.serial_mode = 'badcheck'
                else: m.usb_mode = 'badcheck'
                self.assertNotEqual(m.net_open(token)[2], 0)
                self.assertEqual(m.end(token)[2][4], 1)
                count = len(m.requests)
                self.assertEqual(reset(m, transport, 0), 0x8001)
                self.assertNotEqual(m.begin()[2], 0)
                self.assertEqual(m.close(), (0, 0)); self.assertEqual(m.open(), 0)
                self.assertTrue(m.status()[1][3] & 8)
                # Model the operator retiring the old peer and clearing input.
                m.sessions.clear(); m.serial_rx.clear()
                m.serial_mode = 'normal'
                if transport == 3:
                    m.usb_rx.clear(); m.usb_mode = 'normal'
                self.assertEqual(reset(m, transport), 0)
                self.assertEqual(len(m.requests), count)  # reset sends no packets
                self.assertFalse(m.status()[1][3] & 8)
                _, token, rc = m.begin(); self.assertEqual(rc, 0)
                self.assertEqual(m.clock(token)[:2], (0, 0))
                m.end(token)

    def test_reset_rejects_bad_transport_failed_preference_and_live_driver(self):
        m = USBMachine(); m.open(); m.set_config(config(3))
        self.assertEqual(reset(m, 0), 0x8001)
        self.assertEqual(reset(m, 3, 2), 0x8001)
        _, token, _ = m.begin(); m.usb_mode = 'badcheck'; m.usb_close_fail = True
        m.clock(token); m.end(token)
        self.assertEqual(reset(m, 3), 0x8006)
        m.usb_close_fail = False; m.pref_write_failure = True
        self.assertEqual(reset(m, 3), 0x8005)

    def test_cs1_explicit_reset_clears_corrupt_marker_without_mmio(self):
        m = NetworkMachine(); m.open()
        m.preferences[0x4e694c31, 0x4e32] = (99, b'corrupt')
        before = m.mmio
        self.assertEqual(reset(m, 1), 0)
        self.assertEqual(m.mmio, before)
        self.assertEqual(m.begin()[2], 0)

    def test_probe_and_status_load_nonresident_usb_without_opening_port(self):
        m = USBMachine(); m.open(); m.set_config(config(3))
        m.usb_present = False; m.usb_loadable = True
        self.assertEqual(m.status()[1][-1], 0)
        self.assertFalse(m.usb_live)
        self.assertFalse(m.requests)
        _, token, rc = m.begin(); self.assertEqual(rc, 0)
        self.assertEqual(m.clock(token)[:2], (0, 0))
        m.end(token)
        self.assertIn('remove', m.usb_calls)
