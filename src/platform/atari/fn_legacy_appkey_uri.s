; Atari implementation of the private legacy AppKey URI builder.
; cc65 passes capacity in A/X, then uri and key on the software stack.
; ptr1 is cc65 scratch; no persistent buffer or additional parameters.
        .export __fn_legacy_appkey_build_uri
        .import __fn_legacy_appkey_creator_id, __fn_legacy_appkey_app_id
        .import incsp3
        .importzp c_sp, ptr1

        .segment "RODATA"
prefix: .byte "persist:///FujiNet/"
suffix: .byte ".key", 0
hex:    .byte "0123456789abcdef"

        .segment "CODE"
__fn_legacy_appkey_build_uri:
        cpx #0
        bne valid
        cmp #32                 ; 31 bytes plus terminator
        bcs valid
        lda #0
        tax
        jmp incsp3
valid:
        ldy #0
        lda (c_sp),y
        sta ptr1
        iny
        lda (c_sp),y
        sta ptr1+1
        ldy #18
copy_prefix:
        lda prefix,y
        sta (ptr1),y
        dey
        bpl copy_prefix
        ldy #19
        lda __fn_legacy_appkey_creator_id+1
        jsr hexbyte
        lda __fn_legacy_appkey_creator_id
        jsr hexbyte
        lda __fn_legacy_appkey_app_id
        jsr hexbyte
        ldy #2
        lda (c_sp),y
        ldy #25
        jsr hexbyte
        ldx #0
copy_suffix:
        lda suffix,x
        sta (ptr1),y
        iny
        inx
        cpx #5
        bne copy_suffix
        lda #31
        ldx #0
        jmp incsp3
hexbyte:
        pha
        lsr a
        lsr a
        lsr a
        lsr a
        tax
        lda hex,x
        sta (ptr1),y
        iny
        pla
        and #15
        tax
        lda hex,x
        sta (ptr1),y
        iny
        rts
