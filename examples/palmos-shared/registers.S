/* Guard one real A807 call. A4 deliberately differs from library globals;
 * preserve the app's original A4/A5 even if the test detects a bad callee. */
    .text
    .globl NioTestGuardedGlobals
NioTestGuardedGlobals:
    movem.l %d2/%a2-%a5,-(%sp)
    movea.l #0x4a4a1234,%a4
    movea.l %a4,%a2
    movea.l %a5,%a3
    move.l 30(%sp),-(%sp)
    move.l 30(%sp),-(%sp)
    move.w 32(%sp),-(%sp)
    trap #15
    .word 0xa807
    lea 10(%sp),%sp
    movea.l 34(%sp),%a0
    clr.w (%a0)
    cmpa.l %a2,%a4
    bne.s 1f
    cmpa.l %a3,%a5
    bne.s 1f
    move.w #1,(%a0)
1:
    movem.l (%sp)+,%d2/%a2-%a5
    rts
