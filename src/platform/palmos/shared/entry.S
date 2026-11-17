/* Entry must be the first bytes of libr/0. All targets are in this resource. */
    .text
    .globl start
start:
    bra.w NioLibInstall
    .even
    .globl NioLibDispatch
NioLibDispatch:
    .word NioLibName - NioLibDispatch
    .word NioLibOpen - NioLibDispatch
    .word NioLibClose - NioLibDispatch
    .word NioLibSleep - NioLibDispatch
    .word NioLibWake - NioLibDispatch
    .word NioLibGetVersion - NioLibDispatch
    .word NioLibGetCapabilities - NioLibDispatch
    .word NioLibTestGlobals - NioLibDispatch
    .word NioLibBeginClient - NioLibDispatch
    .word NioLibEndClient - NioLibDispatch
    .word NioLibProbe - NioLibDispatch
    .word NioLibNetOpen - NioLibDispatch
    .word NioLibNetInfo - NioLibDispatch
    .word NioLibNetRead - NioLibDispatch
    .word NioLibNetWrite - NioLibDispatch
    .word NioLibNetClose - NioLibDispatch
    .word NioLibNetReady - NioLibDispatch
    .word NioLibClockGet - NioLibDispatch
    .word NioLibClockTimezone - NioLibDispatch
    .word NioLibGetConfig - NioLibDispatch
    .word NioLibSetConfig - NioLibDispatch
    .word NioLibGetTransportStatus - NioLibDispatch
    .word NioLibNetJsonQuery - NioLibDispatch
    .word NioLibResetTransport - NioLibDispatch
    .word NioLibRawCall - NioLibDispatch
    .word NioLibFileStat - NioLibDispatch
    .word NioLibFileRead - NioLibDispatch
    .word NioLibFileWrite - NioLibDispatch
    .word NioLibFileMkdir - NioLibDispatch
    .word NioLibFileListPages - NioLibDispatch
    .word NioLibOpenHeaders - NioLibDispatch
    .word NioLibFujiGetInfo - NioLibDispatch
    .word NioLibWifiGetStatus - NioLibDispatch
    .word NioLibWifiGetConfig - NioLibDispatch
    .word NioLibWifiSetConfig - NioLibDispatch
    .word NioLibWifiGetAdapterInfo - NioLibDispatch
    .word NioLibWifiScan - NioLibDispatch
    .word NioLibWifiTryJoin - NioLibDispatch
    .word NioLibWifiGetJoinStatus - NioLibDispatch
    .word NioLibAppstoreStat - NioLibDispatch
    .word NioLibAppstoreRead - NioLibDispatch
    .word NioLibAppstoreWrite - NioLibDispatch
    .word NioLibAppstoreDelete - NioLibDispatch
    .word NioLibAppstoreList - NioLibDispatch
    .word NioLibSlotCatalogGet - NioLibDispatch
    .word NioLibSlotCatalogPut - NioLibDispatch
    .word NioLibSlotCatalogDelete - NioLibDispatch
    .word NioLibSlotCatalogRange - NioLibDispatch
    .word NioLibResolveMountTarget - NioLibDispatch
    .word NioLibFormatMountDisplay - NioLibDispatch
    .word NioLibClockSetTimezone - NioLibDispatch
    .word NioLibClockSetTimezoneSave - NioLibDispatch
    .word NioLibDiskMount - NioLibDispatch
    .word NioLibDiskUnmount - NioLibDispatch
    .word NioLibDiskFlush - NioLibDispatch
    .word NioLibDiskInfo - NioLibDispatch
    .word NioLibDiskClearChanged - NioLibDispatch
    .word NioLibDiskRestoreBoot - NioLibDispatch
    .word NioLibDiskReadSector - NioLibDispatch
    .word NioLibDiskWriteSector - NioLibDispatch
    .word NioLibClockSet - NioLibDispatch
    .word NioLibClockSyncNetworkTime - NioLibDispatch
    .word NioLibClockGetFormat - NioLibDispatch
    .word NioLibClockGetTz - NioLibDispatch
    .word NioLibLegacySetAppkeyDetails - NioLibDispatch
    .word NioLibLegacyReadAppkey - NioLibDispatch
    .word NioLibLegacyReadAppkeyBuffered - NioLibDispatch
    .word NioLibLegacyWriteAppkey - NioLibDispatch
    .word NioLibDiskLastError - NioLibDispatch
    .word NioLibDiskInspect - NioLibDispatch
    .word NioLibFujiGetHsio - NioLibDispatch
    .word NioLibFujiSetHsio - NioLibDispatch
NioLibName:
    .asciz "NioLib"
    .even
