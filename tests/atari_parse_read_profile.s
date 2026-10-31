        .export _profile_read, profile_start, profile_done
        .import _fn_parse_read_response
        .segment "CODE"
_profile_read:
profile_start:
        jsr _fn_parse_read_response
profile_done:
        rts
