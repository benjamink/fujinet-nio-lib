"""Actual compiled cached-JSON trap/client over mocked CS1/serial/USB peers.
The peer emulates the existing host command, never another HTTP GET for a query.
"""
import json
import struct
import unittest
import test_palmos_shared as f
from test_palmos_shared_network import checksum, reply
from test_palmos_shared_usb import USBMachine
from test_palmos_shared_transports import config
if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC


class CachedJSONPeer:
    def __init__(self):
        self.documents = {}
        self.json_pending_config = self.json_pending_reads = 0
        self.json_bad = None
        self.http_opens = 0
        super().__init__()

    def execute(self, request):
        dev, cmd = request[:2]
        handle = int.from_bytes(request[7:9], 'little')
        if dev == 0xfd and cmd == 1:
            response = super().execute(request)
            handle = int.from_bytes(response[11:13], 'little')
            self.http_opens += 1
            self.documents[handle] = {'value': 'first', 'other': 'second',
                'fetch': self.http_opens, 'timestamp': 1234567890,
                'iss_position': {'latitude': '12.34', 'longitude': '56.78'},
                'status': 'success', 'city': 'Example', 'regionName': 'Region',
                'countryCode': 'US', 'lat': 12.34, 'lon': 56.78,
                'current': {'temperature_2m': 20, 'apparent_temperature': 19,
                    'wind_speed_10m': 4, 'wind_direction_10m': 180,
                    'relative_humidity_2m': 50, 'surface_pressure': 1010, 'weather_code': 1},
                'utc_offset_seconds': 0,
                'daily': {'sunrise': [1000], 'sunset': [2000], 'time': list(range(6)),
                    'temperature_2m_max': [20]*6, 'temperature_2m_min': [10]*6,
                    'weather_code': [1]*6}}
            return response
        if dev == 0xfd and cmd == 4:
            response = super().execute(request)
            self.documents.pop(handle, None)
            return response
        if dev == 0xfd and cmd == 7:
            self.requests.append(request)
            assert len(request) == int.from_bytes(request[2:4], 'little')
            assert checksum(request) == request[4]
            assert request[6] == 1 and request[9:11] == b'\x01\0'
            assert int.from_bytes(request[11:13], 'little') == len(request)-13
            if handle not in self.documents: return reply(dev, cmd, status=0x44)
            selector = request[13:].decode()
            item = self.documents[handle]
            try:
                if selector:
                    if not selector.startswith('/'): raise KeyError(selector)
                    for part in selector[1:].split('/'):
                        part = part.replace('~1', '/').replace('~0', '~')
                        item = item[int(part)] if isinstance(item, list) else item[part]
                value = json.dumps(item) if isinstance(item, (dict, list)) else str(item)
            except (KeyError, ValueError, IndexError): value = ''
            self.sessions[handle] = value.encode()
            if self.json_pending_config:
                self.json_pending_config -= 1
                return reply(dev, cmd, status=4)  # selector APPLIED, first body pending
            payload = struct.pack('<BBHHI', 1, 1, 0, handle, len(self.sessions[handle]))
            if self.json_bad == 'handle': payload = payload[:4] + struct.pack('<H',handle+1) + payload[6:]
            if self.json_bad == 'version': payload = b'\x02' + payload[1:]
            if self.json_bad == 'reserved': payload = payload[:2] + b'\x01\0' + payload[4:]
            if self.json_bad == 'short': payload = payload[:6]
            if self.json_bad == 'not_ready_flag': payload = payload[:1] + b'\0' + payload[2:]
            return reply(dev, cmd, payload)
        if dev == 0xfd and cmd == 2 and self.json_pending_reads:
            self.requests.append(request)
            self.json_pending_reads -= 1
            return reply(dev,cmd,status=4)
        return super().execute(request)


class JSONMachine(CachedJSONPeer, USBMachine):
    pass


