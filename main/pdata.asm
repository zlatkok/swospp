[list -]
%include "swos.inc"
%include "pdata.inc"
%include "version.inc"
[list +]

bits 32

; additional externs
extern int2str
extern LoadSWPPMenu
extern WriteYULetters
extern FixGetTextSize
extern OpenReplayFile
extern CloseReplayFile
extern MyHilPtrIncrement
extern MyHilPtrAdd
extern PrintSmallNumber
extern PushMenu, PopMenu
extern SelectMenuMusic
%ifdef DEBUG
extern EndLogFile
%endif

; patch specific data
section .data

newMessage:
    db "Sensible World Of Soccer V2.0 ", 13, 10
    db "SWOS++ v"
    db verStr
    db 13, 10, "$"

swosppTitle:
%ifdef SENSI_DAYS
    db "SENSIBLE DAYS", 0
%else
    db "SWOS++", 0
%endif


; patch-specific code
section .text

extern replayStatus, EndProgram, SwitchToPrevVideoMode
extern ShutDownNetwork, qAllocFinish


; Hook ALT-F1 fast exit, as well as normal exit from menu.
HookTermination:
        WriteToLog "Termination handler invoked."
        call SwitchToPrevVideoMode
        xor  eax, eax
        jmp  EndProgram


; We are overwriting unused invisible entry in main menu, so have to show it explicitly.
MakeSWOSPPVisible:
        mov  word [currentTeamNumber], -1
SWOSMainMenuAfterDraw:
        mov  dword [D0], 4      ; 4 is our entry number
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  word [esi + 4], 0
        retn


FixFindFiles:
        cmp  byte [esi + 1], 'C'
        jz   .accept
        cmp  byte [esi + 1], 'R'
        jz   .accept
        jmpa FindFiles + 0x102
.accept:
        jmpa FindFiles + 0xd6


FixGetFilenameAndExtension:
        cmp  dword [D1], 'LIH.'
        jz   .accept
        cmp  dword [D1], 'LPR.'
        jz   .accept
        jmpa GetFilenameAndExtension + 0x136
.accept:
        jmpa GetFilenameAndExtension + 0x158


FixSelectFileToSaveDialog:
        cmp  dword [D1], 'LIH.'
        jz   .accept
        cmp  dword [D1], 'LPR.'
        jz   .accept
        jmpa SelectFileToSaveDialog + 0x199
.accept:
        jmpa SelectFileToSaveDialog + 0x154


AddReplayDesc:
        cmp  dword [D1], 'CAT.'
        jz   .jump_back
        mov  dword [A0], aHigh
        cmp  dword [D1], 'LPR.'
        jnz  .jump_back
        mov  dword [A0], aReplay
.jump_back:
        jmpa SelFilesBeforeDrawCommon + 0x13a


; reset control variables to zero, in case setup.dat contains some values
extern VerifyJoysticks
ResetControlVars:
        xor  eax, eax
        mov  [g_joy1XValue], eax
        mov  [g_joy2XValue], eax
        mov  [g_joy1Status], eax
        mov  [setupDatBuffer], ax

%ifndef SENSI_DAYS
        mov  al, [pl2Keyboard]
        test al, al                         ; if true, both players are on the keyboard
        jnz  .pl2_kbd
%endif

        call VerifyJoysticks  ; this will prevent controls blocking if there is currently no joystick attached
        WriteToLog "g_joyKbdWord = %hd, after VerifyJoysticks()", dword [g_joyKbdWord]
        retn

.pl2_kbd:
        mov  word [g_joyKbdWord], 4         ; just in case user changed config through install.exe
        retn


; if shirt number is out of normal range, add 9000 to it
SetPlayerNumber:
        test ax, ax
        jz   .increase
        cmp  ax, 16
        ja   .increase

.return:
        add  word [D0], ax
        retn

.increase:
        add  ax, 9000
        jmp  short .return


