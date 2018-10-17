;
; Menu structures and macros for declaring menus the safe way
;

struc Menu                  ; (size = 22)
    .onInit:        resd 1  ; 0
    .afterDraw:     resd 1  ; 4
    .onDraw:        resd 1  ; 8
    .currentEntry:  resd 1  ; 12
    .numEntries     resw 1  ; 16
    .endOfMenuPtr   resd 1  ; 18
endstruc

struc MenuEntry                 ; (size = 56)
    .alreadyDrawn:      resb 2  ;  0
    .ordinal:           resb 2  ;  2
    .isInvisible:       resb 2  ;  4
    .isDisabled:        resb 2  ;  6
    .leftEntry:         resb 1  ;  8
    .rightEntry:        resb 1  ;  9
    .upEntry:           resb 1  ; 10
    .downEntry:         resb 1  ; 11
    .directionLeft:     resb 1  ; 12
    .directionRight:    resb 1  ; 13
    .directionUp:       resb 1  ; 14
    .directionDown:     resb 1  ; 15
    .skipLeftEntry:     resb 1  ; 16
    .skipRightEntry:    resb 1  ; 17
    .skipUpEntry:       resb 1  ; 18
    .skipDownEntry:     resb 1  ; 19
    .x:                 resb 2  ; 20
    .y:                 resb 2  ; 22
    .width:             resb 2  ; 24
    .height:            resb 2  ; 26
    .entryType1:        resb 2  ; 28
        ; union
        .func1:                 ; 30
        .backAndFrameColor:
        .sprite1:       resb 4
    .entryType2:        resb 2  ; 34
    .stringFlags:       resb 2  ; 36
        ; union
        .func2:                 ; 38
        .string:
        .sprite2:
        .stringTable:
        .multiLineText:
        .number:
        .spriteCopy:    resb 4
     .onSelect:         resb 4  ; 42
     .controlsMask:     resb 2  ; 46
     .beforeDraw:       resb 4  ; 48
     .afterDraw:        resb 4  ; 52
endstruc

; StartMenu
;
; %1 - menu name (label)
; %2 - before draw function (one-time initialization on menu load)
; %3 - after draw function (called once)
; %4 - on draw function (called repeatedly in menu loop)
; %5 - default entry

%macro StartMenu 1-5.nolist   0, 0, 0, 0
    %ifctx menu
        %error "Menus can't be nested"
    %endif
    %ifctx entry
        %error "Previous menu entry not finished"
    %endif
    %push menu
    %assign previousEntryX  0   ; entry will be able to use X coordinate from previous entry
    %assign previousEntryY  0   ; same goes for X
    %assign %$isTemplate    0
    %assign %$entryIndex    0
    %define %$menuName %1
    align 4, db 'z'
    %1:
    dd %2
    dd %3
    dd %4
    dd %5
%endmacro

%macro EndMenu 0.nolist
    %ifctx entry
        %error "Previous menu entry not finished"
    %endif
    %ifnctx menu
        %error "Expecting `StartMenu' before `EndMenu'"
    %endif
    dw -999
    %pop
%endmacro

%macro chkmenu 0.nolist
    %ifnctx menu
        %error "Command valid only inside menu"
    %endif
%endmacro

%macro chkentry 0.nolist
    %ifnctx entry
        %error "Command valid only inside menu entry"
    %endif
%endmacro

%macro MenuXY 2.nolist
    chkmenu
    dw -998
    dw %1
    dw %2
%endmacro

%macro StartTemplateEntry 0.nolist
    chkmenu
    %assign %$isTemplate 1
    dw -997
%endmacro

%macro RestoreTemplateEntry 0.nolist
    chkmenu
    dw -996
%endmacro

; MarkNextEntryOrdinal
;
; %1 - symbol name
;
; Define symbol given as parameter and assign to it the value of next entry
; ordinal. Handy for marking beginnings of collective entries.

%macro MarkNextEntryOrdinal 1.nolist
    chkmenu
    %assign %1 %$entryIndex
%endmacro

