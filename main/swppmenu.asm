; Contains SWOS++ main menu, and some of the menus derived from it, as well
; as belonging menu manipulation routines.

[list -]
%include "swos.inc"
%include "swsmenu.inc"
%include "version.inc"
[list +]

; %1 - string, %2 (optional) starting index
%macro xorstr 1-2
    %if %0 == 2
        %ifnnum %2
            %error "Second parameter must be numeric"
        %endif
    %endif
    %ifnstr %1
        %error "First parameter must be string"
    %endif
    %assign i 1
    %strlen len %1
    %rep len
        %substr char %1 i
        %if %0 == 2
            db char ^ i + %2
        %else
            db char ^ i
        %endif
        %assign i  i + 1
    %endrep
%endmacro

extern DrawBitmap
extern AnimateBalls
extern InitCounter
extern ShowReplaysMenu
extern ShowControlsMenu

extern EditDIYFile
extern DIYFileSelected
extern DIYUpdateList
extern DIYEditScrollUp
extern DIYEditScrollDown
extern DIYTeamsListInit
extern DIYTeamsOnSelect
extern DIYTeamsExit

section .text
bits 32

global LoadSWPPMenu
LoadSWPPMenu:
        mov  dword [A6], swosppMenu
        calla ShowMenu
        jmpa  CommonMenuExit

; swospp menu

SWOSPPAbout:
        mov  dword [A6], aboutMenu
show_menu:
        jmpa ShowMenu

%ifndef SENSI_DAYS
extern multiplayerMenu
ShowMultiplayerMenu:
        mov  dword [A6], multiplayerMenu
        jmp  short show_menu
%endif


; about menu

DrawLogo:
        mov  eax, WIDTH
        mov  edx, HEIGHT
        movzx ecx, byte [logoData]      ; logo width
        movzx ebx, byte [logoData + 1]  ; logo height
        sub  eax, ecx
        shr  eax, 1
        sub  edx, ebx
        shr  edx, 1
        sub  edx, byte 4
        push logoData + 2
        push ebx
        call DrawBitmap
        retn


section .data

global aboutText
aboutText:
    db 3
    xorstr "SWOS++ V"
    xorstr verStr, 8
%ifdef DEBUG
    xorstr " (DEBUG)", 8 + verStrLen
%endif
    db 0
    xorstr "PROGRAMMED BY ZLATKO KARAKAŠ"
    db 0
    xorstr "SPECIAL THANKS TO PLAYAVELI"
    db 0


; menus

    ;
    ; SWOS++ main menu
    ;

    StartMenu swosppMenu, 0, 0, 0, 1
        ; title
        StartEntry  92, 0, 120, 15
            EntryColor  0x17
%ifdef SENSI_DAYS
            EntryString 0, "SENSIBLE DAYS"
%else
            EntryString 0, "SWOS++ MENU"
%endif
        EndEntry

%ifndef SENSI_DAYS
        ; multiplayer! yeah baby!
        StartEntry 92, 40, 120, 15, multiplayerEntry
            NextEntries -1, -1, -1, currentEntry + 1
            EntryColor 14
            EntryString 0, "MULTIPLAYER"
            OnSelect    ShowMultiplayerMenu
        EndEntry