; We will execute inside DrawSprites loop, and check sprite number that's being drawn.
; See if it's over 9000!!!! It means it's player number that's greater than 16.
;
; in:
;     ax -  sprite number
;    esi -> Sprite structure
;
DrawPlayerNumber:
        cmp  ax, 9000
        ja   .custom_draw

.return:
        mov  dword [D0], eax
        or   ax, ax
        retn

.custom_draw:
        sub  ax, 9000 + 1187
        cmp  ax, 255                ; clamp it to byte, that field in file is a byte
        ja   .skip_sprite

        push byte 1
        mov  dx, word [esi + 32]    ; do not subtract center x/y, as PrintSmallNumber will do centering of its own
        sub  dx, [cameraX]          ; x - camera x
        mov  cx, word [esi + 36]
        sub  cx, [cameraY]          ; y - camera y
        sub  cx, word [esi + 40]    ; subtract z
        movsx edx, dx
        movsx ecx, cx
        movzx eax, ax               ; number to draw
        inc  ecx
        call PrintSmallNumber

.skip_sprite:
        or   al, -1                 ; force sign flag so loop skips to next sprite
        retn


SubsDrawPlayerNumber:
        mov  esi, [benchTeam]
        mov  esi, [esi + 20]    ; player sprites table
        movzx eax, word [D7]
        shl  eax, 2
        mov  esi, [esi + eax]
        mov  ax, [esi + 102]    ; cards
        test ax, ax
        jz   .no_cards

        pop  eax                ; discard return address
        jmpa DrawSubstitutesMenuEntry + 0x5de ; don't draw player number

.no_cards:
        add  dword [D1], 3      ; add 3 pixels more between faces and names
        movzx eax, byte [D5]
        cmp  eax, byte 16
        jbe  .return

        call  int2str           ; if it's our number, convert it the usual way
        mov  [A0], eax          ; A0 -> string for printing
.return:
        mov  word [D3], 2
        retn


; Fix drawing of little player numbers when editing tactics.
;
; in:
;     D0 - sprite index
;     D1 - x coordinate
;     D2 - y coordinate
;
EditTacticsDrawPlayerNumber:
        movzx eax, word [D0]
        cmp  eax, 161       ; number 0
        jz   .custom_draw

        cmp  word [D0], 173
        jb   .out

.custom_draw:
        push byte 0
        movzx eax, word [D0]        ; OK so we gotta deal with high player number
        sub  ax, 161                ; number to draw = start of little numbers - 1
        movsx edx, word [D1]
        movsx ecx, word [D2]
        call PrintSmallNumber
        retn

.out:
        jmpa DrawSpriteCentered


FixPlayerBooking:
        cbw
        test al, al
        jz   .increase

        cmp  al, 16
        ja   .increase

.return:
        mov  [D1], ax
        retn

.increase:
        add  ax, 9000
        jmp  short .return


HookSaveCoordinatesForHighlights:
        mov  ax, [goToSubsTimer]    ; without this, cutout is too sharp
        test ax, ax
        jnz  .its_ok

        mov  ax, [inSubstitutesMenu]
        test ax, ax
        jnz  .cancel

.its_ok:
        mov  eax, [current_hil_ptr]
        mov  [A1], eax
        retn

.cancel:
        pop  eax
        retn


%ifndef SENSI_DAYS
; HookMenuLoop
;
; This part is crucial for whole network part to work properly. We will execute each
; frame before MenuProc, and it might get skipped depending on network activity.
;
extern NetworkOnIdle, CheckIfJoystickDisconnected
HookMenuLoop:
        call CheckIfJoystickDisconnected
        call NetworkOnIdle
        test eax, eax
        jz   .skip_menu_proc

        calla ReadTimerDelta
        jmpa DrawMenu

.skip_menu_proc:
        pop  eax
        retn


