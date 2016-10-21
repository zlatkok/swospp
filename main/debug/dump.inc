%ifndef C_EXTERN
  %define C_EXTERN  extern
%endif

%ifndef CALLMACROS_DEFINED
%define CALLMACROS_DEFINED

  %macro C_call 1-*
    %rep (%0-1)
      %rotate -1
      push dword %1
    %endrep
    %rotate -1
    call %1
    add esp, (%0-1)*4
  %endm

  %macro M$_call 1-*
    %rep (%0-1)
      %rotate -1
      push dword %1
    %endrep
    %rotate -1
    call %1
  %endm

  %macro PAS_call 1-*
    %rep (%0-1)
      %rotate 1
      push dword %1
    %endrep
    %rotate 1
    call %1
  %endm
%endif

%ifndef DUMP_H
%define DUMP_H

; enum flags_bits
  ALIGN_LEFT_BIT    equ 0
  ALIGN_RIGHT_BIT   equ 1
  ALIGN_CENTERX_BIT equ 2
  ALIGN_UP_BIT      equ 3
  ALIGN_DOWN_BIT    equ 4
  ALIGN_CENTERY_BIT equ 5
  HEX_BIT           equ 6
  OCTAL_BIT         equ 7
  BIN_BIT           equ 8
  UNSIGNED_BIT      equ 9
  SIGN_PREFIX_BIT   equ 10
  BIG_FONT_BIT      equ 11


%define NO_FLAGS 0
%define NO_ALIGNMENT NO_FLAGS
%define ALIGN_LEFT 1 << ALIGN_LEFT_BIT
%define ALIGN_RIGHT 1 << ALIGN_RIGHT_BIT
%define ALIGN_CENTERX 1 << ALIGN_CENTERX_BIT
%define ALIGN_UP 1 << ALIGN_UP_BIT
%define ALIGN_DOWN 1 << ALIGN_DOWN_BIT
%define ALIGN_CENTERY 1 << ALIGN_CENTERY_BIT
%define FL_HEX 1 << HEX_BIT
%define FL_OCTAL 1 << OCTAL_BIT
%define FL_BIN 1 << BIN_BIT
%define FL_UNSIGNED 1 << UNSIGNED_BIT
%define FL_SIGN_PREFIX 1 << SIGN_PREFIX_BIT
%define FL_BIG_FONT 1 << BIG_FONT_BIT
%define ALIGN_CENTER (ALIGN_CENTERX | ALIGN_CENTERY)
%define ALIGN_UPRIGHT (ALIGN_RIGHT | ALIGN_UP)
%define ALIGN_UPLEFT (ALIGN_LEFT | ALIGN_UP)
%define ALIGN_DOWNRIGHT (ALIGN_DOWN | ALIGN_RIGHT)
%define ALIGN_DOWNLEFT (ALIGN_DOWN | ALIGN_LEFT)


%endif