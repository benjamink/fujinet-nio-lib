; Polled 19200 SIO derived from Michael Sternberg's SYN-ACK WIRE net_sio.s
; (GPL-3.0, commit 3b6fa091). Channels 1/2, VBI and DLI remain available.
; Private register ABI: fn_stream_buf/frame/len/mode, A=0 success else $8a.
.export fn_stream_sio, fn_stream_wait_serial
.importzp fn_stream_buf
.import fn_stream_frame, fn_stream_len, fn_stream_mode
RTCLOK=$14
POKMSK=$10
SSKCTL=$0232
AUDF3=$D204
AUDF4=$D206
AUDCTL=$D208
SKREST=$D20A
SERIO=$D20D
IRQEN=$D20E
SKCTL=$D20F
PBCTL=$D303
WSYNC=$D40A
TRIES=13
ACKWAIT=15
KEYS=$C0
SERIN=$20
SEROR=$10
SEROC=$08
SIOERR=$8A
.bss
sum: .res 1
txbusy: .res 1
ticks: .res 1
tlast: .res 1
tries: .res 1
want: .res 1
io_error: .res 1
waitmask: .res 1
.code
fn_stream_sio:
    lda #0
    sta io_error
        php
        sei
        lda #$28                ; channel 3 at 1.79 MHz, joined to 4: 19200 baud
        sta AUDCTL              ; (channels 3 and 4 stay at volume 0: the OS's
        sta AUDF3               ; SIO leaves them so, and nothing else plays them)
        lda #0
        sta AUDF4
        lda #TRIES
        sta tries
@cmd:   lda io_error
        beq :+
        sec
        jmp @end
:       jsr command
        bcc @acked
        dec tries
        bne @cmd
        beq @end                ; always (carry set)
@acked: bit fn_stream_mode
        bmi @write
        bvc @done               ; neither way: just COMPLETE
        jsr complete
        bcs @end
        sta sum                 ; A = 0: the checksum covers the data only
        tay
:       jsr getb
        bcs @end
        sta (fn_stream_buf),y
        iny
        cpy fn_stream_len
        bne :-
        lda sum
        sta want
        jsr getb
        bcs @end
        eor want
        bne @bad
        lda SKCTL               ; framing error or overrun? (bits 7, 6: 0 if so)
        and #$C0
        eor #$C0
        beq @end                ; no: fine (carry clear)
@bad:   sec
        bcs @end                ; always
@write: ldy #20                 ; at least 1 ms after the ACK
        jsr delay
        jsr sendstart
        ldy #0
:       lda (fn_stream_buf),y
        jsr putb
        iny
        cpy fn_stream_len
        bne :-
        jsr sendsum
        jsr ack
        bcs @end
@done:  jsr complete
@end:   lda #0
        bcc :+
        lda #SIOERR
:       tay
        lda SSKCTL              ; the serial port, IRQs and COMMAND as the OS left them
        sta SKCTL
        lda POKMSK
        sta IRQEN
        lda #$3C
        sta PBCTL
        plp
        ldx #0
        tya                     ; Z for the callers
        rts

; Send the command frame and wait for its ACK: carry clear if it came.
command:
        lda #$34                ; COMMAND low
        sta PBCTL
        ldy #14                 ; 750-1600 us before the frame
        jsr delay
        jsr sendstart
        ldy #0
:       lda fn_stream_frame,y
        jsr putb
        iny
        cpy #4
        bne :-
        jsr sendsum
        ldy #12                 ; 650-950 us after it
        jsr delay
        lda #$3C                ; COMMAND high
        sta PBCTL
ack:    jsr getb                ; (sendsum left us listening)
        bcs no
        cmp #'A'
        beq yes
no:     sec
        rts

; Wait up to 16 s for COMPLETE: carry clear and A = 0 if it came.
complete:
        lda #4
        sta want
:       lda #240
        jsr getbt
        bcc :+
        dec want
        bne :-
        rts                     ; carry set
:       cmp #'C'
        bne no
yes:    lda #0
        clc
        rts

; A byte into A, added to the checksum: carry clear, or set if none came in
; ACKWAIT frames (getbt: in A frames). Keeps Y.
getb:   lda io_error
        beq :+
        sec
        rts
:       lda #ACKWAIT
getbt:  sta ticks
        lda RTCLOK
        sta tlast
@wait:  lda IRQEN
        and #SERIN
        beq @got
        lda RTCLOK
        cmp tlast
        beq @wait
        sta tlast
        dec ticks
        bne @wait
        sec
        rts
@got:   ldx #KEYS|SERIN         ; let the latch take the next byte
        jsr irqs
        lda SERIO
addsum: pha                     ; add A to the checksum; keeps A, carry clear
        clc
        adc sum
        adc #0
        sta sum
        pla
        clc
        rts

; Send the checksum, wait for it to go out, then listen.
sendsum:
        lda sum
        jsr putb
        lda #SEROR
        jsr fn_stream_wait_serial
        lda #SEROC
        jsr fn_stream_wait_serial
        lda #$13                ; receive, clocked by channels 3 and 4
        sta SKCTL
        sta SKREST
        ldx #KEYS|SERIN
        bne irqs                ; always

; Send A, adding it to the checksum. Keeps Y.
putb:   jsr addsum
        bit txbusy              ; the frame's first byte goes straight out
        bpl @put
        pha
        lda #SEROR
        jsr fn_stream_wait_serial
        ldx #KEYS|SEROR|SEROC
        jsr irqs
        pla
@put:   sta SERIO
        sec
        ror txbusy
        rts

; Ready POKEY to send a frame; the checksum starts at 0.
sendstart:
        lda #$23                ; send, clocked by channel 4
        sta SKCTL
        sta SKREST
        lda #0
        sta sum
        sta txbusy
        ldx #KEYS|SEROR|SEROC
        ; fall through

; Re-arm POKEY's IRQ latches: IRQEN to the keys alone, then to X.
irqs:   lda #KEYS
        sta IRQEN
        stx IRQEN
        rts

; Wait Y scan lines.
delay:  sta WSYNC
        dey
        bne delay
        rts

; A stuck transmitter must not trap the client in a hardware polling loop.
fn_stream_wait_serial:
    sta waitmask
    lda #ACKWAIT
    sta ticks
    lda RTCLOK
    sta tlast
@wait:
    lda IRQEN
    and waitmask
    beq @ready
    lda RTCLOK
    cmp tlast
    beq @wait
    sta tlast
    dec ticks
    bne @wait
    lda #1
    sta io_error
@ready:
    rts