HookInputText:
        push dword [A0]
        push dword [A6]
        call NetworkOnIdle
        pop  dword [A6]
        pop  dword [A0]
        test eax, eax
        jnz  .continue_input

        push byte 1     ; cancel input completely
        pop  eax
        mov  [D0], eax
        or   eax, eax
        pop  ebx
        retn

.continue_input:
        calla GetKey
        calla FilterKeys
        retn


; CheckInputDisabledTimer
;
; Patch CheckControls to add support for temporary disabling input in menus,
; and also to have an in-menu input received from the other player.
;
extern BroadcastControls, GetControlsFromNetwork, pl2Keyboard
declareGlobal disabledInputCycles, 1
declareGlobal lastFireState, 1
CheckInputDisabledTimer:
        mov  al, [disabledInputCycles]
        test al, al
        jz   .go_on

        cmp  al, -1                         ; -1 signaling that controls are (possibly) coming from other player
        jz   .redirect_input

        dec  byte [disabledInputCycles]

.no_key:
        mov  word [lastKey], 0
        mov  word [keyCount], 0
        mov  word [final_controls_status], -1
        mov  byte [convertedKey], 0
        mov  word [fire], 0
        mov  word [short_fire], 0
        pop  eax                            ; skip entire function
        retn

.redirect_input:
        call GetControlsFromNetwork
        cmp  eax, -1
        jz   .get_real_key

        mov  ecx, eax
        shr  ecx, 16                        ; ecx = long fire in high word
        and  eax, 0xffff

        test al, 0x80
        jz   .no_change_in_fire_state

        xor  byte [lastFireState], -1

.no_change_in_fire_state:
        and  al, ~0x80
        test al, al
        jnz  .process_key

        calla GetKey                        ; to enable alt-f1
        jmp  .no_key

.process_key:
        mov  [g_joy1Status], ax
        mov  byte [Joy1SetStatus], 0xc3     ; prevent it from writing real values to joy1Status
        call .go_on                         ; call GetKey so alt-f1 can end the program
        pop  ebx
        push dword [g_joyKbdWord]
        mov  al, [pl2Keyboard]              ; if 2nd player on keyboard is on,
        cbw                                 ; CheckControls will be patched
        mov  [g_joyKbdWord], ax             ; prevent joy1Status getting overwritten in any case
        mov  word [controlsHeldTimer], 2000 ; prevent too fast commands getting rejected
        push ecx
        call ebx
        pop  ecx
        pop  dword [g_joyKbdWord]
        mov  byte [Joy1SetStatus], 0x66
        mov  al, byte [lastFireState]
        cbw
        mov  [fire], ax
        movzx eax, byte [g_joy1Status]
        shr  eax, 5
        and  eax, 1
        neg  eax
        mov  [short_fire], ax               ; we will get short fire each time it's issued
        mov  [longFireFlag], cx
        retn

.get_real_key:
        call .go_on
        pop  ebx
        call ebx                            ; let CheckControls execute in full
        xor  eax, eax                       ; al = controls byte
        cmp  word [final_controls_status], -1   ; make sure we don't flood
        jz   .check_short_fire

        mov  al, [controlWord]              ; send real controls for player1, keyboard or joystick
        cmp  word [g_joyKbdWord], 1
        jz   .check_short_fire
        mov  al, [g_joy1Status]

.check_short_fire:
        and  al, ~0x20
        cmp  byte [short_fire], 0
        jz   .check_long_fire
        or   al, 0x20                       ; short fire is generated periodically

.check_long_fire:
        mov  bl, [fire]
        cmp  bl, [lastFireState]
        jz   .test_controls

        or   al, 0x80                       ; signal long fire state change
        mov  [lastFireState], bl            ; use previously unused bit 7 to signal it

.test_controls:
        test al, al
        jz   .no_key

        movzx edx, word [longFireFlag]
        call BroadcastControls
        retn

.go_on:
        calla GetKey
        mov  ax, [lastKey]

.out:
        retn


