#include <assert.h>
#include <stdint.h>
#include "fujinet-nio.h"
static uint8_t selected, table[4], ids[4], disks[15], exists, fail_write, writes;
uint8_t u1mb_read(uint16_t a)
{
    if (a == 0xd800) return exists && selected == 1 ? 'X' : 0;
    if (a == 0xd801) return exists && selected == 1 ? 'U' : 0;
    if (a >= 0xd184 && a <= 0xd187) return ids[a - 0xd184];
    if (a >= 0xd174 && a <= 0xd182) return disks[a - 0xd174];
    if (a >= 0xd188 && a <= 0xd18b) return table[a - 0xd188];
    return 0;
}
void u1mb_write(uint16_t a, uint8_t v)
{
    if (a == 0xd1ff) selected = v;
    else if (a >= 0xd174 && a <= 0xd182) {
        assert(selected == 1); ++writes;
        if (!fail_write) disks[a - 0xd174] = v;
    }
    else if (a >= 0xd188 && a <= 0xd18b) {
        assert(selected == 1); ++writes;
        if (!fail_write) table[a - 0xd188] = v;
    } else assert(0);
}
int main(void)
{
    assert(fn_atari_u1mb_clear_devices(0) == FN_ERR_INVALID);
    assert(fn_atari_u1mb_clear_devices(16) == FN_ERR_INVALID);
    assert(fn_atari_u1mb_clear_devices(1) == FN_ERR_UNSUPPORTED && !writes && !selected);
    exists = 1; table[0] = 1; table[1] = 5; table[2] = 41; table[3] = 2;
    assert(fn_atari_u1mb_clear_devices(2) == FN_OK && !selected);
    assert(table[0] == 1 && table[1] == 0 && table[2] == 41 && table[3] == 2);
    fail_write = 1;
    assert(fn_atari_u1mb_clear_devices(1) == FN_ERR_IO && !selected);
    fail_write = 0;
    ids[0] = 0x7f; ids[1] = 0x70; ids[2] = 0x7f;
    assert(fn_atari_u1mb_clear_device(0x7f) == FN_OK);
    assert(table[0] == 0 && table[2] == 0 && table[3] == 2);
    assert(fn_atari_u1mb_clear_devices(15) == FN_OK);
    assert(!(table[0] | table[1] | table[2] | table[3]) && !selected);
    disks[0] = 5; disks[1] = 6; disks[14] = 1;
    assert(fn_atari_u1mb_clear_disks(0x4001) == FN_OK);
    assert(disks[0] == 0 && disks[1] == 6 && disks[14] == 0);
    assert(fn_atari_u1mb_clear_disks(0x8000) == FN_ERR_INVALID);
    return 0;
}
