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
NioLibName:
    .asciz "NioLib"
    .even
