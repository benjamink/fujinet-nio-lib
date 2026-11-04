"""BBC library -> MOS ROM -> FujiBus, including aliased request/reply buffers."""
import time
import pytest
from conftest import _build_bbc_test_ssd, _HERE
from fuji_device import disk_image_responder
from fujinet_tools import fujibus as fb
from helpers import command, wait_for_screen_text

@pytest.fixture(scope='session')
def pages_ssd():
    return _build_bbc_test_ssd(_HERE.parent / 'apps/list_pages.c', 'PAGES', 'PAGES')

def test_list_pages_filter_paging_and_short_reply(beebium, fuji_device, pages_ssd):
    seen = []
    def inner(pkt):
        if (pkt.device, pkt.command) != (0xfe, 7):
            return None
        seen.append(pkt.payload)
        page = len(seen)-1
        # Third response deliberately violates the minimum reply size.
        data = b'\x01' if page == 2 else (
            bytes([1, 0, 0, 0]) + (2).to_bytes(4, 'little') +
            page.to_bytes(2, 'little') + bytes([1, 1, 1, 1, 0, page]))
        return fb.build_fuji_response_wire(0xfe, 7, 0, data)
    fuji_device.set_responder(disk_image_responder(image_path=str(pages_ssd),
        catalog_slot=7, drive_slot=4, uri='sd0:/pages.ssd', inner=inner))
    for line in ('*FHOST sd0:/', '*FIN 7 pages.ssd', '*FMOUNT 7 0', '*RUN PAGES'):
        command(beebium, line)
        time.sleep(.2)
    wait_for_screen_text(beebium, 'PAGES OK')
    prefix = b'\x01\x0a\x00sd0:/games'
    assert seen == [prefix + bytes([0,0,1,120,0,2,40,7]) + b'*MiNeR*',
                    prefix + bytes([1,0,1,120,0,2,40,7]) + b'*MiNeR*',
                    prefix + bytes([1,0,1,120,0,2,40])]
