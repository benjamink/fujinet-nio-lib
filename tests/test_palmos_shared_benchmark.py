"""Actual compiled public ABI 1.6 benchmark/client/service code, with modeled OS and peer.
No physical files are written. Costs are injected ticks, not hardware rates.
"""
import struct
import re
import unittest
import test_palmos_shared as f
from test_palmos_shared_network import reply, checksum
from test_palmos_shared_services import ServiceMachine
from test_palmos_shared_usb import USBMachine
from test_palmos_shared_transports import config
if f.HAVE_UNICORN:
    from unicorn.m68k_const import UC_M68K_REG_A0, UC_M68K_REG_A7, UC_M68K_REG_PC, UC_M68K_REG_D0

URI = b'host:/nio-write-bench.bin'
KEYS = ('bytes','ticks','rate','verifyBytes','verifyTicks','checksum',
        'initTicks','cleanupTicks','failedOffset','started','calls',
        'verifyCalls','errors','result','palm','cleanupResult','cleanupPalm','stage')

class FileModel:
    def __init__(self, mode='good'):
        self.mode = mode
        self.file = bytearray(b'old-file-tail' * 6000)
        self.writes, self.reads = [], []
        super().__init__()

    def execute(self, p):
        if p[:2] not in (b'\xfe\x03', b'\xfe\x04'):
            return super().execute(p)
        self.requests.append(p)
        assert checksum(p) == p[4]
        assert len(p) == int.from_bytes(p[2:4], 'little') and p[6] == 1
        n = int.from_bytes(p[7:9], 'little')
        assert p[9:9+n] == URI
        at = 9+n
        off, count = struct.unpack_from('<IH', p, at)
        if p[1] == 4:
            self.now += 2
            payload = p[at+6:]
            assert count == 512 and len(payload) == count
            assert payload == bytes(((off+i)^((off+i)>>8))&255 for i in range(count))
            self.writes.append(off)
            if self.mode == 'status':
                return reply(0xfe, 4, b'', status=5)
            accepted = 2 if self.mode == 'partial' else 0 if self.mode == 'zero' else count
            if off == 0: self.file.clear()
            assert off == len(self.file), 'unexpected write offset or replay'
            self.file.extend(payload[:accepted])
            echo = off+1 if self.mode == 'echo' else off
            acknowledged = 513 if self.mode == 'overcount' else accepted
            reserved = b'\0\x01' if self.mode == 'reserved' else b'\0\0'
            return reply(0xfe, 4, b'\x01\0'+reserved+struct.pack('<IH',echo,acknowledged))
        self.now += 5
        assert count == (1 if off == 65536 else 512)
        self.reads.append(off)
        if self.mode == 'read-status':
            return reply(0xfe, 3, b'', status=5)
        data = bytes(self.file[off:off+count])
        if self.mode == 'corrupt' and off == 0:
            data = bytes([data[0]^1])+data[1:]
        if self.mode == 'short-read' and off == 0:
            data = data[:-1]
        if self.mode == 'tail' and off == 65536:
            data = b'x'
        return reply(0xfe,3,b'\x01\0\0\0'+struct.pack('<IH',off,len(data))+data)

    def run_write(self):
        self.uc.mem_write(0x9000, URI+b'\0')
        self.call('NioTestWriteBenchmark',struct.pack('>I',0x9000),
                  app=True,a5=self.app_a5,budget=100000000)
        values = struct.unpack('>10I8H', self.uc.mem_read(
            self.app_data+self.app_symbols['nio_write_benchmark'],56))
        return dict(zip(KEYS,values))

class WriteMachine(FileModel, ServiceMachine):
    pass

class WriteUSBMachine(FileModel, USBMachine):
    pass

