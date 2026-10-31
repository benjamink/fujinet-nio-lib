        .export _profile_uri, profile_start, profile_done
        .import __fn_legacy_appkey_build_uri
        .segment "CODE"
_profile_uri:
profile_start:
        jsr __fn_legacy_appkey_build_uri
profile_done:
        rts
