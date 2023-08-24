[list -]
%include "swos.inc"
%include "dump.inc"
%include "dumpmac.inc"
[list +]

section .data

; variables list to dump

global dump_vars
dump_vars:

    db 'w'
    set_flags(ALIGN_UPLEFT)
    dd breakCameraMode
    db '.'
    declareStr "break Camera Mode"

    ;db 'w'
    ;set_flags(ALIGN_UPLEFT)
    ;dd subsBlockDirections
    ;db '.'
    ;declareStr "block directions"

    ;db 'w'
    ;set_flags(ALIGN_UPLEFT)
    ;dd eventTimer
    ;db '.'
    ;declareStr "event timer"

; this list must be terminated with zero byte
    db 0