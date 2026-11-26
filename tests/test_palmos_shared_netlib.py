"""The Network transport (4) under Unicorn: the real compiled NioLib, its NIO
core (netlib/), the NetLib glue (netlib.c) and the transport.c branches.

Only Palm OS is mocked: SysLibFind("Net.lib") and the NetLib library traps the
image actually calls (see nio_sock_palm.c and netlib.c). Its sockets are backed
by scripted in-Python peers: no real network, host or link is used. This is
host evidence for the code paths, not a qualification of NetLib, PPP or a
real server.
"""
import ipaddress
import json
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_transports import TransportMachine, config

if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_A0, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC

HAS_NETLIB = ((f.OUT / 'verification.json').exists() and 'NioLibGetNetLibStatus' in
              json.loads((f.OUT / 'verification.json').read_text()).get('dispatch', {}))

NETWORK = 4                     # nioTransportNetwork
NET_REF = 0x0b                  # the refNum SysLibFind("Net.lib") hands out here
LIB = 7                         # NioLib's own refNum in the harness

# NioLib.h
BUSY, PARAM = 0x8006, 0x8001    # nioLibErrBusy, nioLibErrParam
PRESENT, UP, SESSION = 1, 2, 4  # nioNetLibPresent, nioNetLibUp, nioNetLibSession
ACTIVE, AVAILABLE = 4, 2        # nioStatusActive, nioStatusAvailable
# fujinet-nio.h
FN_OK, FN_NOT_FOUND, FN_NOT_READY, FN_IO = 0, 1, 4, 5
READ_EOF, INFO_LENGTH, INFO_STATUS = 0x01, 0x02, 0x04
# NetMgr.h (SDK 3.5): netErrorClass is 0x1200.
ERR_ALREADY_OPEN, ERR_NOT_OPEN, ERR_STILL_OPEN = 0x1201, 0x1202, 0x1203
ERR_TIMEOUT, ERR_NO_IF, ERR_UNREACHABLE, ERR_NO_NAME = 0x1212, 0x121c, 0x122d, 0x123e
SOCKET_INET, SOCKET_STREAM = 2, 1             # netSocketAddrINET, netSocketTypeStream
IF_ACTUAL_IP = 0x1002                         # netIFSettingActualIPAddr
HOST_INFO_ADDRESS = 552                       # NetHostInfoBufType.address[0]

# sysLibTrapCustom is 0xa805. Only the NetLib calls the image makes; any other is an error.
NETLIB_TRAPS = {0xa801: 'Open', 0xa802: 'Close', 0xa805: 'AddrINToA', 0xa806: 'AddrAToIN',
                0xa807: 'SocketOpen', 0xa808: 'SocketClose', 0xa80c: 'SocketConnect',
                0xa811: 'Send', 0xa813: 'Receive', 0xa81b: 'GetHostByName',
                0xa820: 'IFGet', 0xa821: 'IFSettingGet', 0xa82e: 'OpenCount',
                0xa831: 'OpenIfCloseWait'}


class Connection:
    """One TCP connection as the server sees it."""
    def __init__(self, server, address, port):
        self.server, self.address, self.port = server, address, port
        self.sent, self.inbox = bytearray(), bytearray()
        self.remote_closed = self.closed = False
        self.delivered = 0              # bytes the Palm has received


class HttpServer:
    """Answers one HTTP request per connection, then closes the connection."""
    def __init__(self, body=b'', status=200, chunk=None):
        self.body, self.status, self.chunk = body, status, chunk

    def received(self, conn):
        if conn.inbox or conn.remote_closed or b'\r\n\r\n' not in conn.sent:
            return
        conn.inbox += (b'HTTP/1.0 %d OK\r\nContent-Type: text/plain\r\n'
                       b'Content-Length: %d\r\n\r\n' % (self.status, len(self.body))) + self.body
        conn.remote_closed = True


