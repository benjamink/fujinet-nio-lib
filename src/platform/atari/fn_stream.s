; Experimental small stream API, one AX block pointer, no cc65 runtime.
.macpack longbranch
.export _fn_stream_init, _fn_stream_open, _fn_stream_read
.export _fn_stream_write, _fn_stream_close, _fn_stream_appkey
.exportzp fn_stream_buf
.export fn_stream_frame, fn_stream_len, fn_stream_mode
.ifdef FN_STREAM_TEST_IO
.import fn_stream_test_io
fn_stream_sio = fn_stream_test_io
.else
.import fn_stream_sio
.endif
.zeropage
ctx: .res 2
fn_stream_buf: .res 2
.bss
fn_stream_frame: .res 4
fn_stream_len: .res 1
fn_stream_mode: .res 1
op: .res 1
reply_len: .res 1
tries: .res 1
.code
save:
    sta ctx
    stx ctx+1
    lda #$7f
    sta fn_stream_frame
    lda #0
    sta fn_stream_frame+3
    rts
buffer_reply:
    clc
    lda ctx
    adc #131
    sta fn_stream_buf
    lda ctx+1
    adc #0
    sta fn_stream_buf+1
    rts
_fn_stream_init:
    jsr save
    jsr buffer_reply
    lda #'s'
    sta fn_stream_frame+1
    lda #0
    sta fn_stream_frame+2
    lda #10
    sta fn_stream_len
    lda #$40
    sta fn_stream_mode
    jsr fn_stream_sio
    bne failed
    ldy #0
    lda (fn_stream_buf),y
    cmp #1
    bne invalid
    iny
    lda (fn_stream_buf),y
    cmp #120
    bne invalid
    ldy #9
@copy:
    lda (fn_stream_buf),y
    sta (ctx),y
    dey
    cpy #1
    bne @copy
    ldy #0
    lda #1
    sta (ctx),y
    lda #0
    tax
    rts
invalid:
    lda #$81
    ldx #0
    rts
failed:
    lda #$80
    ldx #0
    rts
_fn_stream_open:
    ldy #1
    bne enter
_fn_stream_read:
    ldy #2
    bne enter
_fn_stream_write:
    ldy #3
    bne enter
_fn_stream_close:
    ldy #4
    bne enter
_fn_stream_appkey:
    ldy #5
enter:
    sty op
    jsr save
    ldy #0
    lda (ctx),y
    cmp #1
    beq :+
    jmp invalid
:
    ldy #1
    lda op
    sta (ctx),y
    ldy #10
    cmp #4
    bne :+
    lda #0
    sta (ctx),y
:
    lda (ctx),y
    ldx op
    cpx #5
    beq @key
    cmp #121
    bcs invalid
    cpx #2
    beq @read
    cpx #3
    bne @small
    cmp #0
    beq invalid
@small:
    lda #11
    bne @length
@key:
    lda #75
    bne @length
@read:
    cmp #0
    beq invalid
    clc
    adc #11
@length:
    sta reply_len
    lda #3
    sta tries
retry:
    lda ctx
    sta fn_stream_buf
    lda ctx+1
    sta fn_stream_buf+1
    lda #11
    ldx op
    cpx #1
    beq @payload
    cpx #3
    bne @send
@payload:
    ldy #10
    clc
    adc (ctx),y
@send:
    sta fn_stream_len
    sta fn_stream_frame+2
    lda #'S'
    sta fn_stream_frame+1
    lda #$80
    sta fn_stream_mode
    jsr fn_stream_sio
    jne again
    jsr buffer_reply
    lda reply_len
    sta fn_stream_len
    sta fn_stream_frame+2
    lda #'T'
    sta fn_stream_frame+1
    lda #$40
    sta fn_stream_mode
    jsr fn_stream_sio
    jne again
    ; Verify token and sequence before consuming the result or advancing.
    ldy #3
    ldx #2
@verify:
    lda (fn_stream_buf),y
    pha
    tya
    pha
    txa
    tay
    pla
    sta reply_len ; scratch: receive completed
    pla
    cmp (ctx),y
    jne again
    ldy reply_len
    iny
    inx
    cpx #10
    bne @verify
    ldy #0
    lda (fn_stream_buf),y
    cmp #9
    jcs again
    beq :+
    ; A nonzero status may not claim transferred bytes.
    cmp #0
    beq :+
    ldy #2
    lda (fn_stream_buf),y
    jne again
:
    ldy #2
    lda (fn_stream_buf),y
    ldx op
    cpx #5
    bne :+
    cmp #65
    jcs again
    bcc checked
:
    ldy #10
    cmp (ctx),y
    bcc checked
    beq checked
    jcs again
checked:
    ; Replies to errors have count zero; successful read is bounded by request.
    ldy #6
@inc:
    lda (ctx),y
    clc
    adc #1
    sta (ctx),y
    bne @done
    iny
    cpy #10
    bne @inc
    ; Sequence exhaustion: explicit reinitialization required, no silent wrap.
    ldy #0
    lda #0
    sta (ctx),y
@done:
    ldy #0
    lda (fn_stream_buf),y
    ldx #0
    rts
again:
    dec tries
    beq fail_end
    ; Reply length was scratch during verification; recompute on retries.
    lda #11
    ldx op
    cpx #5
    bne :+
    lda #75
:
    cpx #2
    bne :+
    ldy #10
    lda (ctx),y
    clc
    adc #11
:
    sta reply_len
    jmp retry
fail_end:
    jmp failed