class ViewMachine(WriteMachine):
    def __init__(self):
        self.form=1030
        self.erases,self.draws,self.dialogs=[],[],[]
        self.inserts=[]
        self.alerts=[]
        self.confirm=1
        super().__init__()

    def string(self,address):
        data=bytearray()
        while self.uc.mem_read(address+len(data),1)[0]:
            data+=self.uc.mem_read(address+len(data),1)
        return data.decode()

    def hook(self,uc,pc,size,context):
        if getattr(self,'capture_write',False) and pc==self.APP+self.app_symbols['NioTestWriteBenchmark']:
            # UI routing only: core benchmark execution is covered separately.
            sp=uc.reg_read(UC_M68K_REG_A7)
            self.selected_uri=self.string(self.read32(sp+4))
            uc.reg_write(UC_M68K_REG_PC,self.read32(sp))
            uc.reg_write(UC_M68K_REG_A7,sp+4)
            return
        if bytes(uc.mem_read(pc,2))!=b'\x4e\x4f':
            return super().hook(uc,pc,size,context)
        trap,sp=self.read16(pc+2),uc.reg_read(UC_M68K_REG_A7)
        if trap==0xa175: result=self.form
        elif trap in (0xa173,0xa183,0xa139):
            uc.reg_write(UC_M68K_REG_A0,0x9000)
            result=0
        elif trap==0xa180: result=0
        elif trap==0xa171: result=0  # FrmDrawForm
        elif trap==0xa15d:
            text=self.string(self.read32(sp+4))
            self.inserts.append((text,self.read16(sp+8)))
            result=1
        elif trap==0xa026:
            uc.mem_write(self.read32(sp),bytes(uc.mem_read(
                self.read32(sp+4),self.read32(sp+8))))
            result=0
        elif trap==0xa192:
            self.alerts.append(self.read16(sp))
            if getattr(self,'capture_write',False):
                uc.mem_write(0x9000,b'moved field text\0')
            result=self.confirm
        elif trap==0xa219:
            self.erases.append(struct.unpack('>4h',uc.mem_read(self.read32(sp),8)))
            result=0
        elif trap==0xa220:
            self.draws.append((self.string(self.read32(sp)),self.read16(sp+6),self.read16(sp+8)))
            result=0
        elif trap==0xa194:
            self.dialogs.append(tuple(self.string(self.read32(sp+n)) for n in (2,6,10)))
            result=0
        elif trap==0xa2de:
            pos=sp+8
            def replace(m):
                nonlocal pos
                fmt=m[0]
                if fmt.endswith('s'):
                    value=self.string(self.read32(pos));pos+=4
                    return value
                width=4 if 'l' in fmt else 2
                value=self.read32(pos) if width==4 else self.read16(pos);pos+=width
                return fmt.replace('l','')%value
            data=re.sub(r'%0?\d*l?[suX]',replace,self.string(self.read32(sp+4))).encode()
            uc.mem_write(self.read32(sp),data+b'\0')
            result=len(data)
        else: return super().hook(uc,pc,size,context)
        uc.reg_write(UC_M68K_REG_D0,result)
        uc.reg_write(UC_M68K_REG_PC,pc+4)

