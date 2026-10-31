#include <stdint.h>
#include "fujinet-nio.h"

/* Hooks are used only by the host test, which models PBI selection. */
#ifdef FN_U1MB_TEST
extern uint8_t u1mb_read(uint16_t address);
extern void u1mb_write(uint16_t address, uint8_t value);
#else
static uint8_t u1mb_read(uint16_t address) { return *(volatile uint8_t *)address; }
static void u1mb_write(uint16_t address, uint8_t value) { *(volatile uint8_t *)address = value; }
#endif

/* The OS owns the selected-device shadow. Write the shadow before PDVS so a
 * PBI IRQ can restore the correct selection; restore the caller's selection. */
static void select_pbi(uint8_t select)
{
    u1mb_write(0x0248u, select);
    u1mb_write(0xd1ffu, select);
}

static uint8_t find_pbi(void)
{
    uint8_t select, a, b;
    for (select = 0x80; select; select >>= 1) {
        select_pbi(select);
        /* Generic Atari PBI ROM identification plus the BIOS family marker. */
        if (u1mb_read(0xd803u) != 0x80 || u1mb_read(0xd80bu) != 0x91) continue;
        a = u1mb_read(0xd800u); b = u1mb_read(0xd801u);
        if ((a == 'X' && b == 'U') || (a == 'I' && b == 'C') || (a == 'S' && b == '3')) return select;
    }
    return 0;
}

uint8_t fn_atari_u1mb_pbi(void)
{
    uint8_t original = u1mb_read(0x0248u), select = find_pbi();
    select_pbi(original);
    return select;
}

static uint8_t clear(uint8_t mask, uint8_t device, uint8_t by_id)
{
    uint8_t original = u1mb_read(0x0248u), select = find_pbi(), i, result = FN_OK;
    if (!select) result = FN_ERR_UNSUPPORTED;
    else for (i = 0; i < 4; ++i) {
        if ((by_id && u1mb_read(0xd184u + i) == device) || (!by_id && (mask & (1u << i)))) {
            u1mb_write(0xd188u + i, 0);
            if (u1mb_read(0xd188u + i)) result = FN_ERR_IO;
        }
    }
    select_pbi(original);
    return result;
}

uint8_t fn_atari_u1mb_clear_devices(uint8_t mask)
{
    if (!mask || (mask & 0xf0)) return FN_ERR_INVALID;
    return clear(mask, 0, 0);
}

uint8_t fn_atari_u1mb_clear_device(uint8_t device)
{
    if (!device) return FN_ERR_INVALID;
    return clear(0, device, 1);
}

uint8_t fn_atari_u1mb_clear_disks(uint16_t mask)
{
    uint8_t select, original, i, result = FN_OK;
    if (!mask || (mask & 0x8000u)) return FN_ERR_INVALID;
    original = u1mb_read(0x0248u);
    select = find_pbi();
    if (!select) result = FN_ERR_UNSUPPORTED;
    else for (i = 0; i < 15; ++i) {
        if (mask & ((uint16_t)1 << i)) {
            u1mb_write(0xd174u + i, 0);
            if (u1mb_read(0xd174u + i)) result = FN_ERR_IO;
        }
    }
    select_pbi(original);
    return result;
}
