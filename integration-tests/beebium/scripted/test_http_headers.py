import time
import pytest
from conftest import _build_bbc_test_ssd, _HERE
from fuji_device import disk_image_responder, build_network_open_response, build_network_close_response
from helpers import command, wait_for_screen_text

@pytest.fixture(scope='session')
def headers_ssd():
    return _build_bbc_test_ssd(_HERE.parent/'apps/http_headers.c','HEADERS','HEADERS')

def test_headers_body_length_and_one_shot_cleanup(beebium,fuji_device,headers_ssd):
    opens=[]
    def inner(pkt):
        if pkt.device!=0xfd:return None
        if pkt.command==1:
            opens.append(pkt.payload)
            return build_network_open_response(handle=0x1234,proto_flags=0)
        if pkt.command==4:return build_network_close_response()
    fuji_device.set_responder(disk_image_responder(image_path=str(headers_ssd),catalog_slot=7,
        drive_slot=4,uri='sd0:/headers.ssd',inner=inner))
    for line in ('*FHOST sd0:/','*FIN 7 headers.ssd','*FMOUNT 7 0','*RUN HEADERS'):
        command(beebium,line);time.sleep(.2)
    wait_for_screen_text(beebium,'HEADERS OK')
    def string(s):
        b=s.encode();return len(b).to_bytes(2,'little')+b
    hs=(2).to_bytes(2,'little')+string('Authorization')+string('Bearer bbc-test')+string('X-Client')+string('BBC')
    assert opens[0]==bytes([1,1,0])+string('http://example.com/')+hs+bytes(6)
    assert opens[1]==bytes([1,2,0x20])+string('http://example.com/upload')+hs+bytes.fromhex('785634120000')
    assert opens[2]==bytes([1,1,8])+string('http://example.com/plain')+bytes(8)
