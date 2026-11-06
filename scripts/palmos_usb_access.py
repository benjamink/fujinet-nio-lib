#!/usr/bin/env python3
"""TEMPORARY bench-only access, never run without separate explicit authorization.
Scopes 082d:0100, config 1/interface 0, one physical Linux USB path and a finite
session. Restores surviving device ACLs/binding. No modules, resets or udev rules.
"""
import argparse
import ctypes
import ctypes.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import time


def valid_path(value):
    if not re.fullmatch(r'[1-9][0-9]*-[1-9][0-9]*(\.[1-9][0-9]*){0,6}', value):
        raise argparse.ArgumentTypeError('Expected a physical USB path such as 1-9.4')
    return value


def emit(event, **fields):
    print(json.dumps(dict(event=event, **fields)), flush=True)


def inspect(path):
    device = Path('/sys/bus/usb/devices') / path
    if (device / 'idVendor').read_text().strip() != '082d' or (device / 'idProduct').read_text().strip() != '0100':
        raise ValueError('Selected physical path is not the tested Visor')
    interface = device.parent / (path + ':1.0')
    if ((interface / 'bInterfaceNumber').read_text().strip() != '00' or
            (interface / 'bInterfaceClass').read_text().strip().lower() != 'ff'):
        raise ValueError('Unexpected interface; refusing changes')
    node = Path('/dev/bus/usb') / ('%03d' % int((device / 'busnum').read_text())) / ('%03d' % int((device / 'devnum').read_text()))
    return interface, node, node.stat().st_ino



def generic_record(data):
    if len(data) != 6: return False
    count = data[0] | (data[1] << 8)
    if not 1 <= count <= 2: return False
    records = [(data[2+2*n], data[3+2*n]) for n in range(count)]
    if any(f > 4 or p not in (1, 2) for f, p in records): return False
    if len({p for _, p in records}) != count: return False
    return (0, 2) in records


def generic_two(node):
    """Read-only old connection query; never unbind a HotSync logical port."""
    lib = ctypes.CDLL(ctypes.util.find_library('usb-1.0') or 'libusb-1.0.so.0')
    ptr = ctypes.c_void_p
    lib.libusb_init.argtypes = [ctypes.POINTER(ptr)]
    lib.libusb_get_device_list.argtypes = [ptr, ctypes.POINTER(ctypes.POINTER(ptr))]
    lib.libusb_get_device_list.restype = ctypes.c_ssize_t
    lib.libusb_get_bus_number.argtypes = lib.libusb_get_device_address.argtypes = [ptr]
    lib.libusb_get_bus_number.restype = lib.libusb_get_device_address.restype = ctypes.c_uint8
    lib.libusb_open.argtypes = [ptr, ctypes.POINTER(ptr)]
    lib.libusb_control_transfer.argtypes = [ptr, ctypes.c_uint8, ctypes.c_uint8,
        ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint16, ctypes.c_uint]
    lib.libusb_free_device_list.argtypes = [ctypes.POINTER(ptr), ctypes.c_int]
    lib.libusb_close.argtypes = lib.libusb_exit.argtypes = [ptr]
    context, handle, devices = ptr(), ptr(), ctypes.POINTER(ptr)()
    if lib.libusb_init(ctypes.byref(context)): return False
    try:
        count = lib.libusb_get_device_list(context, ctypes.byref(devices))
        for i in range(max(0, count)):
            device = devices[i]
            if (lib.libusb_get_bus_number(device) != int(node.parent.name) or
                    lib.libusb_get_device_address(device) != int(node.name)): continue
            if lib.libusb_open(device, ctypes.byref(handle)): return False
            data = (ctypes.c_uint8 * 6)()
            got = lib.libusb_control_transfer(handle, 0xc2, 3, 0, 0, data, 6, 100)
            if got != 6: return False
            return generic_record(bytes(data))
        return False
    finally:
        if handle: lib.libusb_close(handle)
        if devices: lib.libusb_free_device_list(devices, 1)
        lib.libusb_exit(context)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--physical-path', required=True, type=valid_path)
    parser.add_argument('--uid', required=True, type=int)
    parser.add_argument('--seconds', type=int, default=900)
    parser.add_argument('--authorized', action='store_true')
    args = parser.parse_args()
    if args.uid <= 0 or not 1 <= args.seconds <= 1800:
        parser.error('Non-root user ID and duration 1..1800 seconds required')
    if not args.authorized:
        emit('dry-run', physical_path=args.physical_path, uid=args.uid, seconds=args.seconds)
        return
    if os.geteuid() != 0:
        raise SystemExit('Requires an explicitly authorized sudo invocation; no changes performed')
    # The bench operator must end HotSync first. Never compete with its owner.
    processes = subprocess.check_output(['ps', '-axo', 'args='], text=True)
    if any('pilot-xfer' in line or '/palm-sync/' in line or 'palmos_usb_install.py' in line for line in processes.splitlines()):
        raise SystemExit('HotSync process present: stop here; no changes performed')
    stop = False
    def interrupted(*_):
        nonlocal stop
        stop = True
    signal.signal(signal.SIGINT, interrupted); signal.signal(signal.SIGTERM, interrupted)
    records = {}
    end = time.monotonic() + args.seconds
    try:
        while not stop and time.monotonic() < end:
            try:
                interface, node, inode = inspect(args.physical_path)
                key = str(node), inode
                if key not in records and not generic_two(node):
                    time.sleep(0.1)
                    continue
                if key not in records:
                    original = subprocess.check_output(['getfacl', '-p', str(node)], text=True)
                    driver = (interface / 'driver').resolve().name if (interface / 'driver').exists() else None
                    if driver not in (None, 'visor'):
                        raise ValueError('Unexpected driver owner: ' + driver)
                    records[key] = (original, driver, interface)
                    subprocess.run(['setfacl', '-m', 'u:%d:rw' % args.uid, str(node)], check=True)
                    emit('granted', node=str(node), interface=interface.name, previous_driver=driver)
                # The kernel may finish binding shortly after enumeration.
                if (interface / 'driver').exists() and (interface / 'driver').resolve().name == 'visor':
                    original, _, saved_interface = records[key]
                    records[key] = (original, 'visor', saved_interface)
                    Path('/sys/bus/usb/drivers/visor/unbind').write_text(interface.name)
                    emit('unbound', interface=interface.name)
            except FileNotFoundError:
                pass  # Closed Palm port; await its next enumeration.
            time.sleep(0.02)
    finally:
        for (name, inode), (acl, driver, interface) in records.items():
            try:
                current_interface, node, current_inode = inspect(args.physical_path)
                if str(node) != name or current_inode != inode:
                    continue  # Never touch a recycled/disconnected device node.
                subprocess.run(['setfacl', '--restore=-'], input=acl, text=True, check=True)
                if driver == 'visor' and not (current_interface / 'driver').exists():
                    Path('/sys/bus/usb/drivers/visor/bind').write_text(current_interface.name)
                emit('restored', node=name, interface=interface.name)
            except (OSError, ValueError, subprocess.CalledProcessError) as error:
                emit('restore-warning', node=name, error=str(error))
        emit('finished', observed_nodes=len(records))

if __name__ == '__main__': main()
