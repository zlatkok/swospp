; macros for generating patch data

; %1 - address, %2 - actual data
%macro PatchByte 2.nolist
    db 1
    dd %1
    db %2
%endmacro

%macro PatchWord 2.nolist
    db 2
    dd %1
    dw %2
%endmacro

%macro PatchDword 2.nolist
    db 4
    dd %1
    dd %2
%endmacro

; %1 - address, %2 - actual data or code
%macro PatchData 2.nolist
    db %%size
    dd %1
%%start:
    %2
%%size equ $ - %%start
%endmacro

; Macros for declaring multi-line code and data
%macro StartRecord 1.nolist
    %ifctx record
        %error "Patch records can't be nested"
    %endif
    %push record
    db %$size
    dd %1
    %$start:
%endmacro

%macro EndRecord 0.nolist
    %ifctx record
        %$size equ $ - %$start
        %assign %$size $ - %$start
        %if %$size == 0
            %error "Patch record must not be empty"
        %endif
        %if %$size > 255
            %error "Data too big. Split into two or more records."
        %endif
        %pop
    %else
        %error "Expecting `StartRecord' before `EndRecord'"
    %endif
%endmacro

; %1 - destination address in SWOS to patch, %2 - address to write
%macro PatchOffset 2.nolist
    PatchDword %1, %2
%endmacro

; %1 - address, %2 - repeat value, %3 - data value
%macro FillRecord 3.nolist
    db 0
    dd %1
    db %2
    db %3
%endmacro

; Macro for calls and jumps from SWOS to SWOS (code)
%macro SWOSCall 2.nolist
    db 5, 0xe8
    dd %2 - %1 - 5
%endmacro

%macro SWOSJmp 2.nolist
    db 5, 0xe9
    dd %2 - %1 - 5
%endmacro