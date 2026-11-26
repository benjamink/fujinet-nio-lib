/* Link-only growth fixture: never part of an ordinary library build. */
    .text
    .even
    .globl nl_layout_padding
nl_layout_padding:
    .space NL_LAYOUT_PAD_BYTES, 0