@unittest.skipUnless(f.HAVE_UNICORN, 'requires compiled Palm benchmark app and Unicorn')
class FileBenchmarkTests(unittest.TestCase):
    def complete(self, m):
        v = m.run_write()
        self.assertEqual((v['bytes'],v['calls'],v['errors'],v['result'],v['stage']),
                         (65536,128,0,0,8))
        self.assertEqual((v['verifyBytes'],v['verifyCalls'],v['checksum']),
                         (65536,129,0x007f8000))
        self.assertEqual((v['ticks'],v['verifyTicks']), (256,645))
        self.assertEqual((v['cleanupResult'],v['cleanupPalm']), (0,0))
        self.assertEqual(m.writes, list(range(0,65536,512)))
        self.assertEqual(m.reads, list(range(0,65536,512))+[65536])
        self.assertEqual(bytes(m.file), bytes((i^(i>>8))&255 for i in range(65536)))
        self.assertEqual(m.globals,0)

    def test_a_full_write_readback_and_exact_timing_partition(self):
        self.complete(WriteMachine())

    def test_b_partial_and_zero_acknowledgements_preserve_progress_and_stop(self):
        for mode,count in (('partial',2),('zero',0)):
            with self.subTest(mode=mode):
                m=WriteMachine(mode);v=m.run_write()
                self.assertEqual((v['bytes'],v['calls'],v['verifyCalls'],v['stage']),
                                 (count,1,0,3))
                self.assertEqual(v['result'],5)
                self.assertEqual(m.writes,[0])
                self.assertEqual(len(m.file),count)
                self.assertEqual(v['cleanupResult'],0)
                self.assertEqual(m.globals,0)

    def test_c_rejections_and_bad_acks_stop_without_replay(self):
        for mode in ('echo','overcount','reserved','status'):
            with self.subTest(mode=mode):
                m=WriteMachine(mode);v=m.run_write()
                self.assertEqual((v['bytes'],v['calls'],v['verifyCalls'],v['stage']),
                                 (0,1,0,2))
                self.assertEqual(v['result'],5)
                self.assertEqual(v['cleanupResult'],0 if mode=='status' else 6)
                if mode!='status':
                    self.assertEqual(len(m.file),512) # zero confirmed bytes is not proof of no write
                self.assertEqual(m.writes,[0])
                self.assertEqual(m.globals,0)

    def test_d_timeout_does_not_send_again_or_start_verification(self):
        m=WriteMachine();m.silent=True
        v=m.run_write()
        self.assertEqual((v['bytes'],v['calls'],v['verifyCalls'],v['stage']), (0,1,0,2))
        self.assertNotEqual(v['result'],0)
        self.assertEqual(v['cleanupResult'],6)
        self.assertEqual(m.doorbells,1)
        self.assertEqual(m.globals,0)

    def test_e_readback_corruption_and_short_reads_fail_after_timed_writes(self):
        for mode,stage in (('corrupt',6),('short-read',5)):
            with self.subTest(mode=mode):
                m=WriteMachine(mode);v=m.run_write()
                self.assertEqual((v['bytes'],v['ticks'],v['verifyCalls'],v['stage']),
                                 (65536,256,1,stage))
                self.assertEqual(v['result'],5)
                self.assertEqual(v['cleanupResult'],0)
                self.assertEqual(m.globals,0)

    def test_f_unexpected_tail_is_rejected(self):
        m=WriteMachine('tail');v=m.run_write()
        self.assertEqual((v['bytes'],v['verifyBytes'],v['stage']), (65536,65536,7))
        self.assertEqual(v['result'],5)
        self.assertEqual(v['verifyCalls'],129)
        self.assertEqual(m.globals,0)

    def test_g_read_workload_verifies_64k_in_128_calls(self):
        m=WriteMachine()
        m.file=bytearray((i^(i>>8))&255 for i in range(65536))
        m.uc.mem_write(0x9000, URI+b'\0')
        m.call('NioTestBenchmark',struct.pack('>I',0x9000),app=True,a5=m.app_a5,budget=70000000)
        v=struct.unpack('>6I6H', m.uc.mem_read(m.app_data+m.app_symbols['nio_benchmark'],36))
        self.assertEqual((v[0],v[4],v[6:]), (65536,0x007f8000,(128,0,0,0,0,0)))
        self.assertFalse(m.writes)
        self.assertEqual(m.globals,0)

    def test_read_errors_stop_without_retry_or_writes(self):
        for mode,stage in (('read-status',2),('short-read',3),('corrupt',4)):
            with self.subTest(mode=mode):
                m=WriteMachine(mode)
                m.file=bytearray((i^(i>>8))&255 for i in range(65536))
                m.uc.mem_write(0x9000,URI+b'\0')
                m.call('NioTestBenchmark',struct.pack('>I',0x9000),
                       app=True,a5=m.app_a5,budget=10000000)
                values=struct.unpack('>6I6H',m.uc.mem_read(
                    m.app_data+m.app_symbols['nio_benchmark'],36))
                self.assertEqual((values[0],values[6],values[7],values[8]),(0,1,1,5))
                self.assertEqual(m.read16(m.app_data+m.app_symbols['nio_benchmark_failure']),stage)
                self.assertEqual(m.reads,[0])
                self.assertFalse(m.writes)
                self.assertEqual(m.globals,0)

    def test_init_failure_and_old_service_abi_never_write(self):
        m=WriteMachine();m.present=False
        v=m.run_write()
        self.assertEqual((v['stage'],v['calls'],v['verifyCalls']),(1,0,0))
        self.assertNotEqual(v['result'],0)
        self.assertFalse(m.writes)
        self.assertEqual(m.globals,0)
        m=WriteMachine();m.service_version=(8,1,5,8)
        v=m.run_write()
        self.assertNotEqual(v['result'],0)
        self.assertFalse(m.writes)
        self.assertEqual(m.globals,0)

    def test_i_usb_uses_the_same_wire_workload_and_verification(self):
        m=WriteUSBMachine()
        m.saved_preferences[0x4e694c31,0x4e34]=1,config(3)
        self.complete(m)

    def test_j_ui_resources_and_redraw_preserve_field_and_controls(self):
        m=ViewMachine()
        m.call('draw_results',b'',app=True,a5=m.app_a5)
        self.assertTrue(m.erases)
        # Erases must avoid editable field (ends at 57) and controls at 139.
        self.assertTrue(all(y>=57 and y+h<=139 for x,y,w,h in m.erases))
        self.assertTrue(all(61<=y and y+10<=139 for text,x,y in m.draws))
        self.assertTrue(any('Overwrites 64 KiB' in t for t,x,y in m.draws))
        self.assertEqual(m.mmio,0)
        m.call('benchmark_details',struct.pack('>H',0x0100),app=True,a5=m.app_a5)
        self.assertTrue(any('Verify' in a and 'Readback' in b for a,b,c in m.dialogs))
        metadata,resources=f.image.prc((f.OUT/'NioLibTest.prc').read_bytes())
        self.assertIn((b'tFRM',1030),resources)
        self.assertIn((b'tFRM',1020),resources)
        self.assertIn((b'Talt',1036),resources)
        self.assertIn((b'Talt',1027),resources)
        self.assertIn((b'Talt',1037),resources)
        self.assertEqual(m.mmio,0)

    def test_read_and_network_redraw_do_not_erase_controls(self):
        for form in (1020,1002):
            with self.subTest(form=form):
                m=ViewMachine();m.form=form
                m.call('draw_results',b'',app=True,a5=m.app_a5)
                bottom=139 if form==1020 else 122
                self.assertTrue(m.erases)
                self.assertTrue(all(y+h<=bottom for x,y,w,h in m.erases))
                if form==1020:
                    self.assertTrue(all(y>=57 for x,y,w,h in m.erases))
                self.assertEqual(m.mmio,0)

    def test_verified_transfer_with_uncertain_cleanup_is_not_displayed_as_pass(self):
        m=ViewMachine()
        address=m.app_data+m.app_symbols['nio_write_benchmark']
        values=[0]*18
        values[15]=6  # cleanupResult
        values[17]=8  # nioWriteDone
        m.uc.mem_write(address,struct.pack('>10I8H',*values))
        m.call('draw_results',b'',app=True,a5=m.app_a5)
        self.assertTrue(any('FAIL: cleanup' in text for text,x,y in m.draws))
        self.assertFalse(any('PASS:' in text for text,x,y in m.draws))

    def test_cancel_overwrite_confirmation_never_starts_client_or_writes(self):
        m=ViewMachine()
        m.uc.mem_write(0x9000,URI+b'\0')
        # Palm EventType: ctlSelectEvent, pen state/padding, x/y, control ID.
        m.uc.mem_write(0x9100,struct.pack('>HBBhhH',9,0,0,0,0,1032)+b'\0'*14)
        self.assertEqual(m.call('handle',struct.pack('>I',0x9100),
                               app=True,a5=m.app_a5),1)
        self.assertEqual(m.alerts,[1036])
        self.assertEqual(m.mmio,0)
        self.assertEqual(m.globals,0)
        self.assertFalse(m.writes)

    def test_default_uri_insert_uses_exact_string_length(self):
        for form,uri in ((1020,'tnfs+tcp://tnfs.fujinet.online/PALM/nio-throughput.bin'),
                         (1030,'host:/nio-write-bench.bin')):
            with self.subTest(form=form):
                m=ViewMachine();m.form=form
                m.uc.mem_write(0x9100,struct.pack('>H',24)+b'\0'*22) # frmOpenEvent
                m.call('handle',struct.pack('>I',0x9100),app=True,a5=m.app_a5)
                self.assertEqual(m.inserts,[(uri,len(uri))])
                self.assertEqual(m.mmio,0)

    def test_selected_uri_survives_field_text_changes_during_confirmation(self):
        m=ViewMachine();m.confirm=0;m.capture_write=True
        m.uc.mem_write(0x9000,URI+b'\0')
        m.uc.mem_write(0x9100,struct.pack('>HBBhhH',9,0,0,0,0,1032)+b'\0'*14)
        m.call('handle',struct.pack('>I',0x9100),app=True,a5=m.app_a5)
        self.assertEqual(m.selected_uri,URI.decode())
        self.assertEqual(m.alerts,[1036])
        self.assertEqual(m.mmio,0)
