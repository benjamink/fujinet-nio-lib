; Network Read response parser. Private no-argument ABI: all inputs/outputs are
; in _fn_parse_ctx. ptr1/ptr2 are cc65 scratch; this path is non-reentrant, as
; is the surrounding transport. The original C remains for other targets.
        .export _fn_parse_read_response
        .import _fn_parse_response_header, __fn_parse_ctx
        .importzp ptr1, ptr2
        .bss
actual: .res 2
remaining: .res 2
        .code
_fn_parse_read_response:
        jsr _fn_parse_response_header
        cmp #0
        beq :+
        ldx #0
        rts
:       lda __fn_parse_ctx+24     ; service status
        beq :+
        ldx #0
        rts
:
        lda __fn_parse_ctx+11
        bne payload_ok
        lda __fn_parse_ctx+10
        cmp #12
        bcs payload_ok
        jmp invalid
payload_ok:
        clc
        lda __fn_parse_ctx
        adc __fn_parse_ctx+8
        sta ptr1
        lda __fn_parse_ctx+1
        adc __fn_parse_ctx+9
        sta ptr1+1
        ldy #1
        lda (ptr1),y
        sta __fn_parse_ctx+25
        and #8                   ; FN_READ_AVAILABLE
        bne has_available
        lda #0
        sta __fn_parse_ctx+27
        sta __fn_parse_ctx+28
        beq metadata
has_available:
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+27
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+28
metadata:
        ldy #4
        lda (ptr1),y
        sta __fn_parse_ctx+12
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+13
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+14
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+15
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+16
        iny
        lda (ptr1),y
        sta __fn_parse_ctx+17
        iny
        lda (ptr1),y
        sta actual
        iny
        lda (ptr1),y
        sta actual+1
        ; actual <= response payload length minus 12
        sec
        lda __fn_parse_ctx+10
        sbc #12
        sta remaining
        lda __fn_parse_ctx+11
        sbc #0
        sta remaining+1
        lda remaining
        cmp actual
        lda remaining+1
        sbc actual+1
        bcc invalid
        ; actual <= caller's capacity
        lda __fn_parse_ctx+6
        cmp actual
        lda __fn_parse_ctx+7
        sbc actual+1
        bcc invalid
        lda __fn_parse_ctx+2
        sta ptr2
        lda __fn_parse_ctx+3
        sta ptr2+1
        ora ptr2
        beq success              ; null output: parse metadata only
        clc
        lda ptr1
        adc #12
        sta ptr1
        bcc :+
        inc ptr1+1
:       ldy #0
        ldx actual+1
        beq copy_tail
copy_page:
        lda (ptr1),y
        sta (ptr2),y
        iny
        bne copy_page
        inc ptr1+1
        inc ptr2+1
        dex
        bne copy_page
copy_tail:
        ldx actual
        beq success
copy_byte:
        lda (ptr1),y
        sta (ptr2),y
        iny
        dex
        bne copy_byte
success:
        lda actual
        sta __fn_parse_ctx+10
        lda actual+1
        sta __fn_parse_ctx+11
        lda #0
done:   ldx #0
        rts
invalid:
        lda #2                   ; FN_ERR_INVALID
        bne done