@unittest.skipUnless(f.HAVE_UNICORN and (f.OUT/'json.o').exists(), 'requires JSON candidate and Unicorn')
class JSONTests(unittest.TestCase):
    def machine(self, transport=1):
        m=JSONMachine(); m.open(); m.set_config(config(transport)); _,token,result=m.begin()
        self.assertEqual(result,0)
        _,handle,result=m.net_open(token); self.assertEqual(result,0)
        return m,token,handle

    def query(self,m,token,handle,path='/value'):
        m.uc.mem_write(0x9500,path.encode()+b'\0')
        err=m.call('NioLibNetJsonQuery',struct.pack('>HIHII',7,token,handle,0x9500,0x9400))
        return err,m.read16(0x9400)

    def test_exact_wire_same_handle_different_fields_all_transports(self):
        for transport in (1,2,3):
            m,t,h=self.machine(transport)
            for selector,wanted in (('/value',b'first'),('/other',b'second'),('/fetch',b'1')):
                self.assertEqual(self.query(m,t,h,selector),(0,0))
                args=struct.pack('>HIHIIHIII',7,t,h,0,0x9800,32,0x9600,0x9602,0x9604)
                self.assertEqual(m.call('NioLibNetRead',args),0)
                self.assertEqual(m.read16(0x9604),0)
                self.assertEqual(bytes(m.uc.mem_read(0x9800,m.read16(0x9600))),wanted)
            self.assertEqual(m.http_opens,1); self.assertEqual(len(m.documents),1)
            queries=[p for p in m.requests if p[:2]==b'\xfd\x07']
            self.assertEqual(len(queries),3)
            payload=struct.pack('<B H B B H',1,h,1,0,6)+b'/value'
            expected=bytearray(struct.pack('<BBHBB',0xfd,7,19,0,0)+payload); expected[4]=checksum(expected)
            self.assertEqual(queries[0],bytes(expected))
            m.end(t); self.assertFalse(m.documents); self.assertFalse(m.sessions)
            self.assertEqual(m.close(),(0,0))

    def test_handle_owner_bounds_and_reply_validation(self):
        m,t,h=self.machine(); before=len(m.requests)
        self.assertEqual(self.query(m,t+1,h)[0],0x8007)
        self.assertNotEqual(self.query(m,t,h+99)[1],0)
        self.assertEqual(len(m.requests),before)
        self.assertNotEqual(self.query(m,t,h,'/'+('x'*1011))[1],0) # 1012 exceeds packet
        self.assertEqual(len(m.requests),before)
        self.query(m,t,h,'/'+('x'*1010)) # 1011 + 13 = 1024, no truncation
        self.assertEqual(len(m.requests[-1]),1024)
        for bad in ('handle','version','reserved','short','not_ready_flag'):
            for transport in (1,2,3):
                m,t,h=self.machine(transport); m.json_bad=bad
                self.assertNotEqual(self.query(m,t,h)[1],0,(bad,transport))
                m.end(t); m.close()

    def test_reselect_resets_only_the_selected_handles_read_cursor(self):
        m,t,first=self.machine()
        _,second,result=m.net_open(t);self.assertEqual(result,0)
        def read(handle,offset,count):
            args=struct.pack('>HIHIIHIII',7,t,handle,offset,0x9800,count,0x9600,0x9602,0x9604)
            self.assertEqual(m.call('NioLibNetRead',args),0)
            self.assertEqual(m.read16(0x9604),0)
            return bytes(m.uc.mem_read(0x9800,m.read16(0x9600)))
        self.assertEqual(self.query(m,t,first),(0,0));self.assertEqual(read(first,0,2),b'fi')
        self.assertEqual(self.query(m,t,second,'/other'),(0,0));self.assertEqual(read(second,0,2),b'se')
        self.assertEqual(self.query(m,t,second),(0,0));self.assertEqual(read(second,0,2),b'fi')
        self.assertEqual(read(first,2,3),b'rst')
        self.assertEqual(m.http_opens,2)
        m.end(t);self.assertFalse(m.documents);self.assertEqual(m.close(),(0,0))

    def test_client_rejects_older_library_without_calling_new_trap(self):
        class Older(JSONMachine):
            def hook(self,uc,pc,size,ctx):
                if bytes(uc.mem_read(pc,2))==b'\x4e\x4f':
                    trap,sp=self.read16(pc+2),uc.reg_read(UC_M68K_REG_A7)
                    if trap==0xa816: raise AssertionError('new trap used on old ABI')
                    if trap==0xa805:
                        uc.mem_write(self.read32(sp+2),struct.pack('>4H',8,1,3,6))
                        uc.reg_write(UC_M68K_REG_D0,0);uc.reg_write(UC_M68K_REG_PC,pc+4);return
                super().hook(uc,pc,size,ctx)
        m=Older();self.assertEqual(m.client('fn_init'),0)
        m.uc.mem_write(0x9500,b'/value\0'); before=len(m.requests)
        self.assertNotEqual(m.client('fn_json_query',struct.pack('>HI',0x100,0x9500)),0)
        self.assertEqual(len(m.requests),before)
        m.client('fn_shutdown');self.assertFalse(m.globals)
