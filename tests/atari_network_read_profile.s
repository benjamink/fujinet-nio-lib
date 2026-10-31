        .export _profile_network_read, profile_start, profile_done
        .import _fn_read
        .segment "CODE"
_profile_network_read:
profile_start:
        jsr _fn_read
profile_done:
        rts