; StartEntry
;
; %1 - x
; %2 - y
; %3 - width
; %4 - height
; %5 - name of this entry - it will be declared as Nasm symbol in the form of
;      menuName_entryName with the value of this entry ordinal number; handy
;      for tracking important entries
;
; Several variables are available for easier stacking of entries:
; - previousEntryX and previousEntryY, x and y coordinate of previous entry
; - previousEntryEndX and previousEntryEndY, equals to previous entry x + width and y + height
; - currentEntry equals to index of this current entry
;
%macro StartEntry 0-5.nolist   0, 0, 0, 0
    %ifctx entry
        %error "Entries can't be nested"
    %endif
    chkmenu
    %if %0 >= 5
        %xdefine %{$menuName}_%{5} %$entryIndex
    %endif
    %assign currentEntry    %$entryIndex
    ; template entries do not show up in final menu
    %if %$isTemplate == 0
        %assign %$entryIndex %$entryIndex + 1
    %endif
    %assign %$isTemplate    0
    %assign %$currentX      %1
    %assign %$currentY      %2
    %assign %$currentEndX   %1 + %3
    %assign %$currentEndY   %2 + %4
    %push entry
    dw %1
    dw %2
    dw %3
    dw %4
    %$x equ %1
    %$y equ %2
    %$width equ %3
    %$height equ %4
%endmacro

%macro EndEntry 0.nolist
    chkentry
    %pop
    chkmenu
    %assign previousEntryX      %$currentX
    %assign previousEntryY      %$currentY
    %assign previousEntryEndX   %$currentEndX
    %assign previousEntryEndY   %$currentEndY
    dw 0
%endmacro

%macro InvisibleEntry 0.nolist
    chkentry
    dw 1
%endmacro

; NextEntries
;
; Set next entries when player presses left, right, up, down.
; Default = no movement (-1)
;
; %1 - left entry
; %2 - right entry
; %3 - up entry
; %4 - down entry
;
; Entries are 0-based.

%macro NextEntries 0-4.nolist   -1, -1, -1, -1
    chkentry
    dw 2
    db %1
    db %2
    db %3
    db %4
%endmacro

%macro EntryFunc1 1.nolist
    chkentry
    dw 3
    dd %1
%endmacro

;  EntryColor
;
;  high nibble - frame color
;  low nibble  - background color

%macro EntryColor 1.nolist
    chkentry
    dw 4
    dw %1
%endmacro

%macro EntrySprite1 1.nolist
    chkentry
    dw 5
    dw %1
%endmacro

%macro EntryFunc2 1.nolist
    chkentry
    dw 7
    dd %1
%endmacro

%macro EntryString 2.nolist
    chkentry
    dw 8
    dw %1
    %ifnnum %1
        %error "First parameter must be string flags"
    %endif
    %ifstr %2
        [section .strdata]
        %%str:
            db %2, 0
        __SECT__
        dd %%str
    %else
        dd %2
    %endif
%endmacro

%macro EntrySprite2 1.nolist
    chkentry
    dw 9
    dw %1
%endmacro

; StringTable
;
; %1 - string flags
; %2 - pointer to actual table:
;      dd pointer to string index variable (word)
;      dw string index variable starting value
;      dd... pointers to strings (on select must take care of changing index)

%macro StringTable 2.nolist
    chkentry
    dw 10
    dw %1
    dd %2
%endmacro

; MultiLineText
;
; %1 - string flags:
;      bits 0-3 = text color
;      bit  4   = use large font
;      bit 13   = blinking text
;      bit 14   = alignment right
;      bit 15   = alignment left
; %2 - string list ptr, points to a byte (no. lines), followed by that many
;      zero terminated strings (each will be drawn on separate line)
;      (note - strings themselves, not pointers)

%macro MultiLineText 2.nolist
    chkentry
    %if %$width < 4 || %$height < 4
        %error "Entry must have width and height with multi-line text"
    %endif
    dw 11
    dw %1
    dd %2
%endmacro

%macro EntryNumber 2.nolist
    chkentry
    dw 12
    dw %1
    dw %2
%endmacro

%macro OnSelect 1.nolist
    chkentry
    dw 13
    dd %1
%endmacro

%macro OnSelectWithMask 2.nolist
    chkentry
    dw 14
    dw %1
    dd %2
%endmacro

%macro BeforeDraw 1.nolist
    chkentry
    dw 15
    dd %1
%endmacro

%macro AfterDraw 1.nolist
    chkentry
    dw 16
    dd %1
%endmacro

; LeftSkip
;
; %1 - menu entry that gets selected if this one is disabled or invisible
; %2 - new direction if     - // -
;      (0 = up, 1 = right, 2 = down, 3 = left)
;      Can be used to change direction in search of next entry.

%macro LeftSkip 2.nolist
    chkentry
    dw 17
    db %1
    db %2
%endmacro

%macro RightSkip 2.nolist
    chkentry
    dw 18
    db %1
    db %2
%endmacro

%macro UpSkip 2.nolist
    chkentry
    dw 19
    db %1
    db %1
%endmacro

%macro DownSkip 2.nolist
    chkentry
    dw 20
    db %1
    db %2
%endmacro

%macro SpriteCopy 1.nolist
    chkentry
    dw 21
    dd %1
%endmacro