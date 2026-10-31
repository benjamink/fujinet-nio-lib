; Atari compact fn_write and its diagnostic accessor. The public cc65 ABI and
; all retry/partial-write semantics are the same as common C. Like the rest of
; the compact backend, private argument and diagnostic storage is non-reentrant.
.macpack longbranch
.export _fn_write, _fn_write_get_last_diagnostics
.import __fn_initialized, __fn_sessions, __fn_req_buf, __fn_resp_buf
.import __fn_transport_ctx, __fn_parse_ctx
.import _fn_find_session, _fn_build_write_packet, _fn_transport_exchange, _fn_parse_response_header
.import pushax, pusheax, addysp
.importzp c_sp, ptr1, sreg
.ifndef FN_WRITE_RETRY_LIMIT
FN_WRITE_RETRY_LIMIT = 500
.endif
.assert FN_WRITE_RETRY_LIMIT >= 0 .and FN_WRITE_RETRY_LIMIT <= 65535, error, "invalid write retry limit"
.macro inc16 word
    inc word
    bne :+
    inc word+1
:
.endmacro
.bss
diag: .res 12                   ; fn_write_diagnostics_t
written_ptr: .res 2
length: .res 2
data_ptr: .res 2
position: .res 4
handle: .res 2
session_ptr: .res 2
total: .res 2
remaining: .res 2
accepted: .res 2
retries: .res 2
error_code: .res 1
.code
_fn_write_get_last_diagnostics:
    sta ptr1
    stx ptr1+1
    ora ptr1+1
    beq @done
    ldy #11
@copy:
    lda diag,y
    sta (ptr1),y
    dey
    bpl @copy
@done:
    rts
_fn_write:
    sta written_ptr
    stx written_ptr+1
    lda #0
    ldy #11
@clear:
    sta diag,y
    dey
    bpl @clear
    lda written_ptr
    sta ptr1
    lda written_ptr+1
    sta ptr1+1
    ora ptr1
    beq @initialized
    lda #0
    tay
    sta (ptr1),y
    iny
    sta (ptr1),y
@initialized:
    lda __fn_initialized
    jeq invalid
    ldy #9
@args:
    lda (c_sp),y
    sta length,y
    dey
    bpl @args
    lda handle
    ora handle+1
    jeq invalid
    lda length
    ora length+1
    beq @find
    lda data_ptr
    ora data_ptr+1
    jeq invalid
@find:
    lda handle
    ldx handle+1
    jsr _fn_find_session
    cmp #$ff
    bne @slot
    lda #1                      ; FN_ERR_NOT_FOUND
    jmp done
@slot:
    cmp #0
    beq @slot0
    lda #<(__fn_sessions+14)
    ldx #>(__fn_sessions+14)
    jmp @store_session
@slot0:
    lda #<__fn_sessions
    ldx #>__fn_sessions
@store_session:
    sta session_ptr
    sta ptr1
    stx session_ptr+1
    stx ptr1+1
    ldy #6                      ; write_offset
    ldx #0
@compare:
    lda (ptr1),y
    cmp position,x
    jne invalid
    iny
    inx
    cpx #4
    bne @compare
    lda #0
    sta total
    sta total+1
    jsr reset_retries
next:
    sec
    lda length
    sbc total
    sta remaining
    lda length+1
    sbc total+1
    sta remaining+1
    ; Maximum Write data = 512-byte packet less its 15-byte header.
    cmp #1
    bcc build
    bne limit
    lda remaining
    cmp #$f2
    bcc build
limit:
    lda #$f1
    sta remaining
    lda #1
    sta remaining+1
build:
    lda #<__fn_req_buf
    ldx #>__fn_req_buf
    jsr pushax
    lda handle
    ldx handle+1
    jsr pushax
    lda session_ptr
    sta ptr1
    lda session_ptr+1
    sta ptr1+1
    ldy #8
    lda (ptr1),y
    sta sreg
    iny
    lda (ptr1),y
    sta sreg+1
    ldy #7
    lda (ptr1),y
    tax
    dey
    lda (ptr1),y
    jsr pusheax
    lda data_ptr
    ora data_ptr+1
    beq @null_data
    clc
    lda data_ptr
    adc total
    pha
    lda data_ptr+1
    adc total+1
    tax
    pla
    jmp @push_data
@null_data:
    lda #0
    tax
