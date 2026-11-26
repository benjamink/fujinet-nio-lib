/* ABI 1.6 services. Existing fn_* result structures use fixed-width scalars,
 * Palm's two-byte alignment and caller-owned pointers; they are not packets.
 * Compile clients with the same public headers and the Palm SDK. No enum or
 * 64-bit scalar crosses a trap. All calls require the foreground client token.
 * Unknown capabilities must return Unsupported without invoking these traps. */
#ifndef NIOLIB_SERVICES_H
#define NIOLIB_SERVICES_H
#include "fujinet-nio.h"
#include "fn_raw.h"

#define nioLibTrapRawCall (sysLibTrapCustom + 19)
Err NioLibRawCall(UInt16 refNum, UInt32 token, UInt16 device, UInt16 command, const void * payload, UInt16 payload_length, void * reply, UInt16 reply_capacity, fn_raw_response_t * response, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapRawCall);

#define nioLibTrapFileStat (sysLibTrapCustom + 20)
Err NioLibFileStat(UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, fn_file_stat_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFileStat);

#define nioLibTrapFileRead (sysLibTrapCustom + 21)
Err NioLibFileRead(UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, UInt32 offset, uint8_t * data, UInt16 capacity, fn_file_read_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFileRead);

#define nioLibTrapFileWrite (sysLibTrapCustom + 22)
Err NioLibFileWrite(UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, UInt32 offset, const uint8_t * data, UInt16 length, fn_file_write_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFileWrite);

#define nioLibTrapFileMkdir (sysLibTrapCustom + 23)
Err NioLibFileMkdir(UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, UInt16 flags, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFileMkdir);

#define nioLibTrapFileListPages (sysLibTrapCustom + 24)
Err NioLibFileListPages(UInt16 refNum, UInt32 token, fn_file_io_t * io, const char * uri, const fn_list_pages_req_t * req, fn_list_pages_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFileListPages);

#define nioLibTrapOpenHeaders (sysLibTrapCustom + 25)
Err NioLibOpenHeaders(UInt16 refNum, UInt32 token, fn_handle_t * handle, UInt16 method, const char * url, UInt16 flags, const fn_http_header_t * headers, UInt16 header_count, UInt32 body_len, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapOpenHeaders);

#define nioLibTrapFujiGetInfo (sysLibTrapCustom + 26)
Err NioLibFujiGetInfo(UInt16 refNum, UInt32 token, fn_fuji_info_t * info, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFujiGetInfo);

#define nioLibTrapWifiGetStatus (sysLibTrapCustom + 27)
Err NioLibWifiGetStatus(UInt16 refNum, UInt32 token, fn_wifi_status_t * status, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiGetStatus);

#define nioLibTrapWifiGetConfig (sysLibTrapCustom + 28)
Err NioLibWifiGetConfig(UInt16 refNum, UInt32 token, fn_wifi_config_t * config, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiGetConfig);

#define nioLibTrapWifiSetConfig (sysLibTrapCustom + 29)
Err NioLibWifiSetConfig(UInt16 refNum, UInt32 token, const fn_wifi_config_update_t * update, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiSetConfig);

#define nioLibTrapWifiGetAdapterInfo (sysLibTrapCustom + 30)
Err NioLibWifiGetAdapterInfo(UInt16 refNum, UInt32 token, fn_wifi_adapter_info_t * info, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiGetAdapterInfo);

#define nioLibTrapWifiScan (sysLibTrapCustom + 31)
Err NioLibWifiScan(UInt16 refNum, UInt32 token, UInt16 offset, UInt16 limit, fn_wifi_scan_record_t * records, UInt16 capacity, uint8_t * count, uint8_t * more, uint8_t * response_buffer, UInt16 response_capacity, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiScan);

#define nioLibTrapWifiTryJoin (sysLibTrapCustom + 32)
Err NioLibWifiTryJoin(UInt16 refNum, UInt32 token, const char * ssid, const char * password, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiTryJoin);

#define nioLibTrapWifiGetJoinStatus (sysLibTrapCustom + 33)
Err NioLibWifiGetJoinStatus(UInt16 refNum, UInt32 token, uint8_t * state, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapWifiGetJoinStatus);

