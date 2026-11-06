#!/usr/bin/env python3
"""Coordinated Stage 2B runner. Dry-run unless --authorized is explicit.
No privilege changes, driver detachment, installation or automatic retries.
"""
import argparse
from collections import Counter
import json
import hashlib
import os
from pathlib import Path
import re
import selectors
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parent
TRANSPORTS = ('usb', 'springboard')


def summarize(text):
    """Summarize existing logs; dispatch is not proof of physical delivery."""
    received = Counter(re.findall(r'receive: id=\d+ dev=0x([0-9a-fA-F]+) cmd=0x([0-9a-fA-F]+)', text))
    sent = Counter(re.findall(r'send: dev=0x([0-9a-fA-F]+) status=(\d+) cmd=0x([0-9a-fA-F]+)', text))
    return {
        'requests_by_device_command': {':'.join(k): v for k, v in sorted(received.items())},
        'replies_by_device_status_command': {':'.join(k): v for k, v in sorted(sent.items())},
        'clock_get_requests': sum(v for (d, c), v in received.items() if d.lower() == '45' and c == '01'),
        'network_open_requests': sum(v for (d, c), v in received.items() if d.lower() == 'fd' and c == '01'),
        'network_close_requests': sum(v for (d, c), v in received.items() if d.lower() == 'fd' and c == '04'),
        'active_session_observations': re.findall(r'active_sessions:\s*(\d+)', text),
        'usb_status_lines': [line for line in text.splitlines() if 'generation=' in line],
        'warning_lines': [line for line in text.splitlines() if re.search(r'timeout|ambiguous|retir|ACCESS|BUSY|denied', line, re.I)],
        'acceptance': 'Requires Palm PASS and review of request/reply, cleanup and USB status evidence; counts alone are not acceptance.',
    }


def run(args):
    bundle = args.bundle.resolve()
    executable = (args.executable or bundle / 'bin' / (args.transport + '-fujinet-nio')).resolve()
    try:
        manifest = json.loads((bundle / 'manifest.json').read_text())
        identity = manifest['hosts'][args.transport]
        expected = manifest['sha256']['bin/' + args.transport + '-fujinet-nio']
        if identity['sha256'] != expected or hashlib.sha256(executable.read_bytes()).hexdigest() != expected:
            raise ValueError('Prepared host identity/hash changed')
    except (OSError, ValueError, KeyError) as error:
        raise SystemExit('Invalid bundle/executable: %s; prepare a verified bundle first.' % error)
    env = dict(os.environ, FN_CONSOLE='stdio', FN_VISOR_USB_PORT='2')
    if args.transport == 'springboard':
        if not args.port:
            raise SystemExit('--port must identify the RP2350 NIO-link CDC (not the debug console)')
        env['FN_SERIAL_PORT'] = args.port
    if not args.authorized:
        print(json.dumps({'executable': str(executable), 'cwd': str(args.logs),
                          'FN_CONSOLE': 'stdio', 'FN_VISOR_USB_PORT': '2',
                          'FN_SERIAL_PORT': args.port,
                          'action': 'DRY RUN: no executable started; await physical-test authorization'}, indent=2))
        return
    # A conservative operator guard: do not start alongside any NIO backend.
    listing = subprocess.check_output(['ps', '-axo', 'pid=,comm='], text=True)
    if any(line.split()[-1].split('/')[-1] == 'fujinet-nio' for line in listing.splitlines() if line.split()):
        raise SystemExit('An NIO backend is already running. Coordinate its release; no process was killed.')
    os.umask(0o077)
    args.logs.mkdir(parents=True, exist_ok=True)
    logfile = args.logs / 'host.log'
    if logfile.exists():
        raise SystemExit('Choose a new log directory; existing evidence is never overwritten.')
    (args.logs / 'run.json').write_text(json.dumps({'binary': str(executable), 'transport': args.transport,
        'serial_port': args.port, 'identity': identity, 'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}, indent=2))
    child = subprocess.Popen([str(executable)], cwd=args.logs, env=env,
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
    selector = selectors.DefaultSelector()
    selector.register(child.stdout, selectors.EVENT_READ)
    next_status = 0
    stopping = None
    with logfile.open('xb') as log:
        try:
            while True:
                try:
                    if child.poll() is None and time.monotonic() >= next_status and stopping is None:
                        commands = b'net.sessions\n'
                        if args.transport == 'usb': commands += b'visor.status\n'
                        child.stdin.write(commands); child.stdin.flush()
                        next_status = time.monotonic() + 5
                    events = selector.select(0.25)
                    for key, _ in events:
                        data = os.read(key.fileobj.fileno(), 65536)
                        if data:
                            log.write(data); log.flush(); sys.stdout.buffer.write(data); sys.stdout.buffer.flush()
                        else: selector.unregister(key.fileobj)
                    if child.poll() is not None and not selector.get_map(): break
                    if stopping is not None and time.monotonic() - stopping > 10:
                        child.terminate(); break
                except KeyboardInterrupt:
                    # Existing console command exits the core normally, draining
                    # USB callbacks and owner cleanup. No reboot/reset command.
                    if stopping is None and child.poll() is None:
                        child.stdin.write(b'kill\n'); child.stdin.flush(); stopping = time.monotonic()
        finally:
            if child.poll() is None:
                child.terminate()
                try: child.wait(timeout=3)
                except subprocess.TimeoutExpired: child.kill(); child.wait()
            child.stdin.close(); child.stdout.close(); selector.close()
    (args.logs / 'summary.json').write_text(json.dumps(summarize(logfile.read_text(errors='replace')), indent=2) + '\n')
    print('Saved', args.logs / 'summary.json')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    host = sub.add_parser('host')
    host.add_argument('transport', choices=TRANSPORTS)
    host.add_argument('--port')
    host.add_argument('--bundle', type=Path, required=True)
    host.add_argument('--executable', type=Path, help='optional relocated binary; must match bundle hash')
    host.add_argument('--logs', type=Path, required=True)
    host.add_argument('--authorized', action='store_true')
    summary = sub.add_parser('summary'); summary.add_argument('log', type=Path)
    args = parser.parse_args()
    if args.command == 'summary': print(json.dumps(summarize(args.log.read_text(errors='replace')), indent=2))
    else: run(args)

if __name__ == '__main__': main()
