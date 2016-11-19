; misc.inc - various macros and some SWOS symbols
;

; SWOS locations
EndProgram2 equ 0x9923  ; code


; Macros and defines

%define SEEK_SET 0
%define SEEK_CUR 1
%define SEEK_END 2

; xalign - extended align
;
; %1 - alignment request, must be numeric and a power of two
; %2 - org, zero by default
; %3 - value to pad with, default is nop (0x90)

%macro xalign 1-3 0, 0x90
    %ifnum %1
        %if %1 & (%1 - 1)
            %error "Alignment must be a power of 2"
        %else
            times (%1 - ((%2 + $ - $$) & (%1 - 1))) db %3
        %endif
    %else
        %error "First parameter must be numeric"
    %endif
%endmacro


; ripped from Icedump

;------------------------------------------------------------------------------
;macro support for @F,@@,@B
;------------------------------------------------------------------------------

%assign UNNAMED_LOCAL_LABEL 0

%define @F ..@UNNAMED_LOCAL_LABEL0

%macro define@F 1
%define @F ..@UNNAMED_LOCAL_LABEL%{1}
%endmacro

%define @@ @@_ UNNAMED_LOCAL_LABEL

%macro @@_ 1
..@UNNAMED_LOCAL_LABEL%{1}:
%define @B ..@UNNAMED_LOCAL_LABEL%{1}
%assign UNNAMED_LOCAL_LABEL UNNAMED_LOCAL_LABEL+1
define@F UNNAMED_LOCAL_LABEL
%endmacro