.macpack longbranch
        .export _fn_read

        .import __fn_initialized, __fn_sessions
        .import invalid_with_stack_fix
        .import _fn_bbc_osword78
        .import _fn_bbc_rw_setup_read, _fn_bbc_osbget, addysp, return0
        .importzp c_sp, ptr1, ptr2, ptr3, ptr4, tmp1, tmp2, tmp3, tmp4

        .include "fn_protocol.inc"

FN_READ_STACK_BYTES               := 12

STACK_BYTES_READ_PTR              := 0
STACK_MAX_LEN                     := 2
STACK_BUF_PTR                     := 4
STACK_OFFSET                      := 6

.bss
read_block: .res 11
.code

; uint8_t fn_read(fn_handle_t handle,
;                 uint32_t offset,
;                 uint8_t *buf,
;                 uint16_t max_len,
;                 uint16_t *bytes_read,
;                 uint8_t *flags)

_fn_read:
        ldy     __fn_initialized
        jne     @check_args
        ldy     #FN_READ_STACK_BYTES
        jmp     invalid_with_stack_fix

@check_args:
        jsr     _fn_bbc_rw_setup_read
        jcc     @have_session
        cmp     #FN_ERR_NOT_FOUND
        jne     @invalid

@not_found:
        jsr     fix_stack
        ldx     #$00
        lda     #FN_ERR_NOT_FOUND
        rts

@invalid:
        jsr     fix_stack
        ldx     #$00
        lda     #FN_ERR_INVALID
        rts

@have_session:
        ldy     #STACK_BYTES_READ_PTR
        lda     (c_sp),y
        sta     ptr1
        iny
        lda     (c_sp),y
        sta     ptr1+1
        lda     ptr1
        ora     ptr1+1
        jeq     @invalid

        ldy     #STACK_MAX_LEN
        lda     (c_sp),y
        sta     ptr4
        iny
        lda     (c_sp),y
        sta     ptr4+1

        ldy     #STACK_BUF_PTR
        lda     (c_sp),y
        sta     ptr2
        iny
        lda     (c_sp),y
        sta     ptr2+1
        lda     ptr2
        ora     ptr2+1
        jeq     @invalid

        ldy     #STACK_OFFSET
        lda     (c_sp),y
        ldy     tmp4
        cmp     __fn_sessions + FN_SESSION_READ_OFFSET,y
        jne     @invalid
        ldy     #STACK_OFFSET + 1
        lda     (c_sp),y
        ldy     tmp4
        cmp     __fn_sessions + FN_SESSION_READ_OFFSET + 1,y
        jne     @invalid
        ldy     #STACK_OFFSET + 2
        lda     (c_sp),y
        ldy     tmp4
        cmp     __fn_sessions + FN_SESSION_READ_OFFSET + 2,y
        jne     @invalid
        ldy     #STACK_OFFSET + 3
        lda     (c_sp),y
        ldy     tmp4
        cmp     __fn_sessions + FN_SESSION_READ_OFFSET + 3,y
        jne     @invalid

        lda     #$00
        sta     tmp1
        sta     tmp2
        ldy     #0
        sta     (ptr1),y
        iny
        sta     (ptr1),y
        lda     ptr3
        ora     ptr3+1
        beq     @loop
        lda     #0
        ldy     #0
        sta     (ptr3),y

@loop:
        lda     tmp1
        cmp     ptr4
        lda     tmp2
        sbc     ptr4+1
        jcs     @done_full

        jsr     read_chunk
        cmp     #1              ; old ROM: unsupported reason, retain OSBGET
        jeq     @legacy_loop
        cmp     #0
        jne     @io_error
        lda     read_block+7
        and     #2
        jne     @not_ready
        lda     read_block+7
        and     #1
        jne     @eof
        lda     read_block+8
        jeq     @io_error       ; never spin on a successful zero-progress chunk
        clc
        adc     tmp1
        sta     tmp1
        bcc     :+
        inc     tmp2
:       clc
        lda     ptr2
        adc     read_block+8
        sta     ptr2
        bcc     :+
        inc     ptr2+1
:       jmp     @loop
@io_error:
        jsr     fix_stack
        ldx     #0
        lda     #FN_ERR_IO
        rts

@legacy_loop:
        lda     tmp1
        cmp     ptr4
        lda     tmp2
        sbc     ptr4+1
        jcs     @done_full
        lda     tmp3
        jsr     _fn_bbc_osbget
        cpx     #FN_BBC_OSBGET_NOT_READY
        jeq     @not_ready
        cpx     #$FF
        jeq     @eof

@store_byte:
        ldy     #$00
        sta     (ptr2),y
        inc     ptr2
        jne     @count
        inc     ptr2+1
@count: inc     tmp1
        jne     @legacy_loop
        inc     tmp2
        jne     @legacy_loop

@not_ready:
        lda     tmp1
        ora     tmp2
        jne     @done_partial

        jsr     fix_stack
        ldx     #$00
        lda     #FN_ERR_NOT_READY
        rts

@eof:
        ldx     #FN_READ_EOF
        jne     @done

@done_full:
        ldx     #$00
        jeq     @done

@done_partial:
        ldx     #$00

@done:
        ldy     #$00
        lda     tmp1
        sta     (ptr1),y
        iny
        lda     tmp2
        sta     (ptr1),y

        lda     ptr3
        ora     ptr3+1
        jeq     @update_offset

        ldy     #$00
        txa
        sta     (ptr3),y

@update_offset:
        ldy     tmp4
        clc
        lda     __fn_sessions + FN_SESSION_READ_OFFSET,y
        adc     tmp1
        sta     __fn_sessions + FN_SESSION_READ_OFFSET,y
        lda     __fn_sessions + FN_SESSION_READ_OFFSET + 1,y
        adc     tmp2
        sta     __fn_sessions + FN_SESSION_READ_OFFSET + 1,y
        lda     __fn_sessions + FN_SESSION_READ_OFFSET + 2,y
        adc     #$00
        sta     __fn_sessions + FN_SESSION_READ_OFFSET + 2,y
        lda     __fn_sessions + FN_SESSION_READ_OFFSET + 3,y
        adc     #$00
        sta     __fn_sessions + FN_SESSION_READ_OFFSET + 3,y
        jsr     fix_stack
        jmp     return0

fix_stack:
        ldy     #FN_READ_STACK_BYTES
        jmp     addysp

; All state belongs to this synchronous fn_read call. OSWORD wrapper uses ptr1,
; so preserve the caller's bytes_read pointer across it.
read_chunk:
        lda #7
        sta read_block
        lda ptr2
        sta read_block+2
        lda ptr2+1
        sta read_block+3
        sec
        lda ptr4
        sbc tmp1
        sta read_block+4
        lda ptr4+1
        sbc tmp2
        beq :+
        lda #255
        sta read_block+4
:       lda tmp3
        sta read_block+6
        lda ptr1+1
        pha
        lda ptr1
        pha
        lda #<read_block
        ldx #>read_block
        jsr _fn_bbc_osword78
        tay
        pla
        sta ptr1
        pla
        sta ptr1+1
        tya
        rts