class EchoServer:
    """A tcp:// peer that sends back what it gets."""
    chunk = None

    def received(self, conn):
        conn.inbox += conn.sent[getattr(conn, 'echoed', 0):]
        conn.echoed = len(conn.sent)


class NetLibMachine(TransportMachine):
    """TransportMachine plus Net.lib and a scripted network behind it."""
    NET_HEAP = 0x100000

    def __init__(self):
        self.netlib_present = True
        self.netlib_calls = []
        self.open_count = 0             # NetLib's open count: ours and other programs'
        self.close_wait = False         # closed by everyone, link kept for its idle time
        self.open_error = 0
        self.if_errs = 0
        self.hosts = {'example.com': 0x5db8d822}
        self.servers = {}               # (address, port) -> server; anything else refuses
        self.connections, self.sockets, self.next_socket = [], {}, 3
        self.local_ip = 0x0a000005      # what the PPP interface reports
        self.heap_next = 0              # the base heap until the bigger one is mapped
        super().__init__()
        # Room for many lifetimes' allocations (the base heap has twelve 16 KB slots).
        self.uc.mem_map(self.NET_HEAP, 0x100000)
        self.heap_next = self.NET_HEAP

    # The machine's view of the library.
    def calls(self, name):
        return [c for c in self.netlib_calls if c[0] == name]

    def live_allocations(self):
        return [p for p in self.allocations if p not in self.frees]

    def c_string(self, address):
        text = bytearray()
        while self.uc.mem_read(address + len(text), 1)[0]:
            text += self.uc.mem_read(address + len(text), 1)
        return bytes(text)

    def hook(self, uc, pc, size, context):
        if bytes(uc.mem_read(pc, 2)) != b'\x4e\x4f':
            return super().hook(uc, pc, size, context)
        trap, sp = self.read16(pc + 2), uc.reg_read(UC_M68K_REG_A7)
        if trap == 0xa0ba and self.read32(sp) and self.c_string(self.read32(sp)) == b'Net.lib':
            if self.netlib_present: uc.mem_write(self.read32(sp + 4), struct.pack('>H', NET_REF))
            result = 0 if self.netlib_present else 0x0502   # sysErrLibNotFound
        elif trap in NETLIB_TRAPS and self.read16(sp) == NET_REF:
            assert self.netlib_present, 'NetLib call without Net.lib'
            result = getattr(self, 'net_' + NETLIB_TRAPS[trap])(sp)
        elif trap == 0xa013 and self.heap_next:
            size = self.read32(sp)
            ptr = 0 if self.fail_alloc else self.heap_next
            if ptr:
                self.heap_next += (size + 31) & ~15
                self.allocations.append(ptr)
                self.allocation_sizes[ptr] = size
                uc.mem_write(ptr, b'\xcc' * size)
            uc.reg_write(UC_M68K_REG_A0, ptr)
            result = 0
        else:
            return super().hook(uc, pc, size, context)
        uc.reg_write(UC_M68K_REG_D0, result & 0xffffffff)
        uc.reg_write(UC_M68K_REG_PC, pc + 4)

    def error(self, address, value):
        self.uc.mem_write(address, struct.pack('>H', value))

    # NetLib, after NetMgr.h's prototypes.
    def net_Open(self, sp):
        self.netlib_calls.append(('Open',))
        if self.open_error: return self.open_error
        self.uc.mem_write(self.read32(sp + 2), struct.pack('>H', self.if_errs))
        already = self.open_count > 0
        self.open_count += 1
        self.close_wait = False
        return ERR_ALREADY_OPEN if already else 0

    def net_Close(self, sp):
        immediate = self.read16(sp + 2)
        self.netlib_calls.append(('Close', immediate))
        if not self.open_count: return ERR_NOT_OPEN
        self.open_count -= 1
        if self.open_count: return ERR_STILL_OPEN
        self.close_wait = not immediate
        return 0

    def net_OpenCount(self, sp):
        self.netlib_calls.append(('OpenCount',))
        self.uc.mem_write(self.read32(sp + 2), struct.pack('>H', self.open_count))
        return 0

    def net_OpenIfCloseWait(self, sp):
        self.netlib_calls.append(('OpenIfCloseWait',))
        if self.open_count or not self.close_wait: return ERR_NOT_OPEN
        self.open_count, self.close_wait = 1, False
        return 0

    def net_AddrAToIN(self, sp):
        text = self.c_string(self.read32(sp + 2)).decode('latin-1')
        self.netlib_calls.append(('AddrAToIN', text))
        try: return int(ipaddress.IPv4Address(text))
        except ValueError: return 0xffffffff

    def net_GetHostByName(self, sp):
        name = self.c_string(self.read32(sp + 2)).decode('latin-1')
        buffer, err = self.read32(sp + 6), self.read32(sp + 14)
        self.netlib_calls.append(('GetHostByName', name))
        assert self.open_count, 'DNS without NetLib open'
        if name not in self.hosts:
            self.error(err, ERR_NO_NAME)
            self.uc.reg_write(UC_M68K_REG_A0, 0)
            return 0
        self.uc.mem_write(buffer + HOST_INFO_ADDRESS, struct.pack('>I', self.hosts[name]))
        self.error(err, 0)
        self.uc.reg_write(UC_M68K_REG_A0, buffer)
        return 0

    def net_SocketOpen(self, sp):
        domain, kind = self.uc.mem_read(sp + 2, 1)[0], self.uc.mem_read(sp + 4, 1)[0]
        self.netlib_calls.append(('SocketOpen', domain, kind))
        assert (domain, kind) == (SOCKET_INET, SOCKET_STREAM) and self.open_count
        socket, self.next_socket = self.next_socket, self.next_socket + 1
        self.sockets[socket] = None
        self.error(self.read32(sp + 12), 0)
        return socket

    def net_SocketConnect(self, sp):
        socket, to, err = self.read16(sp + 2), self.read32(sp + 4), self.read32(sp + 14)
        family, port, address = struct.unpack('>hHI', self.uc.mem_read(to, 8))
        self.netlib_calls.append(('SocketConnect', socket, address, port))
        assert family == SOCKET_INET and self.read16(sp + 8) == 8
        assert socket in self.sockets and self.sockets[socket] is None
        server = self.servers.get((address, port))
        if server is None:
            self.error(err, ERR_UNREACHABLE)
            return 0xffff
        conn = Connection(server, address, port)
        self.connections.append(conn)
        self.sockets[socket] = conn
        self.error(err, 0)
        return 0

    def net_SocketClose(self, sp):
        socket = self.read16(sp + 2)
        self.netlib_calls.append(('SocketClose', socket))
        conn = self.sockets.pop(socket)
        if conn: conn.closed = True
        self.error(self.read32(sp + 8), 0)
        return 0

    def net_Send(self, sp):
        socket, data, length = self.read16(sp + 2), self.read32(sp + 4), self.read16(sp + 8)
        conn = self.sockets[socket]
        assert conn and not conn.closed and length
        conn.sent += self.uc.mem_read(data, length)
        conn.server.received(conn)
        self.error(self.read32(sp + 22), 0)
        return length

    def net_Receive(self, sp):
        socket, data, capacity = self.read16(sp + 2), self.read32(sp + 4), self.read16(sp + 8)
        timeout, err = self.read32(sp + 20), self.read32(sp + 24)
        conn = self.sockets[socket]
        self.netlib_calls.append(('Receive', socket))
        if conn.inbox:
            n = min(capacity, len(conn.inbox), conn.server.chunk or capacity)
            self.uc.mem_write(data, bytes(conn.inbox[:n]))
            del conn.inbox[:n]
            conn.delivered += n
            self.error(err, 0)
            return n
        if conn.remote_closed:
            self.error(err, 0)
            return 0
        self.now += timeout
        self.error(err, ERR_TIMEOUT)
        return 0xffff

    def net_IFGet(self, sp):
        index = self.read16(sp + 2)
        interfaces = [b'loop', b'ppp-']
        if index >= len(interfaces): return ERR_NO_IF
        self.uc.mem_write(self.read32(sp + 4), interfaces[index])
        self.uc.mem_write(self.read32(sp + 8), b'\0\0')
        return 0

    def net_IFSettingGet(self, sp):
        creator, setting = self.uc.mem_read(sp + 2, 4), self.read16(sp + 8)
        assert creator == b'ppp-' and setting == IF_ACTUAL_IP and self.read16(self.read32(sp + 14)) == 4
        self.uc.mem_write(self.read32(sp + 10), struct.pack('>I', self.local_ip))
        return 0

    def net_AddrINToA(self, sp):
        space = self.read32(sp + 6)
        self.uc.mem_write(space, str(ipaddress.IPv4Address(self.read32(sp + 2))).encode() + b'\0')
        self.uc.reg_write(UC_M68K_REG_A0, space)
        return 0

    # The library's calls.
    def select_network(self):
        assert self.set_config(config(NETWORK)) == 0

    def netlib_status(self):
        self.uc.mem_write(0x9300, struct.pack('>H', 36) + b'\xee' * 34)
        err = self.call('NioLibGetNetLibStatus', struct.pack('>HI', LIB, 0x9300))
        size, flags, sessions, connects, failures, _, bytes_in, bytes_out = struct.unpack(
            '>6H2I', self.uc.mem_read(0x9300, 20))
        ip = bytes(self.uc.mem_read(0x9314, 16)).split(b'\0')[0].decode('latin-1')
        return err, dict(flags=flags, sessions=sessions, connects=connects, failures=failures,
                         bytes_in=bytes_in, bytes_out=bytes_out, ip=ip)

    def hangup(self):
        return self.call('NioLibNetLibHangup', struct.pack('>H', LIB))

    def probe(self):
        self.uc.mem_write(0x9400, struct.pack('>6H', 12, 0, 0, 0, 0, 0))
        err = self.call('NioLibProbe', struct.pack('>HII', LIB, 0x9400, 0x9420))
        return err, self.read16(0x9420)

    def net_open(self, token, url='http://example.com/', method=1):
        self.uc.mem_write(0x8500, url.encode() + b'\0')
        self.uc.mem_write(0x8600, struct.pack('>6HII', 20, method, 0, 0, 0, 0, 0x8500, 0))
        err = self.call('NioLibNetOpen', struct.pack('>HIIII', LIB, token, 0x8600, 0x8618, 0x861a))
        return err, self.read16(0x8618), self.read16(0x861a)

    def info(self, token, handle):
        args = struct.pack('>HIHIIII', LIB, token, handle, 0x8800, 0x8804, 0x8808, 0x880a)
        err = self.call('NioLibNetInfo', args)
        return err, self.read16(0x880a), self.read16(0x8800), self.read32(0x8804), self.read16(0x8808)

    def read(self, token, handle, offset=0, capacity=256):
        args = struct.pack('>HIHIIHIII', LIB, token, handle, offset, 0x8900, capacity, 0x8f00, 0x8f02, 0x8f04)
        err = self.call('NioLibNetRead', args)
        return err, self.read16(0x8f04), bytes(self.uc.mem_read(0x8900, self.read16(0x8f00))), self.read16(0x8f02)

    def read_to_eof(self, token, handle, limit=20):
        """Reads as a client does, until EOF; returns the body and the number of reads."""
        data = b''
        for reads in range(1, limit + 1):
            err, result, chunk, flags = self.read(token, handle, len(data))
            assert (err, result) == (0, FN_OK), (err, result)
            data += chunk
            if flags & READ_EOF: return data, reads
        raise AssertionError('no EOF after %d reads' % limit)

    def write(self, token, handle, data, offset=0):
        self.uc.mem_write(0x8a00, data)
        args = struct.pack('>HIHIIHII', LIB, token, handle, offset, 0x8a00, len(data), 0x8f10, 0x8f12)
        err = self.call('NioLibNetWrite', args)
        return err, self.read16(0x8f12), self.read16(0x8f10)

    def net_close(self, token, handle):
        err = self.call('NioLibNetClose', struct.pack('>HIHI', LIB, token, handle, 0x8f20))
        return err, self.read16(0x8f20)

    def reset(self, transport=NETWORK, confirmation=1):
        return self.call('NioLibResetTransport', struct.pack('>HHH', LIB, transport, confirmation))


