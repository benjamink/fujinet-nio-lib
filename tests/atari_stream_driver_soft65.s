.export _test_func, _test_result
.exportzp fn_stream_buf
.export fn_stream_frame, fn_stream_len, fn_stream_mode
.import fn_stream_wait_serial
.zeropage
fn_stream_buf: .res 2
.bss
fn_stream_frame: .res 4
fn_stream_len: .res 1
fn_stream_mode: .res 1
_test_result: .res 5
.code
_test_func:
    lda #$a5
    sta _test_result
    sta _test_result+1
    sta _test_result+2
    sta _test_result+3
    sta _test_result+4
    lda #$37
    sta $d201
    lda #$59
    sta $d203
    lda #$c0
    sta $d40e
    ; POKEY IRQST bits are active LOW. Zero must return immediately,
    ; even when RTCLOK does not tick (this harness has no VBI).
    lda #0
    sta $d20e
    ldy #$76
    lda #$10
    jsr fn_stream_wait_serial
    sty _test_result+1
    lda #$08
    jsr fn_stream_wait_serial
    lda #0
    sta _test_result
    lda $d201
    sta _test_result+2
    lda $d203
    sta _test_result+3
    lda $d40e
    sta _test_result+4
    rts
