; Return the C mock's status in A while deliberately setting Z, as the real
; Atari parser does. The cc65 ABI does not promise condition flags on return.
.export _fn_transport_exchange, _fn_parse_read_response
.import _test_transport_exchange, _test_parse_read_response
.code
_fn_transport_exchange:
    jsr _test_transport_exchange
    ldx #0
    rts
_fn_parse_read_response:
    jsr _test_parse_read_response
    ldx #0
    rts