#define nioLibTrapAppstoreStat (sysLibTrapCustom + 34)
Err NioLibAppstoreStat(UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, fn_appstore_stat_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapAppstoreStat);

#define nioLibTrapAppstoreRead (sysLibTrapCustom + 35)
Err NioLibAppstoreRead(UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, UInt32 offset, uint8_t * buf, UInt16 max_len, fn_appstore_read_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapAppstoreRead);

#define nioLibTrapAppstoreWrite (sysLibTrapCustom + 36)
Err NioLibAppstoreWrite(UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, UInt32 offset, const uint8_t * data, UInt16 len, fn_appstore_write_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapAppstoreWrite);

#define nioLibTrapAppstoreDelete (sysLibTrapCustom + 37)
Err NioLibAppstoreDelete(UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, const char * key, fn_appstore_delete_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapAppstoreDelete);

#define nioLibTrapAppstoreList (sysLibTrapCustom + 38)
Err NioLibAppstoreList(UInt16 refNum, UInt32 token, fn_appstore_io_t * io, const char * namespace_name, UInt16 start_index, uint8_t * key_data, UInt16 key_data_capacity, fn_appstore_list_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapAppstoreList);

#define nioLibTrapSlotCatalogGet (sysLibTrapCustom + 39)
Err NioLibSlotCatalogGet(UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 index, fn_slot_catalog_entry_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapSlotCatalogGet);

#define nioLibTrapSlotCatalogPut (sysLibTrapCustom + 40)
Err NioLibSlotCatalogPut(UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 index, UInt16 flags, const char * target, fn_slot_catalog_entry_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapSlotCatalogPut);

#define nioLibTrapSlotCatalogDelete (sysLibTrapCustom + 41)
Err NioLibSlotCatalogDelete(UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 index, uint8_t * deleted, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapSlotCatalogDelete);

#define nioLibTrapSlotCatalogRange (sysLibTrapCustom + 42)
Err NioLibSlotCatalogRange(UInt16 refNum, UInt32 token, fn_slot_catalog_io_t * io, UInt16 lower, UInt16 upper, UInt16 cursor, UInt16 request_flags, UInt16 max_uri_bytes, UInt16 max_payload_bytes, fn_slot_catalog_page_t * out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapSlotCatalogRange);

#define nioLibTrapResolveMountTarget (sysLibTrapCustom + 43)
Err NioLibResolveMountTarget(UInt16 refNum, UInt32 token, fn_mount_resolve_io_t * io, const char * host, const char * browse_path, const char * leaf, char * canonical_uri, UInt16 canonical_cap, char * display_path, UInt16 display_cap, uint8_t * flags_out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapResolveMountTarget);

#define nioLibTrapFormatMountDisplay (sysLibTrapCustom + 44)
Err NioLibFormatMountDisplay(UInt16 refNum, UInt32 token, fn_mount_resolve_io_t * io, const char * canonical_uri, char * display_path, UInt16 display_cap, uint8_t * flags_out, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFormatMountDisplay);

#define nioLibTrapClockSetTimezone (sysLibTrapCustom + 45)
Err NioLibClockSetTimezone(UInt16 refNum, UInt32 token, const char * tz, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockSetTimezone);

#define nioLibTrapClockSetTimezoneSave (sysLibTrapCustom + 46)
Err NioLibClockSetTimezoneSave(UInt16 refNum, UInt32 token, const char * tz, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockSetTimezoneSave);

#define nioLibTrapDiskMount (sysLibTrapCustom + 47)
Err NioLibDiskMount(UInt16 refNum, UInt32 token, UInt16 slot, const char * uri, UInt16 readonly, UInt16 type, UInt16 sector_size_hint, fn_disk_info_t * info, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskMount);

#define nioLibTrapDiskUnmount (sysLibTrapCustom + 48)
Err NioLibDiskUnmount(UInt16 refNum, UInt32 token, UInt16 slot, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskUnmount);

#define nioLibTrapDiskFlush (sysLibTrapCustom + 49)
Err NioLibDiskFlush(UInt16 refNum, UInt32 token, UInt16 slot, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskFlush);

#define nioLibTrapDiskInfo (sysLibTrapCustom + 50)
Err NioLibDiskInfo(UInt16 refNum, UInt32 token, UInt16 slot, fn_disk_info_t * info, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskInfo);

