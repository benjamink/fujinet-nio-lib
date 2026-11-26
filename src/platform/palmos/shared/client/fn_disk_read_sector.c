#include "../client_private.h"

uint8_t fn_disk_read_sector(uint8_t slot, uint32_t lba, uint8_t * data, uint16_t data_capacity, uint16_t * data_length)
{
    uint8_t result = nl_client_extended_ready();
    if (result) return result;
    return nl_client_translate(NioLibDiskReadSector(nl_client_ref, nl_client_token,
        slot, lba, data, data_capacity, data_length, &nl_client_result_code));
}
