; Implement SWOS 96/97 specific entry in main menu

[list -]
%include "swos.inc"
%include "swsmenu.inc"
[list +]

section .data
; special entry for SWOS 96/97
swosAnniversaryEntry:
    dw 92    ; x
    dw 165   ; y
    dw 120   ; width
    dw 16    ; height
    dw 2     ; type 2 - positions
    db -1    ; left - nothing
    db -1    ; right - nothing
    db 12    ; up - "LOAD OLD COMPETITION"
    db -1    ; down - nothing
    dw 4     ; type 4 - background color
    dw 13
    dw 8     ; type 8 - string
    dw 16    ; string flags
    declareStr "16/17 CREDITS"
    dw 13    ; type 13 - on select function
    dd ShowSWOSAnniversaryMenu
    dw 0     ; type 0 - end of menu entry


swosAnniversaryEntrySize equ $ - swosAnniversaryEntry
newMainMenuSize equ SWOS_MAIN_MENU_SIZE + swosAnniversaryEntrySize

section .bss
; reserve space for new SWOS main menu (size of the menu + one entry)
global newMainMenu
declareStatic newMainMenu, newMainMenuSize


section .text
; SetupSWOSAnniversaryMenu
;
; Copy SWOS main menu to new location and edit it, in order to show new SWOS 16/17 menu entry.
; Note that all patches for SWOS++ entry are done by now.
;
global SetupSWOSAnniversaryMenu
SetupSWOSAnniversaryMenu:
        mov  eax, newMainMenu

        mov  ecx, (SWOS_MAIN_MENU_SIZE - 8) / 4
        mov  esi, SWOS_MainMenu
        mov  edi, eax
        rep  movsd

        mov  ecx, swosAnniversaryEntrySize
        mov  esi, swosAnniversaryEntry
        rep  movsb

        mov  dword [edi], 0x0000fc1a
        mov  dword [edi + 4], 0xfc190000

        ; fix down entry for "LOAD OLD COMPETITION"
        mov  byte [eax + 0x1c3], 13

        ; now that everything is ready, replace SWOS menu references, rest is in our code
        mov  dword [ExitEuropeanChampionshipMenu + 6], newMainMenu

        mov  eax, ShowSWOSAnniversaryEntry
        mov  ebx, CommonMenuExit + 0x98 + 5
        sub  eax, ebx
        mov  [CommonMenuExit + 0x99], eax

        mov  eax, HideSWOSAnniversaryEntry
        mov  ebx, CommonMenuExit + 0x175 + 5
        sub  eax, ebx
        mov  [CommonMenuExit + 0x176], eax

        retn


; ShowOrHideAnniversaryEntry
;
; in:
;      edi - boolean, show or hide the entry
;
; Show or hide SWOS 16/17 entry in main menu as needed. Called from patched SWOS code,
; so must call CalcMenuEntryAddress before returning.
;
ShowOrHideAnniversaryEntry:
        mov  esi, CalcMenuEntryAddress
        call esi
        mov  ecx, [A0]
        mov  word [D0], 13
        call esi
        mov  eax, [A0]
        mov  [eax + MenuEntry.isInvisible], di
        mov  [A0], ecx
        retn


; ShowSWOSAnniversaryEntry
;
; Show SWOS 16/17 entry when we are not showing prompt to save career.
;
ShowSWOSAnniversaryEntry:
        xor  edi, edi
        jmp  ShowOrHideAnniversaryEntry


; HideSWOSAnniversaryEntry
;
; Hide SWOS 16/17 entry or it will be drawn all across the bottom prompt to save career.
;
HideSWOSAnniversaryEntry:
        or   edi, 1
        jmp  ShowOrHideAnniversaryEntry


ShowSWOSAnniversaryMenu:
        mov  dword [A6], swosAnniversaryMenu
        calla ShowMenu
        jmpa  CommonMenuExit


section .data

global swosUnitedLogo
swosUnitedLogo:
    incbin "../bitmap/swos_united_logo.bp"

    extern SWOSAnniversaryMenuOnDraw, SWOSAnniversaryMenuInit

    ;
    ; SWOS 96/97 menu
    ;
    StartMenu swosAnniversaryMenu, SWOSAnniversaryMenuInit, 0, SWOSAnniversaryMenuOnDraw, 1
        ; title
        StartEntry  92, 2, 120, 11
            EntryColor  0x17
            EntryString 0, "16/17 CREDITS"
        EndEntry

        ; exit
        StartEntry 250, 188, 60, 11
            EntryColor  10
            EntryString 0, aExit
            OnSelect    SetExitMenuFlag
        EndEntry
    EndMenu