#define nioLibTrapDiskClearChanged (sysLibTrapCustom + 51)
Err NioLibDiskClearChanged(UInt16 refNum, UInt32 token, UInt16 slot, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskClearChanged);

#define nioLibTrapDiskRestoreBoot (sysLibTrapCustom + 52)
Err NioLibDiskRestoreBoot(UInt16 refNum, UInt32 token, UInt16 slot, fn_disk_info_t * info, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskRestoreBoot);

#define nioLibTrapDiskReadSector (sysLibTrapCustom + 53)
Err NioLibDiskReadSector(UInt16 refNum, UInt32 token, UInt16 slot, UInt32 lba, uint8_t * data, UInt16 data_capacity, uint16_t * data_length, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskReadSector);

#define nioLibTrapDiskWriteSector (sysLibTrapCustom + 54)
Err NioLibDiskWriteSector(UInt16 refNum, UInt32 token, UInt16 slot, UInt32 lba, const uint8_t * data, UInt16 data_length, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskWriteSector);

#define nioLibTrapClockSet (sysLibTrapCustom + 55)
Err NioLibClockSet(UInt16 refNum, UInt32 token, const UInt8 * timeLE8, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockSet);

#define nioLibTrapClockSyncNetworkTime (sysLibTrapCustom + 56)
Err NioLibClockSyncNetworkTime(UInt16 refNum, UInt32 token, UInt8 * timeLE8, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockSyncNetworkTime);

#define nioLibTrapClockGetFormat (sysLibTrapCustom + 57)
Err NioLibClockGetFormat(UInt16 refNum, UInt32 token, UInt8 * time_data, UInt16 format, UInt16 capacity, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockGetFormat);

#define nioLibTrapClockGetTz (sysLibTrapCustom + 58)
Err NioLibClockGetTz(UInt16 refNum, UInt32 token, UInt8 * time_data, const Char * tz, UInt16 format, UInt16 capacity, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapClockGetTz);

#define nioLibTrapLegacySetAppkeyDetails (sysLibTrapCustom + 59)
Err NioLibLegacySetAppkeyDetails(UInt16 refNum, UInt32 token, UInt16 creator_id, UInt16 app_id, UInt16 keysize, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapLegacySetAppkeyDetails);

#define nioLibTrapLegacyReadAppkey (sysLibTrapCustom + 60)
Err NioLibLegacyReadAppkey(UInt16 refNum, UInt32 token, UInt16 key_id, uint16_t * count, uint8_t * data, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapLegacyReadAppkey);

#define nioLibTrapLegacyReadAppkeyBuffered (sysLibTrapCustom + 61)
Err NioLibLegacyReadAppkeyBuffered(UInt16 refNum, UInt32 token, UInt16 key_id, uint16_t * count, uint8_t * data, uint8_t * scratch, UInt16 scratch_size, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapLegacyReadAppkeyBuffered);

#define nioLibTrapLegacyWriteAppkey (sysLibTrapCustom + 62)
Err NioLibLegacyWriteAppkey(UInt16 refNum, UInt32 token, UInt16 key_id, UInt16 count, uint8_t * data, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapLegacyWriteAppkey);

#define nioLibTrapDiskLastError (sysLibTrapCustom + 63)
Err NioLibDiskLastError(UInt16 refNum, UInt32 token, UInt16 * errorP, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskLastError);

#define nioLibTrapDiskInspect (sysLibTrapCustom + 64)
Err NioLibDiskInspect(UInt16 refNum, UInt32 token, fn_service_io_t *io,
    const char *uri, UInt16 type, UInt16 hint, fn_disk_inspection_t *out,
    UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapDiskInspect);
#define nioLibTrapFujiGetHsio (sysLibTrapCustom + 65)
Err NioLibFujiGetHsio(UInt16 refNum, UInt32 token, uint8_t *index, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFujiGetHsio);
#define nioLibTrapFujiSetHsio (sysLibTrapCustom + 66)
Err NioLibFujiSetHsio(UInt16 refNum, UInt32 token, UInt16 index, UInt16 *resultP) NIO_LIB_TRAP(nioLibTrapFujiSetHsio);
#endif