; HookMainMenuSetup
;
; Set initial menu SWOS shows in A6.
;
extern MainMenuSelect
HookMainMenuSetup:
        call MainMenuSelect
        mov  [A6], eax
        retn
%endif


; HookSetBenchPlayersNumbers
;
; in:
;     al  -  bench player ordinal in lineup (12-16)
;     esi -> player (file) + negative offset of header size
;
; Don't reset bench player's shirt number if it's a high number (> 16, or zero)
;
HookSetBenchPlayersNumbers:
        mov  bl, [esi + PlayerFile.shirtNumber + TEAM_FILE_HEADER_SIZE]
        test bl, bl
        jz   .skip

        cmp  bl, 16
        jg   .skip

        mov  [esi + PlayerFile.shirtNumber + TEAM_FILE_HEADER_SIZE], al
        retn

.skip:
        pop  eax
        retn


; HookBenchPlayerSwap
;
; in:
;     esi -> player 1 (file) - header
;     A1  -> player 2 (file) - header
;
; Prevent shirt number swapping in case any of players has high (custom) shirt
; number. If none of them does, allow standard behaviour to happen.
;
HookBenchPlayerSwap:
        mov  al, [esi + PlayerFile.shirtNumber + TEAM_FILE_HEADER_SIZE]
        cmp  al, 16
        jg   .skip

        test al, al
        jz   .skip

        mov  esi, [A1]
        mov  bl, [esi + PlayerFile.shirtNumber + TEAM_FILE_HEADER_SIZE]
        cmp  bl, 16
        jg   .skip

        test bl, bl
        jz   .skip

        mov  [D0], al
        retn

.skip:
        pop  eax
        add  eax, 0x21
        jmp  eax


; .ptdata name is mandatory
section .ptdata data

; must be present
global PatchStart
PatchStart:

