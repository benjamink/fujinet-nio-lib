; Internal cc65 fn_open_exchange(handle*, request_length). Compact backend is
; non-reentrant; the public Open entry validates the pointer before this call.
.macpack longbranch
.export _fn_open_exchange
.import __fn_req_buf, __fn_resp_buf, __fn_sessions, __fn_transport_ctx, __fn_parse_ctx
.import _fn_find_free_slot, _fn_transport_exchange, _fn_parse_open_response
.import incsp2
.importzp c_sp, ptr1
.bss
out_ptr: .res 2
session_ptr: .res 2
request_len: .res 2
.code
_fn_open_exchange:
    sta request_len
    stx request_len+1
    ldy #0
    lda (c_sp),y
    sta out_ptr
    sta ptr1
    iny
    lda (c_sp),y
    sta out_ptr+1
    sta ptr1+1
    lda #0
    dey
    sta (ptr1),y
    iny
    sta (ptr1),y
    jsr _fn_find_free_slot
    cmp #$ff
    bne @slot
    lda #$12                    ; FN_ERR_NO_HANDLES
    jmp done
@slot:
    cmp #0
    beq @slot0
    lda #<(__fn_sessions+14)
    ldx #>(__fn_sessions+14)
    jmp @store
@slot0:
    lda #<__fn_sessions
    ldx #>__fn_sessions
@store:
    sta session_ptr
    stx session_ptr+1
    lda request_len
    sta __fn_transport_ctx+4
    lda request_len+1
    sta __fn_transport_ctx+5
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
    jsr _fn_transport_exchange
    bne done
    lda #<__fn_resp_buf
    sta __fn_parse_ctx
    lda #>__fn_resp_buf
    sta __fn_parse_ctx+1
    lda __fn_transport_ctx+8
    sta __fn_parse_ctx+4
    lda __fn_transport_ctx+9
    sta __fn_parse_ctx+5
    jsr _fn_parse_open_response
    bne done
    lda out_ptr
    sta ptr1
    lda out_ptr+1
    sta ptr1+1
    ldy #0
    lda __fn_parse_ctx+12
    sta (ptr1),y
    iny
    lda __fn_parse_ctx+13
    sta (ptr1),y
    lda session_ptr
    sta ptr1
    lda session_ptr+1
    sta ptr1+1
    ldy #0
    lda #1
    sta (ptr1),y               ; active
    iny
    lda __fn_parse_ctx+26
    sta (ptr1),y               ; proto_flags
    iny
    lda __fn_parse_ctx+25
    and #2
    beq :+
    lda #1
:   sta (ptr1),y               ; needs_body
    ldy #4
    lda __fn_parse_ctx+12
    sta (ptr1),y
    iny
    lda __fn_parse_ctx+13
    sta (ptr1),y
    lda #0
    iny
@clear:
    sta (ptr1),y               ; read and write offsets
    iny
    cpy #14
    bne @clear
    lda #0
done:
    ldx #0
    jmp incsp2
