; Compact Atari fn_read. The cc65 stack and global packet/session context are
; synchronous and non-reentrant, just like the C implementation. Saves the
; public arguments once rather than repeatedly unpacking cc65's C stack.
.macpack longbranch
.export _fn_read, _fn_read_available
.import __fn_initialized, __fn_sessions, __fn_req_buf, __fn_resp_buf
.import __fn_transport_ctx, __fn_parse_ctx
.import _fn_find_session, _fn_build_read_packet, _fn_transport_exchange, _fn_parse_read_response
.import pushax, pusheax, addysp
.importzp c_sp, ptr1, sreg
.bss
flags_ptr: .res 2
bytes_ptr: .res 2
count: .res 2
buffer_ptr: .res 2
position: .res 4
handle: .res 2
session_ptr: .res 2
available: .res 2
.code
_fn_read_available:
    lda available
    ldx available+1
    rts
_fn_read:
    sta flags_ptr
    stx flags_ptr+1
    lda __fn_initialized
    jeq invalid
    ; The five stack arguments occupy 12 consecutive bytes, in this order.
    ldy #11
@args:
    lda (c_sp),y
    sta bytes_ptr,y
    dey
    bpl @args
    lda bytes_ptr
    ora bytes_ptr+1
    jeq invalid
    lda buffer_ptr
    ora buffer_ptr+1
    jeq invalid
    lda handle
    ora handle+1
    jeq invalid
    lda bytes_ptr
    sta ptr1
    lda bytes_ptr+1
    sta ptr1+1
    lda #0
    ldy #0
    sta (ptr1),y
    iny
    sta (ptr1),y
    lda flags_ptr
    sta ptr1
    lda flags_ptr+1
    sta ptr1+1
    ora ptr1
    beq @find
    lda #0
    tay
    sta (ptr1),y
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
    ldy #1                      ; proto_flags
    lda (ptr1),y
    and #1                      ; sequential read
    beq @cap
    ldy #10                     ; read_offset
    ldx #0
@compare:
    lda (ptr1),y
    cmp position,x
    jne invalid
    iny
    inx
    cpx #4
    bne @compare
@cap:
    lda count+1
    cmp #1
    bcc @build
    bne @limit
    lda count
    cmp #$eb                    ; maximum response payload is 490 ($01ea)
    bcc @build
@limit:
    lda #$ea
    sta count
    lda #1
    sta count+1
@build:
    lda #<__fn_req_buf
    ldx #>__fn_req_buf
    jsr pushax
    lda handle
    ldx handle+1
    jsr pushax
    lda position+2
    sta sreg
    lda position+3
    sta sreg+1
    lda position
    ldx position+1
    jsr pusheax
    lda count
    ldx count+1
    jsr _fn_build_read_packet
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
    jsr _fn_transport_exchange
    jne done
    lda #<__fn_resp_buf
    sta __fn_parse_ctx
    lda #>__fn_resp_buf
    sta __fn_parse_ctx+1
    lda buffer_ptr
    sta __fn_parse_ctx+2
    lda buffer_ptr+1
    sta __fn_parse_ctx+3
    lda __fn_transport_ctx+8
    sta __fn_parse_ctx+4
    lda __fn_transport_ctx+9
    sta __fn_parse_ctx+5
    lda count
    sta __fn_parse_ctx+6
    lda count+1
    sta __fn_parse_ctx+7
    jsr _fn_parse_read_response
    jne done
    lda __fn_parse_ctx+27
    sta available
    lda __fn_parse_ctx+28
    sta available+1
    ; Match C's output order: flags first, then bytes_read. This matters
    ; when callers intentionally alias the two destinations.
    lda flags_ptr
    sta ptr1
    lda flags_ptr+1
    sta ptr1+1
    ora ptr1
    beq @store_bytes
    ldy #0
    lda __fn_parse_ctx+25
    sta (ptr1),y
@store_bytes:
    lda bytes_ptr
    sta ptr1
    lda bytes_ptr+1
    sta ptr1+1
    ldy #0
    lda __fn_parse_ctx+10
    sta (ptr1),y
    iny
    lda __fn_parse_ctx+11
    sta (ptr1),y
@advance:
    lda session_ptr
    sta ptr1
    lda session_ptr+1
    sta ptr1+1
    ldy #1
    lda (ptr1),y
    and #1
    beq ok
    ldy #10
    clc
    lda (ptr1),y
    adc __fn_parse_ctx+10
    sta (ptr1),y
    iny
    lda (ptr1),y
    adc __fn_parse_ctx+11
    sta (ptr1),y
    iny
    lda (ptr1),y
    adc #0
    sta (ptr1),y
    iny
    lda (ptr1),y
    adc #0
    sta (ptr1),y
ok:
    lda #0
    beq done
invalid:
    lda #2                      ; FN_ERR_INVALID
done:
    ldx #0
    ldy #12                     ; 12 bytes of cc65 stack arguments
    jmp addysp