; --------------------------------------
; user modifiable part

    ; infiltrate starting message
    PatchOffset Initialization + 0x1c, newMessage

    ; patch in entry in main menu for our menu
    StartRecord SWOS_MainMenu + 0x9e
        dw 85    ; x
        dw 20    ; y
        dw 150   ; width
        dw 16    ; height
        dw 1     ; type 1 - invisible item, will be corrected in menu init
        dw 2     ; type 2 - positions
        db -1    ; left - nothing
        db -1    ; right - nothing
        db -1    ; up - nothing
        db 5     ; down - goto "FRIENDLY"
        dw 4     ; type 4 - background color
        dw 14    ; green
        dw 8     ; type 8 - string
        dw 16    ; string flags
        dd swosppTitle
        dw 13    ; type 13 - on select function
        dd LoadSWPPMenu
        dw 0     ; type 0 - end of menu entry
    EndRecord

    ; patch "EDIT TACTICS" and "FRIENDLY" entries up entry number
    PatchByte SWOS_MainMenu + 0x22, 4
    PatchByte SWOS_MainMenu + 0xce, 4

    ; set SWOS++ entry to default in main menu
    PatchByte SWOS_MainMenu + 0xc, 4

    ; patch main menu init routine and make our SWOS++ entry visible
    StartRecord SWOSMainMenuInit
        calla MakeSWOSPPVisible
        nop
        nop
    EndRecord

    ; patch main menu after draw to keep SWOS++ entry visible
    PatchOffset SWOS_MainMenu + 4, SWOSMainMenuAfterDraw

    ; patch DrawMenuText to add support for Serbian letters
    StartRecord DrawMenuText + 0xb7
        calla WriteYULetters
    EndRecord

    ; fix GetTextSize so it could count 'dj' correctly
    StartRecord GetTextSize + 0x81
        calla FixGetTextSize
    EndRecord

    ; hook game start
    StartRecord GameLoop + 0x1c8
        calla OpenReplayFile
        nop
        nop
    EndRecord

    ; hook game end
    StartRecord GameLoop + 0x64c
        xor  eax, eax           ; signal call from patched code
        calla CloseReplayFile, ebx
    EndRecord

    ; hook incrementing hil pointer so we can save it to file when overflows
    StartRecord IncrementHilBufferPointer
        jmpa MyHilPtrIncrement
    EndRecord

    ; hook incrementing hil pointer while replaying
    StartRecord ValidateHilPointerAdd
        jmpa MyHilPtrAdd
    EndRecord

    ; patch stoopid FindFiles not to reject *.rpl files
    StartRecord FindFiles + 0xce
        jmpa FixFindFiles
    EndRecord

    ; GetFilenameAndExtension must also be patched to allow new extensions
    StartRecord GetFilenameAndExtension + 0x14c
        jmpa FixGetFilenameAndExtension
    EndRecord

    ; patch SelectFileToSaveDialog to show *.rpl files
    StartRecord SelectFileToSaveDialog + 0x148
        jmpa FixSelectFileToSaveDialog
    EndRecord

    ; patch in description for replay files
    StartRecord SelFilesBeforeDrawCommon + 0x124
        jmpa AddReplayDesc
    EndRecord

    ; hook after reading setup.dat to reset control variables
    StartRecord Initialization + 0x4d
        calla ResetControlVars
        times 2 nop
    EndRecord

    ; draw player numbers
    StartRecord DrawControlledPlayerNumbers + 0x184
        calla SetPlayerNumber, ebx ; ax needs to hold shirt number
    EndRecord

    ; hook DrawSprites to draw player numbers above 16 and zero
    StartRecord DrawSprites + 0x65
        calla DrawPlayerNumber, ebx
        nop
        nop
    EndRecord

    ; fix substitutes menu to draw big player numbers correctly
    StartRecord DrawSubstitutesMenuEntry + 0x570
        calla SubsDrawPlayerNumber
        times 2 nop
    EndRecord

    ; push/pop A4 is unnecessary
    StartRecord DrawLittlePlayersAndBall + 0x323
        calla EditTacticsDrawPlayerNumber
        times 4 nop
    EndRecord

    FillRecord DrawLittlePlayersAndBall + 0x32e, 6, 0x90

    ; expand menu entries and move player names and positions by 4 pixels in
    ; substitutes menu to allow bigger shirt numbers
    PatchByte DrawSubstitutesMenuEntry + 0x731, 15  ; move name
    PatchByte DrawSubstitutesMenuEntry + 0x829, 122 ; move position
    PatchByte DrawSubstitutesMenuEntry + 0x38, 132  ; entry length
    PatchByte DrawSubstitutesMenuEntry + 0x168, 130 ; entry solid background

    ; prevent SWOS from reseting bench players numbers to 12..16, but only for high numbered players
    StartRecord SetBenchPlayersNumbers + 0x40
        calla HookSetBenchPlayersNumbers, ebx
        nop
        nop
    EndRecord

    ; player sprite number setting is also done while booking
    StartRecord BookPlayer + 0x165
        calla FixPlayerBooking, ebx
    EndRecord

    ; move card sprites a little bit
    PatchByte DrawSubstitutesMenuEntry + 0x687, 12

    ; hook program end to close log file and some other things
    StartRecord SetPrevVideoModeEndProgram
        calla HookTermination
    EndRecord

%ifdef DEBUG
    ; write int1 before call to SWOS to allow symbol loading
    ;StartRecord DumpTimerVariables + 0x81
    ;    int 1
    ;EndRecord
%endif

    ; always set graphics to VGA to enforce consistency
    PatchByte Initialization + 0x5e, 0xeb

    ; fix overwrite goal with bench scene bug
    ; (also causes corruption of replay files)
    FillRecord BenchCheckControls + 0x78f, 5, 0x90

    ; reject save coordinates requests while in substitutes menu
    StartRecord SaveCoordinatesForHighlights
        calla HookSaveCoordinatesForHighlights
        times 3 nop
    EndRecord

    ; hook main keys check in game to allow new keyboard commands
    extern HookMainKeysCheck
    StartRecord MainKeysCheck
        calla HookMainKeysCheck
        nop
    EndRecord

    ; hook flipping to implement fast replays
    extern CheckForFastReplay
    StartRecord Flip
        calla CheckForFastReplay
        nop
    EndRecord

