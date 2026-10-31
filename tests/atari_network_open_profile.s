        .export _profile_open_exchange, profile_start, profile_done
        .import _fn_open_exchange
        .segment "CODE"
_profile_open_exchange:
profile_start:
        jsr _fn_open_exchange
profile_done:
        rts