%else
%define swosppMenu_multiplayerEntry -1
%assign previousEntryY 15
%endif

        ; replays
        StartEntry 92, previousEntryY + 25, 120, 15
            NextEntries -1, -1, swosppMenu_multiplayerEntry, currentEntry + 1
            EntryColor 14
            EntryString 0, "REPLAYS"
            OnSelect    ShowReplaysMenu
        EndEntry

        ; controls
        StartEntry 92, previousEntryY + 25, 120, 15
            NextEntries -1, -1, currentEntry - 1, currentEntry + 1
            EntryColor  14
            EntryString 0, "CONTROLS"
            OnSelect    ShowControlsMenu
        EndEntry

        ; edit/load diy file
        StartEntry 92, previousEntryY + 25, 120, 15
            NextEntries -1, -1, currentEntry - 1, currentEntry + 1
            EntryColor  14
            EntryString 0, "EDIT/LOAD DIY FILE"
            OnSelect    EditDIYFile
        EndEntry

        ; about
        StartEntry 92, previousEntryY + 25, 120, 15
            NextEntries -1, -1, currentEntry - 1, currentEntry + 1
            EntryColor  14
            EntryString 0, "ABOUT"
            OnSelect    SWOSPPAbout
        EndEntry

        ; exit
        StartEntry 102, 185, 100, 15
            NextEntries -1, -1, currentEntry - 1, -1
            EntryColor  12
            EntryString 0, aExit
            OnSelect    SetExitMenuFlag
        EndEntry
    EndMenu  ; swosppMenu


    ;
    ; about menu
    ;

    StartMenu aboutMenu, InitCounter, 0, DrawLogo, 0

        ; exit
        StartEntry 102, 185, 100, 15
            NextEntries
            EntryColor  12
            EntryString 0, aExit
            OnSelect    SetExitMenuFlag
        EndEntry

        ; about string
        StartEntry 55, 142, 200, 40
            MultiLineText 0, aboutText
            AfterDraw   AnimateBalls
        EndEntry

    EndMenu  ; aboutMenu


    ;
    ; edit/load DIY files menu
    ;

    global edit_DIY_Menu
    StartMenu edit_DIY_Menu, DIYUpdateList, DIYUpdateList, 0, 1
        ; [0] title
        StartEntry  2, 0, 300, 15
            EntryColor  0x17
            EntryString 0, "SELECT DIY FILE TO EDIT/LOAD"
        EndEntry

        ; [1] exit
        StartEntry 121, 185, 62, 15
            EntryColor  12
            EntryString 0, aExit
            OnSelect    SetExitMenuFlag
        EndEntry

        ; [2] down arrow
        StartEntry 0, 180, 8, 14
            NextEntries -1, 1, 3, -1
            EntryColor   0
            EntrySprite2 184
            OnSelect     DIYEditScrollDown
        EndEntry

        ; [3] up arrow
        StartEntry 0, 25, 8, 14
            NextEntries -1, 4, -1, 2
            EntryColor   0
            EntrySprite2 183
            OnSelect     DIYEditScrollUp
        EndEntry

        ; template entry
        StartTemplateEntry
        StartEntry
            EntryColor  2
            EntryString 0, 0
            OnSelect    DIYFileSelected
        EndEntry

        ; [4] entries that show filenames
        %assign y 42
        %rep 45
            StartEntry 43, y, 56, 8
            EndEntry
        %assign y y + 9
        %endrep

        ; template entry
        StartTemplateEntry
        StartEntry
            EntryColor  8
            EntryString 0, 0
        EndEntry

        ; [49] entries that show long tournament names
        %assign y 42
        %rep 45
            StartEntry 43 + 56 + 1, y, 160, 8
            EndEntry
        %assign y y + 9
        %endrep

    EndMenu


    ;
    ; edit teams control
    ;

    global teamListMenu
    StartMenu teamListMenu, DIYTeamsListInit, DIYTeamsListInit, 0, 66
        ; teams template entry
        StartTemplateEntry
        StartEntry 0, 0, 0, 0
            EntryString 0, 0
            OnSelect    DIYTeamsOnSelect
        EndEntry

        ; [0] teams list
        MenuXY 0, 0
        %rep 66
            StartEntry 0, 0, 100, 8
            EndEntry
        %endrep
        RestoreTemplateEntry

        ; [66] exit
        StartEntry 121, 189, 62, 10
            EntryColor  12
            EntryString 0, aExit
            OnSelect    DIYTeamsExit
        EndEntry

        ; [67] title
        StartEntry  2, 0, 300, 15
            EntryColor  0x17
            EntryString 0, "CHOOSE TEAM CONTROLS"
        EndEntry

        ; legend
        ; [68] computer background
        StartEntry 20, 16, 19, 9
            EntryColor 10
        EndEntry

        ; [69] player-coach background
        StartEntry 120, 16, 19, 9
            EntryColor 11
        EndEntry

        ; [70] coach background
        StartEntry 220, 16, 19, 9
            EntryColor 13
        EndEntry

        ; [71] computer text
        StartEntry 45, 16, 0, 11
            EntryString 0x8000, aComputer
        EndEntry

        ; [72] player-coach text
        StartEntry 145, 16, 0, 11
            EntryString 0x8000, aPlayerCoach_0
        EndEntry

        ; [73] coach text
        StartEntry 245, 16, 0, 11
            EntryString 0x8000, aCoach_3
        EndEntry

    EndMenu


section .text

;
; === Modal dialogs
;


