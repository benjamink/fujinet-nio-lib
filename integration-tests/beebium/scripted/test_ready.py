import time
import pytest
from conftest import _build_bbc_test_ssd, _HERE
from fuji_device import (disk_image_responder, build_network_open_response,
    build_network_read_response, build_network_close_response)
from fujinet_tools import fujibus as fb
from helpers import command, wait_for_screen_text

@pytest.fixture(scope='session')
def ready_ssd():
    return _build_bbc_test_ssd(_HERE.parent/'apps/ready.c','READY','READY')

def test_ready_includes_rom_buffer_when_firmware_has_no_ready_handles(beebium,fuji_device,ready_ssd):
    ready_calls=[]
    def inner(pkt):
        if pkt.device!=0xfd:return None
        if pkt.command==1:return build_network_open_response(handle=0x1234,proto_flags=4)
        if pkt.command==2:return build_network_read_response(handle=0x1234,offset=0,data=b'abc',eof=False)
        if pkt.command==4:return build_network_close_response()
        if pkt.command==8:
            ready_calls.append(pkt.payload)
            return fb.build_fuji_response_wire(0xfd,8,0,bytes([1,0,0,0,0]))
    fuji_device.set_responder(disk_image_responder(image_path=str(ready_ssd),catalog_slot=7,
        drive_slot=4,uri='sd0:/ready.ssd',inner=inner))
    for line in ('*FHOST sd0:/','*FIN 7 ready.ssd','*FMOUNT 7 0','*RUN READY'):
        command(beebium,line);time.sleep(.2)
    wait_for_screen_text(beebium,'READY OK')
    assert ready_calls==[b'\x01']
