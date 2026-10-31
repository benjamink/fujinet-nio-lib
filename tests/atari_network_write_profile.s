        .export _profile_network_write, profile_start, profile_done
        .import _fn_write
        .segment "CODE"
_profile_network_write:
profile_start:
        jsr _fn_write
profile_done:
        rts