def request_lines(conn):
    head, _, _ = bytes(conn.sent).partition(b'\r\n\r\n')
    return head.decode('latin-1').split('\r\n')


@unittest.skipUnless(f.HAVE_UNICORN and HAS_NETLIB, 'requires ABI 1.7 artifacts and Unicorn')
class NetworkTransportTests(unittest.TestCase):
    def machine(self, netlib=True):
        m = NetLibMachine()
        m.netlib_present = netlib
        self.assertEqual(m.open(), 0)
        m.select_network()
        return m

    def begin(self, m):
        err, token, result = m.begin()
        self.assertEqual((err, result), (0, FN_OK))
        return token

    def test_http_get_sends_request_line_and_host_and_reads_scripted_body(self):
        m = self.machine()
        body = b'{"palm": "ok"}\n'
        m.servers[m.hosts['example.com'], 80] = HttpServer(body, chunk=9)
        m.servers[0x0a010203, 8080] = HttpServer(b'second')
        token = self.begin(m)
        status = m.status()[1]
        self.assertEqual((status[1], status[2]), (NETWORK, NETWORK))
        self.assertEqual(status[3] & (ACTIVE | AVAILABLE), ACTIVE | AVAILABLE)

        err, handle, result = m.net_open(token, 'http://example.com/data.json?x=1')
        self.assertEqual((err, result), (0, FN_OK)); self.assertNotEqual(handle, 0)
        self.assertEqual(len(m.calls('Open')), 1)
        self.assertIn(('GetHostByName', 'example.com'), m.netlib_calls)
        conn = m.connections[0]
        self.assertEqual((conn.address, conn.port), (m.hosts['example.com'], 80))
        lines = request_lines(conn)
        self.assertEqual(lines[0], 'GET /data.json?x=1 HTTP/1.0')
        self.assertIn('Host: example.com', lines[1:])
        self.assertTrue(conn.sent.endswith(b'\r\n\r\n'))

        err, result, http, length, flags = m.info(token, handle)
        self.assertEqual((err, result, http, length), (0, FN_OK, 200, len(body)))
        self.assertEqual(flags & (INFO_STATUS | INFO_LENGTH), INFO_STATUS | INFO_LENGTH)
        # The server sends 9 bytes at a time: the body comes in several reads, the last with EOF.
        data, reads = m.read_to_eof(token, handle)
        self.assertEqual(data, body)
        self.assertGreater(reads, 1)
        self.assertEqual(m.net_close(token, handle), (0, FN_OK))
        self.assertTrue(conn.closed)

        # A literal address skips DNS; a port other than 80 goes in the Host header.
        err, handle, result = m.net_open(token, 'http://10.1.2.3:8080/')
        self.assertEqual((err, result), (0, FN_OK))
        self.assertEqual(request_lines(m.connections[1])[0], 'GET / HTTP/1.0')
        self.assertIn('Host: 10.1.2.3:8080', request_lines(m.connections[1]))
        self.assertEqual(len(m.calls('GetHostByName')), 1)
        self.assertEqual(m.read(token, handle)[1:3], (FN_OK, b'second'))
        self.assertEqual(len(m.calls('Open')), 1)        # the link is brought up once per session
        self.assertEqual(m.end(token)[:2], (0, 0))
        self.assertEqual(m.calls('Close'), [('Close', 0)])  # NetLib keeps the link for its idle time
        self.assertEqual((m.open_count, m.close_wait), (0, True))
        self.assertFalse(m.sockets)

    def test_tcp_stream_write_then_read_round_trip(self):
        m = self.machine()
        m.servers[0x0a000007, 5000] = EchoServer()
        token = self.begin(m)
        err, handle, result = m.net_open(token, 'tcp://10.0.0.7:5000')
        self.assertEqual((err, result), (0, FN_OK))
        conn = m.connections[0]
        self.assertEqual(conn.sent, b'')                 # a stream sends nothing at open
        self.assertEqual(m.write(token, handle, b'ping\r\n'), (0, FN_OK, 6))
        self.assertEqual(conn.sent, b'ping\r\n')
        err, result, data, flags = m.read(token, handle)
        self.assertEqual((err, result, data), (0, FN_OK, b'ping\r\n'))
        self.assertFalse(flags & READ_EOF)
        self.assertEqual(m.write(token, handle, b'more', offset=6), (0, FN_OK, 4))
        self.assertEqual(m.read(token, handle, offset=6)[1:3], (FN_OK, b'more'))
        conn.remote_closed = True                        # the peer hangs up: end of stream
        err, result, data, flags = m.read(token, handle, offset=10)
        self.assertEqual((result, data), (FN_OK, b'')); self.assertTrue(flags & READ_EOF)
        self.assertEqual(m.end(token)[:2], (0, 0))
        self.assertTrue(conn.closed)

    def test_end_client_closes_every_socket_and_frees_the_session_state(self):
        m = self.machine()
        m.servers[m.hosts['example.com'], 80] = HttpServer(b'x' * 40)
        m.servers[0x0a000007, 5000] = EchoServer()
        idle = m.live_allocations()
        token = self.begin(m)
        state = [p for p in m.live_allocations() if p not in idle]
        self.assertEqual(len(state), 1)                  # the NIO core's state, for this session only
        handles = [m.net_open(token)[1], m.net_open(token)[1], m.net_open(token, 'tcp://10.0.0.7:5000')[1]]
        self.assertEqual(len(set(handles)), 3)
        self.assertEqual(m.netlib_status()[1]['sessions'], 3)
        err, result, cleanup = m.end(token)
        self.assertEqual((err, result), (0, 0))
        self.assertEqual(cleanup[1:5], (3, 3, 0, 0))     # attempted, confirmed, uncertain, ambiguous
        self.assertTrue(all(c.closed for c in m.connections)); self.assertFalse(m.sockets)
        self.assertEqual(m.calls('Close'), [('Close', 0)])
        self.assertIn(state[0], m.frees)
        self.assertEqual(m.live_allocations(), idle)
        err, after = m.netlib_status()
        self.assertEqual((after['flags'], after['sessions'], after['connects']), (PRESENT | UP, 0, 3))

        # The next session starts from nothing: no sessions, fresh counters, old handles unknown.
        token2 = self.begin(m)
        self.assertNotEqual(token2, token)
        err, status = m.netlib_status()
        self.assertEqual((status['flags'], status['sessions'], status['connects'], status['bytes_out']),
                         (PRESENT | SESSION, 0, 0, 0))
        receives = len(m.calls('Receive'))
        for handle in handles:
            self.assertNotEqual(m.read(token2, handle)[1], FN_OK)
        self.assertEqual(len(m.calls('Receive')), receives)
        self.assertEqual(m.end(token2)[:2], (0, 0))
        self.assertEqual(m.live_allocations(), idle)
        self.assertEqual(m.close(), (0, 0))
        self.assertEqual(len(m.allocations), len(m.frees))

    def test_missing_netlib_is_not_found_and_begin_allocates_nothing(self):
        m = self.machine(netlib=False)
        err, status = m.status()
        self.assertEqual((err, status[1], status[7]), (0, NETWORK, FN_NOT_FOUND))
        self.assertEqual(m.probe(), (0, FN_NOT_FOUND))
        allocations = len(m.allocations)
        err, token, result = m.begin()
        self.assertEqual((err, token, result), (0, 0, FN_NOT_FOUND))
        self.assertEqual(len(m.allocations), allocations)
        self.assertEqual(m.status()[1][2], 0)            # nothing active
        self.assertEqual(m.netlib_status(), (0, dict(flags=0, sessions=0, connects=0, failures=0,
                                                     bytes_in=0, bytes_out=0, ip='')))
        self.assertEqual(m.hangup(), 0)
        self.assertEqual(m.netlib_calls, [])
        self.assertEqual(m.close(), (0, 0))

    def test_netlib_status_outside_a_session_reports_the_link(self):
        for state in ('down', 'open', 'close-wait'):
            with self.subTest(state=state):
                m = self.machine()
                m.open_count = int(state == 'open')
                m.close_wait = state == 'close-wait'
                err, status = m.netlib_status()
                self.assertEqual(err, 0)
                if state == 'down':
                    self.assertEqual((status['flags'], status['ip']), (PRESENT, ''))
                else:
                    self.assertEqual((status['flags'], status['ip']), (PRESENT | UP, '10.0.0.5'))
                self.assertEqual(status['sessions'], 0)
                # Only the close-wait check takes NetLib, and lets it go again without hanging up.
                self.assertEqual(m.calls('Close'), [('Close', 0)] if state == 'close-wait' else [])
                self.assertEqual(m.calls('Open'), [])
                self.assertEqual((m.open_count, m.close_wait), (int(state == 'open'), state == 'close-wait'))

    def test_netlib_status_inside_a_session_counts_its_sessions_and_bytes(self):
        m = self.machine()
        m.servers[m.hosts['example.com'], 80] = HttpServer(b'abc')
        token = self.begin(m)
        err, status = m.netlib_status()
        self.assertEqual((err, status['flags'], status['sessions'], status['ip']), (0, PRESENT | SESSION, 0, ''))
        _, handle, _ = m.net_open(token)
        m.net_open(token)
        conn = m.connections[0]
        self.assertEqual(m.read(token, handle)[1:3], (FN_OK, b'abc'))
        err, status = m.netlib_status()
        self.assertEqual(status['flags'], PRESENT | SESSION | UP)
        self.assertEqual((status['sessions'], status['connects'], status['failures']), (2, 2, 0))
        self.assertEqual(status['bytes_out'], 2 * len(conn.sent))
        self.assertEqual(status['bytes_in'], conn.delivered)
        self.assertTrue(conn.delivered > 3)               # headers and body
        self.assertEqual(status['ip'], '10.0.0.5')
        self.assertEqual(m.calls('OpenCount'), [])       # a session's status is its own
        m.end(token)

    def test_hangup_is_busy_while_anything_has_netlib_open(self):
        m = self.machine()
        token = self.begin(m)
        self.assertEqual(m.hangup(), BUSY)
        m.end(token)
        m.netlib_calls.clear()
        m.open_count = 1                                 # another program has it open
        self.assertEqual(m.hangup(), BUSY)
        self.assertEqual(m.calls('Close'), [])
        m.netlib_calls.clear()
        m.open_count, m.close_wait = 0, True             # the link in its close-wait goes now
        self.assertEqual(m.hangup(), 0)
        self.assertEqual([c[0] for c in m.netlib_calls], ['OpenCount', 'OpenIfCloseWait', 'Close'])
        self.assertEqual(m.calls('Close'), [('Close', 1)])
        self.assertEqual((m.open_count, m.close_wait), (0, False))
        m.netlib_calls.clear()
        self.assertEqual(m.hangup(), 0)                  # nothing up: nothing to do
        self.assertEqual(m.calls('Close'), [])

    def test_refused_connect_and_unknown_host_are_io_errors_and_count_as_failures(self):
        m = self.machine()
        token = self.begin(m)
        err, handle, result = m.net_open(token, 'http://10.9.9.9/')
        self.assertEqual((err, handle, result), (0, 0, FN_IO))
        self.assertEqual(len(m.calls('SocketClose')), 1)  # the unconnected socket is not leaked
        self.assertFalse(m.sockets)
        err, handle, result = m.net_open(token, 'http://nowhere.invalid/')
        self.assertEqual((err, handle, result), (0, 0, FN_IO))
        self.assertEqual(len(m.calls('SocketOpen')), 1)   # no socket without an address
        err, status = m.netlib_status()
        self.assertEqual((status['sessions'], status['connects'], status['failures']), (0, 0, 2))
        err, result, cleanup = m.end(token)
        self.assertEqual((err, result, cleanup[1], cleanup[4]), (0, 0, 0, 0))
        self.assertEqual(m.netlib_status()[1]['failures'], 2)

    def test_netlib_that_cannot_open_or_raise_an_interface_fails_open_and_releases_netlib(self):
        for case in ('open error', 'interface error'):
            with self.subTest(case=case):
                m = self.machine()
                m.servers[m.hosts['example.com'], 80] = HttpServer()
                if case == 'open error': m.open_error = ERR_TIMEOUT
                else: m.if_errs = 1
                token = self.begin(m)
                err, handle, result = m.net_open(token)
                self.assertEqual((err, handle, result), (0, 0, FN_NOT_READY))
                self.assertEqual(m.calls('Close'), [] if case == 'open error' else [('Close', 1)])
                self.assertEqual(m.open_count, 0)
                self.assertFalse(m.calls('SocketOpen'))
                self.assertEqual(m.end(token)[:2], (0, 0))
                self.assertEqual(m.open_count, 0)

    def test_netlib_already_open_elsewhere_is_shared_not_taken(self):
        m = self.machine()
        m.servers[m.hosts['example.com'], 80] = HttpServer(b'shared')
        m.open_count = 1                                 # e.g. another program's session
        token = self.begin(m)
        _, handle, result = m.net_open(token)
        self.assertEqual(result, FN_OK)
        self.assertEqual(m.open_count, 2)
        self.assertEqual(m.read(token, handle)[1:3], (FN_OK, b'shared'))
        self.assertEqual(m.end(token)[:2], (0, 0))
        self.assertEqual((m.open_count, m.close_wait), (1, False))
        self.assertEqual(m.calls('Close'), [('Close', 0)])

    def test_reset_transport_network_is_accepted_only_when_idle(self):
        m = self.machine()
        m.servers[m.hosts['example.com'], 80] = HttpServer(b'after')
        self.assertEqual(m.reset(NETWORK), 0)
        self.assertEqual(m.reset(NETWORK, confirmation=0), PARAM)
        self.assertEqual(m.netlib_calls, [])             # nothing is held between sessions
        token = self.begin(m)
        m.net_open(token)
        calls = len(m.netlib_calls)
        self.assertEqual(m.reset(NETWORK), BUSY)
        self.assertEqual(len(m.netlib_calls), calls)
        m.end(token)
        m.netlib_calls.clear()
        self.assertEqual(m.reset(NETWORK), 0)
        self.assertEqual(m.netlib_calls, [])
        token = self.begin(m)                            # and the transport works after it
        _, handle, result = m.net_open(token)
        self.assertEqual(m.read(token, handle)[1:3], (FN_OK, b'after'))
        self.assertEqual(m.end(token)[:2], (0, 0))


if __name__ == '__main__':
    unittest.main()