; InitModalDialog
;
; in:
;     eax -> initialization parameters structure
;
; destroys: eax only
;
; Call this before entering modal dialog loop. Format:
;   0 byte  - flags:
;             bit 0 - show ok button
;             bit 1 - show bitmap(set), show sprite(clear)
;             bit 2 - animate
;             bit 3 - loop animation
;   1 byte  - delay between frames
;   2 word  - starting sprite index - if showing sprite
;   4 word  - ending sprite index
;   2 dword - pointer to bitmap animation table - if showing bitmap
;             pointer to array of pointers to bitmaps terminated by null
;   6 word  - sprite/bitmap width
;   8 word  - sprite/bitmap height
;  10 dword - pointer to string table, format: dword - num. strings, that many pointers to strings follow
;
global InitModalDialog
InitModalDialog:
        mov  [modalDlgArgs], eax
        mov  ax, [eax + 2]
        mov  [currentSprite], ax
        mov  ax, [g_currentTick]
        mov  [lastTick], ax
        mov  word [direction], 1
        mov  byte [currentMenuFrame], 0
        mov  byte [currentBitmapIndex], 0
        retn


; DrawModalDialog
;
; Draw modal dialog - a framed menu entry with background and text lines. Text
; lines will adapt to fit (but it would probably work OK for up to few lines).
; Draw animation if specified.
;
%define DLG_X       50
%define DLG_Y       60
%define DLG_WIDTH  220
%define DLG_HEIGHT  80
global DrawModalDialog
DrawModalDialog:
        mov  ebx, DLG_Y
        mov  ecx, DLG_WIDTH
        mov  edx, DLG_HEIGHT
        mov  esi, [modalDlgArgs]
        mov  al, [esi]
        test al, 1
        mov  eax, DLG_X
        jnz  .draw_framed_rect

        add  eax, 30        ; lesser size dialog when no 'ok' button
        sub  ecx, 60

.draw_framed_rect:
        mov  esi, 12
        call DrawFramedRectangle
        mov  esi, [modalDlgArgs]
        mov  eax, [esi + 10]
        test eax, eax
        jz   .no_strings

        lea  esi, [eax + 4] ; esi -> first string
        mov  eax, [eax]
        test eax, eax
        jz   .no_strings

        mov  edx, eax       ; edx = string counter
        lea  ebx, [eax + 1]
        mov  eax, DLG_HEIGHT
        div  bl
        movzx ebx, al       ; ebx = dy (dialog height divided by num. string + 1)
        sub  ebx, 4
        mov  dword [D1], DLG_X + DLG_WIDTH / 2
        mov  dword [D2], ebx
        add  dword [D2], DLG_Y - 10
        mov  dword [D3], 2   ; color white
        mov  dword [A1], bigCharsTable

.next_string:
        mov  eax, [esi]
        mov  [A0], eax
        push ebx
        push edx
        push esi
        push dword [D1]
        push dword [D2]
        push dword [D3]
        push dword [A1]
        calla DrawMenuTextCentered
        pop  dword [A1]
        pop  dword [D3]
        pop  dword [D2]
        pop  dword [D1]
        pop  esi
        pop  edx
        pop  ebx
        add  esi, 4         ; advance to next string
        add [D2], ebx
        dec  edx
        jnz  .next_string

.no_strings:
        xor  eax, eax
        xor  ecx, ecx
        xor  edx, edx
        mov  esi, [modalDlgArgs]
        mov  al, [esi]      ; check flags, are we doing animation
        mov  bx, [esi + 2]  ; starting sprite index
        test al, 2          ; sprite or bitmap?
        jnz  .test_bitmap_index

        test al, 4
        jz   .sprite_index_ok

        test ebx, ebx
        jb   .no_sprite

        mov  dx, [g_currentTick]
        mov  ax, [lastTick]
        mov  cl, [esi + 1]

.test_loop:
        add  eax, ecx
        cmp  eax, edx
        jge  .check_sprite_index

        mov  di, [direction]
        test di, di
        jz   .decrease

        inc  word [currentSprite]
        jmp  .test_loop

.decrease:
        dec  word [currentSprite]
        jmp  .test_loop

.check_sprite_index:
        sub  eax, ecx
        mov  [lastTick], ax
        mov  bx, [currentSprite]
        mov  ax, [esi + 4]
        cmp  ax, bx
        jge  .check_lower_bound

        mov  bx, [esi + 4]
        xor  byte [direction], 1

.check_lower_bound:
        mov  ax, [esi + 2]
        cmp  ax, bx
        jle  .sprite_index_ok

        mov  bx, [esi + 2]
        xor  byte [direction], 1

.sprite_index_ok:
        mov  [D0], bx
        mov  [currentSprite], bx
        mov  ax, [esi + 6]
        shr  eax, 1
        mov  word [D1], DLG_X + DLG_WIDTH / 2
        sub  [D1], ax
        mov  ax, [esi + 8]
        shr  eax, 1
        mov  word [D2], DLG_Y + 4 * DLG_HEIGHT / 5
        sub  [D2], ax
        push esi
        calla SWOS_DrawSprite
        pop  esi
        jmp  .no_sprite