%ifndef SENSI_DAYS
    ; hook menu loop to implement OnIdle() for network
    StartRecord MenuProc
        calla HookMenuLoop
        times 3 nop
    EndRecord

    ; hook InputText loop to allow OnIdle() to run while entering text
    StartRecord InputText + 0x78
        calla HookInputText
        times 3 nop
    EndRecord
%endif

    ; return whatever dest pointer reached when length ran out, + 1
    FillRecord swos_libc_strncpy_ + 0x1d, 5, 0x90

%ifndef SENSI_DAYS
    ; allow input to be disabled for a while, so user doesn't instantly exit modal menu
    StartRecord CheckControls
        calla CheckInputDisabledTimer
        times 4 nop
    EndRecord
%endif

    ; fix upper half of edx getting dirty in ClearBackground
    StartRecord ClearBackground + 0x3e
        mov  edx, 384   ; overwrites next instruction size prefix and all fits perfectly
    EndRecord

    ; record all open menus to the stack, so we can return to arbitrary one
    StartRecord ShowMenu + 0x34
        calla PushMenu
        nop
        nop
    EndRecord

    ; hook menu exit too and delete them from stack
    StartRecord ShowMenu + 0x66
        calla PopMenu
        nop
        nop
    EndRecord

    ; hook saving of menu to return to after the game
    extern HookMenuSaveAfterTheGame
    StartRecord SWOS + 0x179
        calla HookMenuSaveAfterTheGame
        nop
        nop
    EndRecord

    ; fix InputText to limit text properly when we start with buffer already filled more than limit
    PatchByte InputText + 0x25d, 0x83

%ifndef SENSI_DAYS
    StartRecord InitMainMenuStuff + 0x114
        calla HookMainMenuSetup
        times 3 nop
    EndRecord
%endif

    ; kill this bastard
    PatchByte SetDefaultOptions, 0xc3

    ; make DoUnchainSpriteInMenus use entire D1 (and not just lo word with hi word undefined)
    StartRecord DoUnchainSpriteInMenus
        nop
    EndRecord

    ; fix game length bug on PC version
    PatchByte UpdateTime + 0xfa, 70

    ; fix conversion of shirt numbers greater than 127
    PatchWord MakePlayerNameSprite + 5, 0x00b4

    ; hook player swap so bench players retain their custom shirt numbers
    StartRecord PlayMatchMenuSwapPlayers + 0x100
        calla HookBenchPlayerSwap
        nop
    EndRecord

    extern ReadGamePort2
    StartRecord SWOS_ReadGamePort + 0x10
        jmpa ReadGamePort2
    EndRecord

    ; patch reading of joystick keys which is done directly in Joy1SetStatus and Joy2SetStatus
    extern ReadJoystickButtons
    StartRecord Joy1SetStatus + 0x3b
        calla ReadJoystickButtons
    EndRecord

    StartRecord Joy2SetStatus + 0x3b
        calla ReadJoystickButtons
    EndRecord

%ifdef SENSI_DAYS
    ; prefer swos.xmi over menu.xmi
    StartRecord InitMenuMusic + 0xa
        calla SelectMenuMusic
        times 3 nop
    EndRecord
%endif

%ifdef DEBUG
    ; don't waste time on opening animations in debug version
    PatchByte Initialization + 0x19, 1
%endif


; --------------------------------------
; do not edit lines below


; patch data must be terminated with address -1
    db 0
    dd -1

; this symbol must exist and be at the end
global PatchSize
PatchSize:
    dd $ - PatchStart