@push_data:
    jsr pushax
    lda remaining
    ldx remaining+1
    jsr _fn_build_write_packet
    sta __fn_transport_ctx+4
    stx __fn_transport_ctx+5
    ora __fn_transport_ctx+5
    jeq invalid
    lda #<__fn_req_buf
    sta __fn_transport_ctx
    lda #>__fn_req_buf
    sta __fn_transport_ctx+1
    lda #<__fn_resp_buf
    sta __fn_transport_ctx+2
    lda #>__fn_resp_buf
    sta __fn_transport_ctx+3
    lda #0
    sta __fn_transport_ctx+6
    lda #2
    sta __fn_transport_ctx+7
    inc16 diag
    jsr _fn_transport_exchange
    cmp #3                      ; FN_ERR_BUSY
    jeq transport_busy
    cmp #4                      ; FN_ERR_NOT_READY
    jeq transport_busy
    cmp #0
    jne done
    lda __fn_transport_ctx+8
    sta diag+8
    lda __fn_transport_ctx+9
    sta diag+9
    bne @header
    lda diag+8
    cmp #2
    bcc @parse
@header:
    lda __fn_resp_buf
    sta diag+10
    lda __fn_resp_buf+1
    sta diag+11
@parse:
    lda #<__fn_resp_buf
    sta __fn_parse_ctx
    lda #>__fn_resp_buf
    sta __fn_parse_ctx+1
    lda __fn_transport_ctx+8
    sta __fn_parse_ctx+4
    lda __fn_transport_ctx+9
    sta __fn_parse_ctx+5
    jsr _fn_parse_response_header
    cmp #0
    jne done
    lda __fn_parse_ctx+24
    cmp #3
    jeq service_busy
    cmp #4
    jeq service_busy
    cmp #0
    jne done
    lda #0
    sta accepted
    sta accepted+1
    lda __fn_parse_ctx+11
    bne @get_accepted
    lda __fn_parse_ctx+10
    cmp #12
    bcc @check_accepted
@get_accepted:
    clc
    lda __fn_parse_ctx+8
    adc #<(__fn_resp_buf+10)
    sta ptr1
    lda __fn_parse_ctx+9
    adc #>(__fn_resp_buf+10)
    sta ptr1+1
    ldy #0
    lda (ptr1),y
    sta accepted
    iny
    lda (ptr1),y
    sta accepted+1
@check_accepted:
    lda remaining
    cmp accepted
    lda remaining+1
    sbc accepted+1
    bcs :+
    lda #5                      ; FN_ERR_IO
    jmp done
:
    lda length
    ora length+1
    beq success
    lda accepted
    ora accepted+1
    bne @progress
    inc16 diag+6
    lda #3
    jmp retry
@progress:
    lda session_ptr
    sta ptr1
    lda session_ptr+1
    sta ptr1+1
    ldy #6
    clc
    lda (ptr1),y
    adc accepted
    sta (ptr1),y
    iny
    lda (ptr1),y
    adc accepted+1
    sta (ptr1),y
    iny
    lda (ptr1),y
    adc #0
    sta (ptr1),y
    iny
    lda (ptr1),y
    adc #0
    sta (ptr1),y
    clc
    lda total
    adc accepted
    sta total
    lda total+1
    adc accepted+1
    sta total+1
    jsr reset_retries
    lda total
    cmp length
    bne next_jump
    lda total+1
    cmp length+1
    bne next_jump
success:
    lda written_ptr
    sta ptr1
    lda written_ptr+1
    sta ptr1+1
    ora ptr1
    beq ok
    ldy #0
    lda total
    sta (ptr1),y
    iny
    lda total+1
    sta (ptr1),y
ok:
    lda #0
    beq done
next_jump:
    jmp next
transport_busy:
    sta error_code
    inc16 diag+2
    lda error_code
    jmp retry
service_busy:
    sta error_code
    inc16 diag+4
    lda error_code
retry:
    sta error_code
    lda retries
    ora retries+1
    bne :+
    lda error_code
    jmp done
:
    lda retries
    bne :+
    dec retries+1
:
    dec retries
    jmp next
reset_retries:
    lda #<FN_WRITE_RETRY_LIMIT
    sta retries
    lda #>FN_WRITE_RETRY_LIMIT
    sta retries+1
    rts
invalid:
    lda #2                      ; FN_ERR_INVALID
done:
    ldx #0
    ldy #10
    jmp addysp