.test_bitmap_index:
        xor  ebx, ebx
        mov  bl, [currentBitmapIndex]

.test_bitmap_index_loop:
        mov  edi, [esi + 2]
        shl  ebx, 2
        mov  edi, [edi + ebx]
        test edi, edi
        jnz  .draw_bitmap

        shr  ebx, 2
        dec  ebx
        test byte [esi], 8      ; bit 3 - animation loop
        jz   .test_bitmap_index_loop

        xor  ebx, ebx
        jmp  .test_bitmap_index_loop

.draw_bitmap:
        shr  ebx, 2
        mov  [currentBitmapIndex], bl
        push esi
        movzx ecx, word [esi + 6]
        movzx ebx, word [esi + 8]
        mov  eax, DLG_X + DLG_WIDTH / 2 + 20
        mov  edx, DLG_Y + DLG_HEIGHT - 6 - 4
        sub  edx, ebx
        push edi
        push ebx
        call DrawBitmap
        pop  esi

        xor  eax, eax
        xor  ebx, ebx
        test byte [esi], 4      ; is animation on?
        jz   .no_sprite

        mov  al, [esi + 1]      ; animation delay
        mov  bx, [g_currentTick]
        sub  ebx, eax
        cmp  bx, [lastTick]
        jb   .no_sprite

        inc  byte [currentBitmapIndex]
        add  ebx, eax
        mov  [lastTick], bx

.no_sprite:
        mov  al, [esi]
        test al, 1      ; show OK button flag
        jz   .out

        mov  eax, DLG_X + DLG_WIDTH / 2 - 15
        mov  ebx, DLG_Y + DLG_HEIGHT - 15 - 6 - 4
        mov  ecx, 30
        mov  edx, 14
        mov  esi, 14
        push eax
        push ebx
        push ecx
        push edx
        call DrawFramedRectangle
        mov  word [D1], DLG_X + DLG_WIDTH / 2
        mov  word [D2], DLG_Y + DLG_HEIGHT - 12 - 6 - 4
        mov  word [D3], 2
        mov  dword [A0], aOk
        mov  dword [A1], bigCharsTable
        calla DrawMenuTextCentered
        pop  edx
        pop  ecx
        pop  ebx
        pop  eax
        call DrawShiningFrame
.out:
        retn


; DrawFramedRectangle
;
; in:
;     eax - x
;     ebx - y
;     ecx - width
;     edx - height
;     esi - color
;
; Draws a shaded rectangle with frame.
;
DrawFramedRectangle:
        mov  [D0], esi
        mov  [D1], eax
        mov  [D2], ebx
        mov  [D3], ecx
        mov  [D4], edx
        push dword [D0]
        push dword [D1]
        push dword [D2]
        push dword [D3]
        push dword [D4]
        calla DrawMenuBack
        pop  dword [D4]
        pop  dword [D3]
        pop  dword [D2]
        pop  dword [D1]
        pop  dword [D0]
        calla DrawOuterFrame
        retn


; DrawShiningFrame
;
; in:
;     eax - x
;     ebx - y
;     ecx - width
;     edx - height
;
DrawShiningFrame:
        mov  [D1], eax
        mov  [D2], ebx
        mov  [D3], ecx
        mov  [D4], edx
        xor  eax, eax
        mov  ax, [currentMenuFrame]
        inc  eax
        mov  [currentMenuFrame], ax
        and  eax, 7
        shl  eax, 1
        mov  esi, colorTableShine
        mov  ax, [esi + eax]
        mov  [D0], ax
        jmpa DrawMenuFrame


;
; === Menu stack system
;


