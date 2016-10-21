[list -]
%include "swos.inc"
%include "dump.inc"
[list +]

section .data

; variables list to dump

global dump_vars
dump_vars:

    ;db 'w'
    ;set_flags(ALIGN_UPLEFT)
    ;dd statsTimeout
    ;db '.'
    ;declareStr "stats timeout"

    ;db 'w'
    ;set_flags(ALIGN_UPLEFT)
    ;dd gameState
    ;db '.'
    ;declareStr "game state"
    ;
    ;db 'w'
    ;set_flags(ALIGN_UPLEFT)
    ;dd eventTimer
    ;db '.'
    ;declareStr "event timer"

; this list must be terminated with zero byte
    db 0