; HookMenuSaveAfterTheGame
;
; in:
;     esi -> currentMenu
;
; Set the menu we should return to after the game (if menu that called the game
; isn't suitable). Patched right before main loop starts.
;
global HookMenuSaveAfterTheGame
HookMenuSaveAfterTheGame:
        pop  ebx
        push dword [esi + Menu.currentEntry]
        push dword [currentMenuPtr]
        mov  [savedMenuAfterGame], esp
        jmp  ebx


; SetMenuToReturnToAfterTheGame
;
; in:
;     eax -> menu to return to (packed)
;     edx -  entry ordinal to make active upon return
;
global SetMenuToReturnToAfterTheGame
SetMenuToReturnToAfterTheGame:
        mov  esi, [savedMenuAfterGame]
        mov  [esi], eax
        mov  [D0], edx
        calla CalcMenuEntryAddress
        mov  eax, [A0]
        mov  [esi + 4], eax
        retn


; DecreasePtr
;
; in:
;     esi -> pointer into menu stack
;
; out:
;     esi -> pointer to previous position, wrapped around if needed
;
DecreasePtr:
        cmp  esi, menuStack
        jnz  .ptr_ok

        mov  esi, menuStackEnd

.ptr_ok:
        sub  esi, 4
        retn


; PushMenu
;
; Called from SWOS, from ShowMenu. Record stack pointer so we can return to any
; of the menus that called us, not just the last one. Stack should look like
; this on entry:
; - return address from our patched call <- stack top
; - previous active menu entry pointer
; - previous menu pointer
; - callers address (that called ShowMenu)
;
; Stack state is in our favour allowing us to track both stack state and which menu
; that state belongs simply by dereferencing saved stack pointer.
;
global PushMenu
PushMenu:
        mov  esi, [menuStackPtr]
        cmp  esi, menuStackEnd
        jnz  .ptr_ok

        WriteToLog "PushMenu(), overflow!!!"
        mov  esi, menuStack

.ptr_ok:
        pop  eax
        mov  [esi], esp
        push eax

        add  esi, 4
        mov  [menuStackPtr], esi

        mov  word [g_exitMenu], 0
        retn


; PopMenu
;
; Called from SWOS, from ShowMenu, after last shown menu has been exited. Check
; if a custom menu to return to has been queued and set up stack to return to
; it, if not just remove current one from our internal stack of currently open
; menus.
;
global PopMenu
PopMenu:
        mov  esi, [menuStackPtr]
        call DecreasePtr
        mov  [esi], eax
        mov  [menuStackPtr], esi

        mov  eax, [menuToReturnTo]
        test eax, eax
        jz   .out

        pop  ebx        ; pick up return address first, since we're trashing the stack
        mov  esp, eax
        mov  dword [menuToReturnTo], 0
        push ebx

.out:
        mov  word [g_exitMenu], 0
        retn


; ReturnToMenu
;
; in:
;     eax -> menu pointer we are looking for (to return to the one before it)
;     edx -  go back to previous menu if true, to the specified one if false
; out:
;     eax -> stack frame of returning menu, or null
;
; Searches the stack of open menus for a user specified menu, and sets the mark
; where stack will be unwinded after ShowMenu call, ie. the program execution
; continues seamlessly after ShowMenu call that displayed it.
;
global ReturnToMenu
ReturnToMenu:
        mov  esi, [menuStackPtr]
        call DecreasePtr
        mov  edi, esi           ; esi = current, edi = limit

.search:
        mov  ebx, [esi]
        test ebx, ebx
        jz   .next

        cmp  eax, [ebx]         ; pushed pointer to menu
        jz   .found_it

.next:
        call DecreasePtr

        cmp  esi, edi
        jnz  .search

        xor  eax, eax
        retn                    ; entry not found

.found_it:
        test edx, edx
        jz   .set_stack_marker

        call DecreasePtr        ; we actually need previous one
        cmp  esi, edi
        jnz  .set_stack_marker

        xor  eax, eax
        retn                    ; previous entry overwritten

.set_stack_marker:
        mov  eax, [esi]         ; set the marker - menu was found and we will return to it later
        mov  [menuToReturnTo], eax
        retn


; GetPreviousMenu
;
; out:
;      eax -> previous menu on the stack (one we will return to)
;
global GetPreviousMenu
GetPreviousMenu:
        mov  esi, [menuStackPtr]
        call DecreasePtr
        mov  eax, [esi]
        test eax, eax
        jz   .out

        mov  eax, [eax]
.out:
        retn


section .bss

menuToReturnTo:     ; esp value of custom menu frame to return to (instead of the one that called us)
        resd 1
savedMenuAfterGame: ; menu to which we return after game was played
        resd 1
modalDlgArgs:
        resd 1
currentSprite:
        resw 1
lastTick:
        resw 1
direction:
        resb 1
currentMenuFrame:
        resb 2
currentBitmapIndex:
        resb 1
menuStack:          ; circular buffer of menu stack frames, this will hopefully be enough
        resd 10     ; (main -> swpp -> mp -> lobby -> choose team -> club teams -> europe -> england -> league/division)
menuStackEnd:


section .data

menuStackPtr:
        dd menuStack
logoData:
        incbin "../bitmap/swospp-logo.bp"
