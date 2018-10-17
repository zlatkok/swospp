; controls.asm
;
; Controls changing and its menu. Also contains code for new keyboard commands,
; and for InputText patch which enables usage of additional keys while inputing
; text and shift key support.
;

[list -]
%include "swos.inc"
%include "swsmenu.inc"
[list +]

bits 32

extern int2str
extern SaveOptionsIfNeeded

section .text

extern replayStatus
extern rplFrameSkip
extern HandleMPKeys


; RegisterControlsOptions [called from C++]
;
; in:
;      eax -> options register callback (see options.c for format details)
;
; Register our options with options manager, so we can save and load later without much effort.
;
global RegisterControlsOptions
RegisterControlsOptions:
        pushad              ; no idea what changes will callback make
        callCdecl eax, "controls", 8, "Player 2 keyboard controls", 26, \
            "%1d/pl2Keyboard%1d/codeUp%1d/codeDown%1d/codeLeft%1d/codeRight%1d/codeFire1%1d/codeFire2", \
            pl2Keyboard, codeUp, codeDown, codeLeft, codeRight, codeFire1, codeFire2
        popad
        retn


; HookMainKeysCheck
;
; We've hooked into main key check in the game. Add our additional keyboard commands here.
;
global HookMainKeysCheck
HookMainKeysCheck:
        xor  eax, eax
        mov  al, [convertedKey]
        push eax

%ifdef DEBUG
        cmp  al, 'D'
        jnz  .try_fast_replay

        extern ToggleDebugOutput
        call ToggleDebugOutput

.try_fast_replay:
%endif

        cmp  al, 'F'
        jnz  .try_screen_dump

        xor  byte [replayStatus], 4
        mov  byte [rplFrameSkip], 0
        jmp  short .test_player_keys

.try_screen_dump:
        mov  ax, [lastKey]
        cmp  ax, 0x3c           ; f2
        jnz  .test_player_keys

        cmp  byte [aAsave_256], 'Z'
        jbe  .dump_it
        mov  byte [aAsave_256], 'A'

.dump_it:
        calla DumpScreen

.test_player_keys:
        call TestForPlayerKeys
        jz   .skip_key

        pop  eax
%ifndef SENSI_DAYS
        call HandleMPKeys
%endif
        retn

.skip_key:
        pop  eax
        mov  word [lastKey], 0
        xor  eax, eax
        retn


global ShowControlsMenu
ShowControlsMenu:
        mov  dword [A6], controlsMenu
        jmpa ShowMenu


; TestForPlayerKeys
;
; Check if current scan code is used in any player's keyboard controls, and
; return with zero flag set if so.
;
TestForPlayerKeys:
        movzx eax, byte [lastKey]
        test eax, eax
        jz   .out

        push byte 5
        cld
        mov  edi, UP
        pop  ecx
        repnz scasb
        jz   .out

        cmp byte [pl2Keyboard], 0   ; make sure to check if player2 is on keyboard
        jz   .not_found_out

        push byte 6
        mov  edi, pl2Codes
        pop  ecx
        repnz scasb

.out:
        retn

.not_found_out:
        or   al, -1
        retn


; ControlsMenuInit
;
; Called every time controls menu is entered. Sets entry that corresponds to
; controls configuration to red.
;
ControlsMenuInit:
        push byte 6
        pop  ecx                    ; ecx = counter, 6 entries
        push byte 2
        pop  edi                    ; edi = starting entry number

.set_green_loop:
        mov  [D0], edi
        inc  edi
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  word [esi + MenuEntry.backAndFrameColor], 14 ; set all entries to same color
        dec  ecx
        jnz  .set_green_loop

        cmp  byte [pl2Keyboard], 0  ; check if we have second pl. on keyboard
        jz   .not_pl2_keyboard

        push byte 7                 ; it is entry 7
        jmp  short .set_entry_color

.not_pl2_keyboard:
        mov  ax, [g_joyKbdWord]     ; else use g_joyKbdWord as entry index
        inc  eax                    ; entries start from 2 and are zero based
        push eax

.set_entry_color:
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  word [esi + MenuEntry.backAndFrameColor], 13 ; set selected entry color
        retn


; ControlsOnSelectCommon
;
; Called when some entry is selected. Does all the work with controls changing and
; calibrating.
;
ControlsOnSelectCommon:
        mov  esi, [A5]              ; esi -> this entry
        xor  ebx, ebx
        mov  bx, [esi + MenuEntry.ordinal] ; bx = this entry ordinal
        dec  ebx
        dec  ebx
        mov  ecx, ebx               ; ecx = controls index zero based
        cmp  ecx, 5                 ; keyboard + keyboard?
        jz   .2pl_kbd

        xor  eax, eax
        call SetSecondPlayerOnKeyboard
        jmp  short .inc_ordinal

.2pl_kbd:
        sub  ecx, byte 2            ; make it look like keyboard + joystick

.inc_ordinal:
        inc  ecx                    ; g_joyKbdWord starts from 1, increase ordinal
        mov  [g_joyKbdWord], cx
        or   ecx, byte -1           ; ecx will be player number, init to -1
        xor  eax, eax               ; clear eax so that we get zero when shifting
        mov  ax, [calibrateValues + 2 * ebx] ; get control code
        xor  esi, esi               ; esi will mark joystick number to calibrate
        xor  edi, edi               ; edi will mark keyboard number to redefine

.next_control:
        mov  word [g_scanCode], 0   ; reset last scan code to zero
        inc  ecx                    ; increase player number
        mov  dl, al                 ; get control code
        shr  eax, 8
        test dl, dl
        jz   .clear_controls_out    ; if zero, no more controls

        cmp  dl, 1
        mov  ebx, kbdVars
        jnz  .not_kbd

        mov  ebx, [ebx + 4 * edi]   ; set ebx -> vars to set
        call RedefineKeys           ; keyboard
        test edi, edi
        jz   .keyboard1

        push eax
        or   eax, -1                ; patch code for second keyboard player
        call SetSecondPlayerOnKeyboard
        pop  eax

.keyboard1:
        inc  edi                    ; next time it will be second keyboard
        jmp  short .next_control

.not_kbd:
        call CalibrateJoystick      ; joystick
        jc   .out                   ; check if error

        inc  esi                    ; next time it will be joystick 2
        jmp  short .next_control

.clear_controls_out:
        push byte 105               ; save changes to setup.dat
        mov  dword [A0], aSetup_dat
        mov  dword [A1], setupDatBuffer
        pop  dword [D1]
        calla WriteFile
        call SaveOptionsIfNeeded

.out:
        xor  eax, eax
        mov  [controlWord], ax      ; reset all control variables because int9 could happen
        mov  [g_joy1Status], ax     ; in the middle of this function, and variables could be
        mov  [g_joy2Status], ax     ; erroneously set before new scan codes
        call ControlsMenuInit       ; set color of newly selected entry
        WriteToLog "New controls set, g_joyKbdWord = %hd", dword [g_joyKbdWord]
        WriteToLog "numLoopsJoy1 = %hd, numLoopsJoy2 = %hd", g_numLoopsJoy1, g_numLoopsJoy2
        retn


; Player2KeyProc
;
; When patched in, will be called directly from keyboard handler, from
; SetControlWord. Updates second player control bitfields (joy2Status).
;
Player2KeyProc:
        push eax                ; ax contains scan code
        movzx ebx, word [g_joy2Status]
        test al, 0x80           ; if high bit is set, key was released
        jnz  .key_released

        or   ebx, 0x80          ; hi bit of status is 1 if any key was pressed
        cmp  al, [codeFire1]
        jnz  .not_fire1

        or   ebx, 0x20

.not_fire1:
        cmp  al, [codeFire2]
        jnz  .not_fire2

        or   ebx, 0x40

.not_fire2:
        cmp  al, [codeRight]
        jnz  .not_right

        or   ebx, 8
        and  ebx, ~4

.not_right:
        cmp  al, [codeLeft]
        jnz  .not_left

        or   ebx, 4
        and  ebx, ~8

.not_left:
        cmp  al, [codeDown]
        jnz  .not_down

        or   ebx, 2
        and  ebx, ~1

.not_down:
        cmp  al, [codeUp]
        jnz  .return

        or   ebx, 1
        and  ebx, ~2
        jmp  short .return

.key_released:
        and  al, 0x7f           ; get rid of high bit and see what was released
        and  ebx, 0x7f          ; also clear status high bit - key released
        cmp  al, [codeFire1]
        jnz  .not_fire1_rel

        and  ebx, ~32

.not_fire1_rel:
        cmp  al, [codeFire2]
        jnz  .not_fire2_rel

        and  ebx, ~64

.not_fire2_rel:
        cmp  al, [codeRight]
        jnz  .not_right_rel

        and  ebx, ~8

.not_right_rel:
        cmp  al, [codeLeft]
        jnz  .not_left_rel

        and  ebx, ~4

.not_left_rel:
        cmp  al, [codeDown]
        jnz  .not_down_rel

        and  ebx, ~2

.not_down_rel:
        cmp  al, [codeUp]
        jnz  .return

        and  ebx, ~1

.return:
        mov  [g_joy2Status], bx
        mov  bx, [controlWord]
        pop  eax                ; restore scan code
        retn


; SetSecondPlayerOnKeyboard
;
; in:
;      al - keyboard 2 on or off (boolean)
;
; Turns support for second player on keyboard on or off.
;
global SetSecondPlayerOnKeyboard
SetSecondPlayerOnKeyboard:
        push esi
        push edi
        push ecx

        mov  ecx, eax
        mov  edx, DummyInt9             ; install dummy int9 handler, to exclude possibility
        or   eax, byte -1               ; of keyboard handler running while we are changing it
        call InstallKeyboardHandler
        mov  esi, PatchSetControlWord
        mov  edi, SetControlWord + 6

        test cl, cl
        jz   .patch_it_back

        mov  byte [pl2Keyboard], 1
        push byte 7
        mov  byte [Joy2SetStatus], 0xc3
        mov  byte [Player1StatusProc + 8], 0x74
        mov  byte [Player2StatusProc + 8], 0xeb
        pop  ecx
        rep  movsb
.out:
        sti                             ; restore interrupts
        xor  eax, eax
        call InstallKeyboardHandler     ; return old keyboard handler
        pop  ecx
        pop  edi
        pop  esi
        retn

.patch_it_back:                         ; restore as it was
        mov  byte [pl2Keyboard], 0
        push byte 7
        mov  byte [Joy2SetStatus], 0x66
        mov  byte [Player1StatusProc + 8], 0x75
        mov  byte [Player2StatusProc + 8], 0x74
        pop  ecx
        mov  esi, UnpatchSetControlWord
        rep  movsb
        jmp  short .out

PatchSetControlWord:                    ; just a data to patch
        calla Player2KeyProc, ebx

UnpatchSetControlWord:
        mov  bx, [controlWord]


; RedefineKeys
;
; in:
;      ebx -> 5 + player number bytes array to store key codes
;      ecx -  player number, 0 or 1
;      edi -  keyboard number, 0 or 1
;
; Redefine keys for specified player. Draws menu.
;
RedefineKeys:
        push eax
        push ecx
        push edx
        push esi
        push edi                ; edi must be pushed last
        mov  edx, edi
        add  edx, byte 5        ; edx = max. number of selected keys

.redefine_keys:
        xor  esi, esi           ; esi = current number of selected keys
        mov  word [g_scanCode], 0

.next_key:
        cmp  esi, edx
        ja   .ask_redefine

        pop  edi                ; refresh edi
        push edi
        call DrawSelectKeysMenu
        xor  edi, edi           ; edi = counter for below loop
        mov  word [D2], 60
        mov  word [D3], 2

.draw_selected_keys:
        cmp  edi, esi
        jz   .check_flip

        mov  al, [ebx + edi]
        push edx
        call ScanCode2String
        mov  word [D1], 137
        add  word [D2], 10
        mov  dword [A1], smallCharsTable
        mov  [A0], edx
        pushad
        calla DrawMenuText      ; draw selected key
        popad
        pop  edx
        inc  edi
        jmp  short .draw_selected_keys

.check_flip:
        cmp  esi, edx
        jae  .flip_it

        mov  eax, edi
        shl  edi, 3
        shl  eax, 1
        add  edi, eax
        add  edi, byte 70       ; edi = 70 + 10 * edi
        mov  word [D0], 44
        mov  word [D1], 135
        mov  [D2], edi          ; y = edi
        pushad
        calla SWOS_DrawSprite   ; draw block
        popad

.flip_it:
        pushad
        calla FlipInMenu        ; update screen
        popad
        cmp  esi, edx
        jae  .dont_get_key

        call GetScanCode
        pop  edi                ; we need keyboard number for IsScanCodeValid
        push edi
        call IsScanCodeValid
        jc   .next_key

        mov  [ebx + esi], al

.dont_get_key:
        inc  esi
        jmp  .next_key

.ask_redefine:
        mov  word [D1], 160
        mov  word [D2], 140
        mov  word [D3], 2
        mov  dword [A0], redefineStr
        mov  dword [A1], smallCharsTable
        pushad
        calla DrawMenuTextCentered
        calla FlipInMenu
        popad

.get_key:
        mov   al, [g_scanCode]
        cmp   al, 0x31          ; 'n'
        jz    .redefine_keys

        cmp   al, 0x15          ; 'y'
        jnz   .get_key

.out:
        pop  edi
        pop  esi
        pop  edx
        pop  ecx
        pop  eax
        retn


; IsScanCodeValid
;
; in:
;      al  - scan code to check
;      esi - number of scan codes in table
;      edi - keyboard number (0 or 1)
; out:
;      carry flag: set = scan code invalid, clear = OK
;
; Checks kbdVars array if scan code has been used before.
;
IsScanCodeValid:
        push ecx
        push edi
        push ebx
        push eax

        mov  ecx, esi
        xor  ebx, ebx
        test edi, edi
        jnz  .keyboard2

        inc  edi                ; clear zero flag in case ecx = 0
        mov  edi, [kbdVars]
        repnz scasb
        setnz al
        jmp  short .out

.keyboard2:
        mov  edi, [kbdVars + 4] ; zero flag already clear because edi != 0
        repnz scasb
        setnz bl
        inc  edi                ; clear zero flag in case ecx = 0
        push byte 5
        mov  edi, [kbdVars]
        pop  ecx
        repnz scasb
        setnz al
        and  al, bl

.out:
        cmp  al, 1
        pop  eax
        pop  ebx
        pop  edi
        pop  ecx
        retn


; DrawSelectKeysMenu
;
; in:
;      ecx - player number (0 or 1)
;      edi - keyboard number (0 or 1)
;
; Draws menu for selecting keys.
;
DrawSelectKeysMenu:
        pushad
        push edi
        add  cl, '1'
        mov  [aSelectKeysForPlayer + 23], cl
        mov  ecx, edx
        mov  esi, [linAdr384k]
        mov  edi, esi
        add  esi, 131072
        mov  ecx, 16000
        rep  movsd              ; draw background
        mov  edx, smallCharsTable
        mov  word [D1], 160
        mov  word [D2], 30
        mov  word [D3], 2
        mov  dword [A0], aSelectKeysForPlayer
        mov  [A1], edx
        calla DrawMenuTextCentered
        mov  word [D1], 82
        add  word [D2], 40
        mov  dword [A0], aUp
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], aDown
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], aLeft
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], aRight
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], aFire
        mov  [A1], edx
        calla DrawMenuText
        pop  ecx
        test ecx, ecx
        jz   .out

        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], aBench
        mov  [A1], edx
        calla DrawMenuText

.out:
        popad
        retn


; GetScanCode
;
; out:
;      al - scan code of next pressed key
;
; Waits for a keypress, and returns its scan code in al.
;
GetScanCode:
        push ebx
        push ecx
        push edx
        mov  ecx, 1000

.get_code:
        push ecx
        or   eax, byte -1
        mov  edx, KeyboardHandler
        call InstallKeyboardHandler
        mov  byte [g_scanCode], 0

.get_code_loop:
        mov  al, [g_scanCode]
        or   al, al
        jz   .get_code_loop

.key_up_loop:
        or   al, al
        jge  .return_old_handler

        mov  al, [g_scanCode]
        jmp  short .key_up_loop

.return_old_handler:
        push eax
        xor  eax, eax           ; uninstall keyboard handler
        call InstallKeyboardHandler
        pop  eax
        pop  ecx
        cmp  al, 0x2a
        jnz  .out

        dec  ecx
        jnz  .get_code
.out:
        pop  edx
        pop  ecx
        pop  ebx
        retn


; KeyboardHandler
;
; out:
;      g_scanCode - last pressed key scan code
;
; Reads a scan code from keyboard controller in response to keyboard interrupt
; and puts it into g_scanCode variable.
;
KeyboardHandler:
        push eax
        push ecx
        cli
        in   al, 0x60
        mov  ah, al
        in   al, 0x61
        or   al, 0x80           ; disable keyboard
        out  0x61, al
        mov  [g_scanCode], ah
        mov  al, 0x20
        out  0x20, al
        sti
        pop ecx
        pop eax

DummyInt9:                      ; to be sure when patching SWOS int9 handler
        iret


; ScanCode2String
;
; in:
;      al - scan code
; out:
;      edx -> string from scan code
;
; Converts scan code to printable string.
;
ScanCode2String:
        push esi
        push ebx
        mov  esi, keyTable
        xor  ebx, ebx
        mov  bl, al

.search:
        lodsb
        cmp  al, 0xff
        jz   .not_found

        cmp  al, bl
        jz   .found_it

        xor  eax, eax
        lodsb
        add  esi, eax
        jmp  .search

.found_it:                          ; got it!
        lodsb                       ; skip length byte
        mov  edx, esi
        jmp  short .out

.not_found:
        sub  bl, 2
        mov  al, bl
        mov  esi, oneCharTable
        and  al, 0x7f
        cmp  al, oneCharTableSize
        ja   .unknown

        mov  al, [esi + ebx]
        test al, al
        jz   .unknown

        mov  [keyStrBuf], al
        mov  byte [keyStrBuf + 1], 0
        mov  edx, keyStrBuf
        jmp  short .out

.unknown:
        mov  edx, aUnknownKey       ; don't know this scan code

.out:
        pop  ebx
        pop  esi
        retn


; VerifyJoysticks
;
; Test joysticks for disconnection, and if any is found update g_joyKbdWord not to use it.
;
global VerifyJoysticks
VerifyJoysticks:
        xor  ebx, ebx
        call ReadJoystickValues
        test ebp, ebp
        jz   .out

        cmp ebp, 3                          ; joystick timeout, check is it just one or both
        jb  .at_least_one_joystick_alive

        WriteToLog "Both joysticks disconnected, setting controls to keyboard only"
        mov  word [g_joyKbdWord], 1
        retn

.at_least_one_joystick_alive:
        cmp  word [g_joyKbdWord], 5
        jnz  .out

        WriteToLog "One joystick still connected, setting controls to keyboard+joystick"
        dec  word [g_joyKbdWord]

.out:
        retn


; CheckIfJoystickDisconnected
;
; Prevent controls locking in case any joystick has been disconnected, and fix g_joyKbdWord accordingly.
; If down+right combination was running continuously for 3 seconds do full test for disconnection.
; Do the test at most once per second. This is executed each frame while in menus.
;
declareStatic m_joy1BottomRightFrames, 1
declareStatic m_joy2BottomRightFrames, 1
declareStatic m_lastJoystickTestTick, 2
kJoystickCheckTimeout equ 3 * 70

global CheckIfJoystickDisconnected
CheckIfJoystickDisconnected:
        cmp  word [g_joyKbdWord], 1
        jz   .out

        mov  ax, [g_currentTick]
        sub  ax, [m_lastJoystickTestTick]
        cmp  ax, 70                     ; run at most once per second
        jl   .out

        mov  ax, [g_joy1X]              ; check if we had maximum bottom right joystick values last frame
        cmp  ax, [g_joy1MaxX]
        jb   .reset_joy1_timer

        mov  ax, [g_joy1Y]
        cmp  ax, [g_joy1MaxY]
        jb   .reset_joy1_timer

        inc  byte [m_joy1BottomRightFrames]     ; increase consecutive timer and test if past 3 seconds
        cmp  byte [m_joy1BottomRightFrames], kJoystickCheckTimeout
        jb   .test_joy2

.test_if_connected:
        call VerifyJoysticks
        mov  ax, [g_currentTick]
        mov  [m_lastJoystickTestTick], ax
        xor  eax, eax
        mov  [m_joy1BottomRightFrames], al
        mov  [m_joy2BottomRightFrames], al
        retn

.reset_joy1_timer:
        mov  byte [m_joy1BottomRightFrames], 0

.test_joy2:
        mov  ax, [g_joy2X]
        cmp  ax, [g_joy2MaxX]
        jb   .reset_joy2_timer

        mov  ax, [g_joy2Y]
        cmp  ax, [g_joy2MaxY]
        jb   .reset_joy2_timer

        inc  byte [m_joy2BottomRightFrames]
        cmp  byte [m_joy2BottomRightFrames], kJoystickCheckTimeout
        jae  .test_if_connected

        jmp  .out

.reset_joy2_timer:
        mov  byte [m_joy2BottomRightFrames], 0

.out:
        retn


defaultValues:
        dw 127, 127     ; center values
        dd 0
        dw 1, 1         ; minimum values
        dw 254, 254     ; maximum values
        dw 126, 127     ; divisor for left/right values
        dw 126, 127     ; divisor for top/bottom values
defaultValuesLength equ $ - defaultValues

; SetDefaultCalibrationValues
;
; Apply default calibration values to SWOS that work well with DOSBox.
;
extern memcpy
SetDefaultCalibrationValues:
        push byte defaultValuesLength
        pop  ecx
        mov  eax, g_joy1CenterX
        mov  edx, defaultValues
        push edx
        push ecx
        call memcpy
        pop  ecx
        pop  edx
        mov  eax, g_joy2CenterX
        call memcpy
        mov  dword [g_numLoopsJoy1], 0xfe00fe00
        mov  dword [g_joy1Fire1], 0xfefffeff        ; set both joystick fire buttons to -1 and -2 respectively
        retn


; CalibrateJoystick
;
; in:
;      ecx - player number (0 or 1)
;      esi - joystick number (0 or 1) to calibrate
; out:
;      carry flag: set = error, clear = OK
;
; Draws menu and calibrates joystick. Joystick number decides which joystick are we calibrating.
; Player number is only for display. Doesn't change any of the registers.
;
extern GetCalibrateJoysticksOption, DOSBoxDetected
CalibrateJoystick:
        push eax
        push edi
        push esi                                ; esi must be pushed one before last
        push ecx                                ; ecx must be pushed last

        call GetCalibrateJoysticksOption
        test al, al
        jnz  .recalibrate

        call ReadJoystickValues
        call CheckJoystickTimeout
        jc   .joy_error                         ; even if we're not calibrating still check if a joystick is connected

        call DOSBoxDetected
        test eax, eax
        jz   .out

        call SetDefaultCalibrationValues
        jmp  .out

.recalibrate:
        mov  word [g_scanCode], 0
        mov  bl, [joyButtonsMask + esi]
        push byte 4
        mov  esi, joyCalibrationValues + 8
        pop  ecx                                ; skipping 4 values

.init_skip_values_loop:
        or   dword [esi], 0x8000                ; skip all values except first two
        add  esi, byte 4
        dec  ecx
        jnz  .init_skip_values_loop

        mov  eax, joyCalibrationValues
        sub  esi, byte 3 * 8
        and  dword [esi], ~0x8000               ; mark first two values for reading - they will be read now
        and  dword [esi + 4], ~0x8000
        mov  edx, smallCharsTable
        call ClearJoystickButtons               ; get rid of possible previous button press
        add  esi, 8                             ; esi -> current calibration value
        xor  edx, edx                           ; current value ordinal
        mov  edi, calibrationTitleStrings

.calibrate_axis_input_loop:
        call ReadJoystickValuesForCalibration
        call CheckJoystickTimeout
        jc   .joy_error

        call SetupJoystickMenuValues
        mov  eax, [edi]
        call DrawJoystickAxisCalibrationMenu
        mov  al, [joyButtons]
        and  al, bl
        jz   .calibrate_axis_input_loop

        call ClearJoystickButtons
        add  edi, 4
        and  dword [esi], ~0x8000
        and  dword [esi + 4], ~0x8000
        add  esi, 8
        inc  edx
        cmp  edx, 3
        jb   .calibrate_axis_input_loop

        mov  ecx, [esp + 4]                     ; get joystick number
        lea  esi, [joyButtonPointers + 8 * ecx] ; select joy 1 or 2
        xor  edx, edx                           ; button index

.calibrate_buttons_loop:
        call ReadJoystickValuesForCalibration
        call CheckJoystickTimeout
        jc   .joy_error

        call PromptForJoystickButton
        test al, al
        jz   .calibrate_buttons_loop

.save_button:
        mov  edi, [esi + 4 * edx]               ; select button 1 or 2
        mov  [edi], al                          ; write selected button value

        inc  edx
        call ClearJoystickButtons
        cmp  edx, byte 2
        jl   .calibrate_buttons_loop

        mov  ecx, [esp + 4]
        mov  esi, [joyVars + 4 * ecx]           ; write out divisors and number of loops
        call ApplyCalibrationValues
        call ClearJoystickButtons               ; get rid of left over button presses
        pop  ecx
        push ecx
        call DrawJoystickCalibrationMenuBase    ; get rid of "press special fire" message
        mov  word [D1], 160
        mov  word [D2], 50
        mov  word [D3], 2
        mov  dword [A0], aCalibrateAgain
        mov  edx, smallCharsTable
        mov  [A1], edx
        calla DrawMenuTextCentered              ; ask user if (s)he wishes to do calibration again
        calla FlipInMenu
        mov  esi, [esp + 4]                     ; restore esi for another possible loop

.get_key:
        mov  al, [g_scanCode]                   ; get answer to recalibrate question
        cmp  al, 0x15                           ; 'y'
        jz   .recalibrate
        cmp  al, 0x31                           ; 'n'
        jnz  .get_key
        clc

.out:
        pop  ecx
        pop  esi
        pop  edi
        pop  eax
        retn

.joy_error:
        mov  word [g_joyKbdWord], 1             ; set to keyboard only
        cmp  word [g_scanCode], 0x01            ; if user pressed escape, than there
        stc                                     ; is no need for showing error menu
        jz   .out

        mov  dword [A0], aJoystickError
        calla ShowErrorMenu
        stc
        jmp  short .out


; SetupJoystickMenuValues
;
; in:
;     [esp + 4] - joystick number
;     edx       - offset in joyCalibrationValues of variables to set:
;                     x   |   y    | value
;                 --------+--------+-------
;                  left   | up     |   0
;                  right  | down   |   1
;                  center | center |   2
;
SetupJoystickMenuValues:
        mov  ecx, [esp + 4]             ; get joystick number
        push esi
        shl  ecx, 2
        mov  ebp, [joyVars + ecx]       ; ebp -> variable pointer list for a given joystick
        mov  ax, [joyXRead + ecx]
        mov  [joyCalibrationValues + 2 + 8 * edx], ax
        mov  esi, [ebp + 4 * edx]
        mov  [esi], ax
        mov  ax, [joyYRead + ecx]
        mov  [joyCalibrationValues + 6 + 8 * edx], ax
        mov  esi, [ebp + 4 * edx + 4]
        mov  [esi], ax
        pop  esi
        retn


; CheckJoystickTimeout
;
; in:
;      ebp       - timeout flags:
;                    bit 0 set: joystick 1 timed out
;                    bit 1 set: joystick 2 timed out
;      [esp + 8] - joystick number, 0 or 1
; out:
;      carry flag: set if error, clear if OK
;
; Helper routine of CalibrateJoystick. Checks if given joystick readings have
; resulted with a timeout, or escape was pressed. Sets carry flag if any of it happened.
;
CheckJoystickTimeout:
        cmp  word [g_scanCode], 0x01
        jnz  .check_timeout

        stc
        retn

.check_timeout:
        push ecx
        mov  ecx, [esp + 8]
        inc  ecx
        shr  ebp, cl            ; shift timed-out bit into carry
        pop  ecx
        retn


; ApplyCalibrationValues
;
; in:
;     esi - pointer into joyCalibrationValues
;
; Applies values obtained from calibration to SWOS.
;
ApplyCalibrationValues:
        mov  ebp, [esi + joyXCenterPtr]
        mov  ax, [joyCalibrationXCenter]
        mov  [ebp], ax
        mov  ebp, [esi + joyXLeftPtr]
        mov  bx, [joyCalibrationXLeft]
        mov  [ebp], bx
        sub  ax, bx
        mov  ebp, [esi + joyXLeftDivPtr]
        mov  [ebp], ax

        mov  ebp, [esi + joyXRightPtr]
        mov  ax, [joyCalibrationXRight]
        mov  [ebp], ax
        mov  ebp, [esi + joyXRightDivPtr]
        sub  ax, [joyCalibrationXCenter]
        mov  [ebp], ax

        mov  ebp, [esi + joyYCenterPtr]
        mov  ax, [joyCalibrationYCenter]
        mov  [ebp], ax
        mov  ebp, [esi + joyYUpPtr]
        mov  bx, [joyCalibrationYUp]
        mov  [ebp], bx
        sub  ax, bx
        mov  ebp, [esi + joyYTopDivPtr]
        mov  [ebp], ax

        mov  ebp, [esi + joyYDownPtr]
        mov  ax, [joyCalibrationYDown]
        mov  [ebp], ax
        mov  ebp, [esi + joyYBottomDivPtr]
        sub  ax, [joyCalibrationYCenter]
        mov  [ebp], ax

        mov  ebp, [esi + joyNumLoopsPtr]
        xor  eax, eax
        xor  ecx, ecx
        mov  ax, [joyCalibrationYDown]
        mov  cx, [joyCalibrationXRight]
        sub  eax, ecx
        sbb  ebx, ebx                   ; ebx = eax < ecx ? -1 : 0
        neg  ebx                        ; ebx = eax < ecx ? 0 : -1
        and  ebx, eax                   ; ebx = eax < ecx ? 0 : eax - ecx
        add  ecx, ebx                   ; ecx = eax < ecx ? ecx : eax
        mov  [ebp], cx                  ; cx = max(ax, bx) for number of loops
        retn


; DrawJoystickAxisCalibrationMenu
;
; in:
;      eax       -> menu title string
;      [esp + 4] -  player number
;
; Draws joystick calibration menu and flips it to the screen.
;
DrawJoystickAxisCalibrationMenu:
        push ebx
        push esi
        push edi
        push edx
        push eax
        mov  ecx, [esp + 24]
        call DrawJoystickCalibrationMenuBase
        mov  word [D1], 160
        mov  word [D2], 55
        mov  word [D3], 13
        mov  edx, smallCharsTable
        pop  dword [A0]
        mov  [A1], edx
        calla DrawMenuTextCentered
        mov  word [D1], 160
        mov  word [D2], 67
        mov  eax, aPressEscapeToCancel
        mov  dword [A0], eax
        mov  [A1], edx
        calla DrawMenuTextCentered
        calla FlipInMenu
        pop  edx
        pop  edi
        pop  esi
        pop  ebx
        retn


; DrawJoystickCalibrationMenuBase
;
; in:
;      ecx - player number (0 or 1)
;
; Draws joystick calibration menu main part, drawing values for extreme readings
; depending on joyCalibrationValues table. Does not do the flip.
;
kCalibrationMenuBaseY equ 82
DrawJoystickCalibrationMenuBase:
        push ecx
        push ebx
        push edx

        add  cl, '1'
        mov  byte [joyMenuTitle + 30], cl
        mov  esi, [linAdr384k]
        mov  edi, esi
        add  esi, 131072
        mov  ecx, 16000
        rep  movsd                  ; draw background
        mov  edx, smallCharsTable
        mov  word [D1], 160
        mov  word [D2], 30
        mov  word [D3], 2
        mov  dword [A0], joyMenuTitle
        mov  [A1], edx
        calla DrawMenuTextCentered
        push byte 82
        xor  eax, eax
        mov  esi, joyLeftStrings    ; first draw left strings
        pop  edi

.prepare_loop:
        push byte 3
        mov  word [D2], kCalibrationMenuBaseY
        pop  ecx

.draw_strings_loop:
        push eax
        push esi
        push edi
        push ecx
        add  word [D2], 10
        mov  eax, [esi]
        mov  dword [A0], eax
        mov  [A1], edx
        mov  [D1], edi
        calla DrawMenuText
        pop  ecx
        pop  edi
        pop  esi
        pop  eax
        add  esi, 4
        dec  ecx
        jnz  .draw_strings_loop

        test eax, eax
        jnz  .write_joy_values

        inc  eax
        mov  esi, joyRightStrings ; then draw right strings
        mov  edi, 162
        jmp  short .prepare_loop

.write_joy_values:
        push byte 6
        mov  esi, joyCalibrationValues
        pop  ecx

.joy_values_loop:
        mov  eax, [esi]
        test ax, ax             ; check if value defined (lo word hi bit reset)
        js   .next

        push ecx
        mov  ebx, eax
        mov  ecx, eax
        shr  ebx, 8
        and  ebx, 0x7f          ; ebx = y
        and  ecx, 0xff          ; ecx = x
        shr  eax, 16            ; eax = read value
        mov  [D1], ecx          ; set D1 here since GCC will modify ecx
        mov  [D2], ebx
        call int2str            ; convert to string
        mov  word [D3], 2
        mov  [A0], eax
        mov  dword [A1], smallCharsTable
        pushad
        calla DrawMenuText
        popad
        pop  ecx

.next:
        add  esi, byte 4
        dec  ecx
        jnz  .joy_values_loop

        pop  edx
        pop  ebx
        pop  ecx
        retn


; ReadJoystickValuesForCalibration
;
; out:
;      ebp - bit 0 clear, bit 1 set if joystick 2 timed out, 0 if no error
;
; Reads current joystick values and writes them to joy?Read array, but only if
; the readings are valid.
;
ReadJoystickValuesForCalibration:
        push ebx
        push ecx
        push edx
        push esi
        push edi

        mov  bl, 1                  ; full loop range
        call ReadJoystickValues
        cmp  ebp, 3
        jz   .out

        test ebp, 1
        jnz   .test_joy2

        mov  [joyXRead], ax
        mov  [joyYRead], bx

.test_joy2:
        test ebp, 2
        jnz  .set_buttons

        mov  [joyXRead + 4], cx
        mov  [joyYRead + 4], dx

.set_buttons:
        mov  [joyButtons], si

.out:
        pop  edi
        pop  esi
        pop  edx
        pop  ecx
        pop  ebx
        retn


; ReadGamePort2
;
; in:
;     ebx - expected to be zero
;
; Get joystick readings and update SWOS internal variables. Meant to be used as a
; replacement for SWOS joystick reading routine.
;
global ReadGamePort2
ReadGamePort2:
        call ReadJoystickValues
        mov  [g_joy1X], ax
        mov  [g_joy1Y], bx
        mov  [g_joy2X], cx
        mov  [g_joy2Y], dx
        retn


; CheckDOSBoxJoystick2Values
;
; in:
;      ax  - joy1 x
;      bx  - joy1 y
;      cx  - joy2 x
;      dx  - joy2 y
;      si  - buttons bitfield
; out:
;      input unchanged, except for cx and dx which might get corrected
;      ebp - bit 0 clear, bit 1 set if joystick 2 timed out, 0 if no error
;      carry flag: set = error, clear = OK
;
; If second joystick is disconnected but first isn't, DOSBox will not set carry flag and
; will return some bogus values. Luckily those values are unique and we can detect them --
; basically any value greater than zero outside of positive byte range means second joystick
; is off. Bad thing is that DOSBox only updates joystick connected state at startup, so if the
; joystick is disconnected later we will have no way of detecting it. It will keep returning
; last values it had, and carry flag would be clear. Same goes for first joystick.
;
CheckDOSBoxJoystick2Values:
        push eax
        call DOSBoxDetected
        test eax, eax
        pop  eax
        jz   .out_ok

        cmp  cx, 127
        ja   .joy2_failure

        test cx, cx
        jz   .joy2_failure

        cmp  dx, 127
        ja   .joy2_failure

        test dx, dx
        jz   .joy2_failure

.out_ok:
        xor  ebp, ebp       ; all seems in order here
        retn

.joy2_failure:
        push 2
        pop  ebp
        and  esi, ~1100b
        retn


; ReadJoystickValues
;
; in:
;      ebx - if zero use number of loops values from calibration, otherwise go through full 16-bit range
;            (ignored if using BIOS routines)
; out:
;      ax  - joy1 x
;      bx  - joy1 y
;      cx  - joy2 x
;      dx  - joy2 y
;      si  - buttons bitfield, in lower nibble
;      ebp - bit 0 set if joystick 1 timed out, bit 1 set if joystick 2 timed out, everything 0 if no error
;            (if we're using BIOS both bits will be set if error, we can't differentiate between them,
;             although we handle some cases with DOSBox)
;
; Refresh joystick readings for both joysticks x/y axis. Use BIOS or read from
; the port directly depending on the option set.
;
extern GetUseBIOSJoystickRoutineOption
ReadJoystickValues:
        call GetUseBIOSJoystickRoutineOption
        test al, al
        jz   ReadGamePort

        xor  ebp, ebp
        xor  edx, edx               ; get buttons first
        mov  ah, 0x84
        push eax
        int  0x15
        mov  esi, eax
        pop  eax
        jc   .err_out

        inc  edx                    ; get values, this works so good on DOSBox :)
        int  0x15
        jc   .err_out               ; carry will be set only if both joysticks are disconnected

        not  esi
        shr  esi, 4                 ; place button bits into lower nibble
        and  esi, 0x0f              ; remove axis values
        jmp  CheckDOSBoxJoystick2Values

.err_out:
        mov  ax, [g_joy1CenterX]
        mov  bx, [g_joy1CenterY]
        mov  cx, [g_joy2CenterX]
        mov  dx, [g_joy2CenterY]
        xor  esi, esi
        mov  ebp, 11b               ; sadly we don't know which joystick was disconnected
        retn


; ReadGamePort
;
; in:
;      ebx - if zero use number of loops values from calibration, otherwise go through full 16-bit range
; out:
;      ax  - joy1 x
;      bx  - joy1 y
;      cx  - joy2 x
;      dx  - joy2 y
;      si  - buttons bitfield
;      ebp - bit 0 set if joystick 1 timed out, bit 1 set if joystick 2 timed out, 0 if no timeouts
;
; Replacement for SWOS' game port reading routine. Return center values in case joystick was disconnected.
;
ReadGamePort:
        xor  ecx, ecx
        dec  cx
        test ebx, ebx
        jnz  .init_poll_loop

        movzx ecx, word [g_numLoopsJoy2]
        cmp  word [g_joyKbdWord], 5     ; two joysticks
        jz   .init_poll_loop

        movzx ecx, word [g_numLoopsJoy1]

.init_poll_loop:
        mov  ah, 1                  ; do one "fake" loop, to make sure that the monostable vibrators have stabilized
        xor  esi, esi               ; it also helps ensure entire loop is in the cache
        xor  ebx, ebx
        xor  edi, edi
        xor  ebp, ebp
        test ah, ah
        mov  dx, 0x201              ; game port
        cli
        jnz  .poll_loop

.trigger_the_timer:
        out  dx, al                 ; trigger the timer chip

        jmp  $ + 2
        jmp  $ + 2

.poll_loop:
        in   al, dx
        test al, 0x0f
        jz   .poll_done             ; once the counters hit zero we're done

        shr  al, 1
        adc  esi, 0
        shr  al, 1
        adc  ebx, 0
        shr  al, 1
        adc  edi, 0
        shr  al, 1
        adc  ebp, 0
        dec  ecx                    ; do not use loop instruction here, because if ecx
        jns  .poll_loop             ; happens to be zero we might get into 2^32 loop :<

        shl  al, 4                  ; put button bits back to hi nibble

.poll_done:
        dec  ah
        jz   .trigger_the_timer

        sti
        not  al                     ; reverse button logic (make it 1 = pressed, 0 = depressed)
        shr  al, 4                  ; return button bits in lower nibble
        mov  edx, ebp
        xor  ebp, ebp               ; ebp will be success flag

        cmp  esi, 0xffff            ; check if joystick was disconnected
        jb   .test_joy2

        mov  si, [g_joy1CenterX]    ; if disconnected set to some neutral values so it doesn't
        mov  bx, [g_joy1CenterY]    ; effectively prevent us from using menus
        and  al, 11111100b
        or   ebp, 1

.test_joy2:
        cmp  edi, 0xffff
        jb   .out

        mov  di, [g_joy2CenterX]
        mov  dx, [g_joy2CenterY]
        and  al, 11110011b
        or   ebp, 2

.out:
        xchg eax, esi
        and  esi, 0xff
        mov  ecx, edi
        retn


; ReadJoystickButtons
;
; out:
;      al - joystick buttons bitfield (bits 4-7) with inverted (proper) logic (1=on, 0=off)
;      carry flag: set = error, clear = OK (although in practice there shouldn't be an error)
;
; Read joystick buttons directly or using BIOS depending on options. Replace SWOS'
; game port access with this routine.
;
global ReadJoystickButtons
ReadJoystickButtons:
        push edx
        call GetUseBIOSJoystickRoutineOption
        test al, al
        jnz  .use_bios

        mov  dx, 0x201
        in   al, dx
        jmp  .out

.use_bios:
        mov  ah, 0x84
        xor  edx, edx
        int  0x15
        jnc  .out

        or   al, -1

.out:
        not  al
        and  al, 0xf0       ; don't shift as SWOS will do it
        pop  edx
        retn


; ClearJoystickButtons
;
; in:
;      bl - joystick button mask
; out:
;      carry flag: set = error, clear = OK
;
; Waits for joystick buttons to be depressed.
;
ClearJoystickButtons:
        call ReadJoystickButtons
        jc   .out

        shr  al, 4
        and  al, bl
        jnz  ClearJoystickButtons

.out:
        mov  byte [joyButtons], 0
        retn


; PromptForJoystickButton
;
; in:
;       bl - joystick buttons mask
;      ecx - joystick number (0 or 1)
;      edx - button number (0 or 1)
; out:
;      eax - button pressed: 0 = none, -1 = button 1, -2 = button 2
;
; Asks user for joystick buttons presses and converts them into negative button
; indices we store them as. Also draw "press ... button" string.
;
PromptForJoystickButton:
        push edx
        push ecx
        push esi
        call DrawJoystickCalibrationMenuBase    ; first redraw without the title
        mov  word [D1], 160
        mov  word [D2], 130
        mov  word [D3], 13
        mov  dword [A0], aPressNormalFire
        test edx, edx
        jz   .normal_fire

        mov  dword [A0], aPressSpecialFire

.normal_fire:
        mov  dword [A1], smallCharsTable
        push edx
        push ecx
        push ebx
        push esi
        calla DrawMenuTextCentered
        pop  esi
        pop  ebx
        pop  ecx
        pop  edx
        call ReadJoystickButtons
        shr  al, 4
        and  al, bl             ; mask button bits
        shl  ecx, 1             ; ecx is 0 or 2
        shr  al, cl             ; put button bits into lower two bits
        test al, 1              ; first button?
        jz   .no_button1

        or   eax, -1
        jmp  short .out

.no_button1:
        test al, 2              ; second button?
        jz   .no_button2

        or   eax, -1
        dec  eax
        jmp  short .out

.no_button2:
        xor  eax, eax           ; nothing

.out:
        calla FlipInMenu, edx
        pop  esi
        pop  ecx
        pop  edx
        retn


; InstallKeyboardHandler
;
; in:
;      al  - true = installing, false = deinstalling
;      edx -> keyboard handler to install (if installing)
;
; Installs or uninstalls keyboard handler. Can remember only last keyboard
; handler, do not call multiple times.
;
global InstallKeyboardHandler
InstallKeyboardHandler:
        push ds
        push es
        push ebx
        push eax
        push edx

        cli
        test eax, eax
        jz   .uninstall

        mov  ax, 0x3509
        int  0x21                   ; get old keyboard handler, es:ebx
        mov  [oldInt9Ofs], ebx      ; save it for later
        mov  [oldInt9Seg], es
        mov  ax, cs
        mov  ds, ax
        mov  ax, 0x2509             ; now setup new one
        int  0x21
        jmp  short .out

.uninstall:
        cli
        mov  ax, 0x2509
        mov  edx, [oldInt9Ofs]
        mov  ds, [oldInt9Seg]
        int  0x21

.out:
        sti
        pop  edx
        pop  eax
        pop  ebx
        pop  es
        pop  ds
        retn


; EnableShiftSupport
;
; Patch existing int 9 keyboard handler with support for detecting whether shift
; key is held down or not.
;
global EnableShiftSupport
EnableShiftSupport:
        mov  esi, Int9Patch
        push byte Int9PatchSize
        pop  ecx
        mov  edi, Int9KeyboardHandler + 0x24
        cld
        cli
        rep  movsb
        sti
        retn


CheckShiftCode:
        cmp  al, 0x2a   ; left shift down
        jz   .shift_on

        cmp  al, 0x36   ; right shift down
        jz   .shift_on

        cmp  al, 0xaa   ; left shift up
        jz   .shift_off

        cmp  al, 0xb6   ; right shift up
        jz   .shift_off

.out:
        cmp  al, 0x80
        retn

.shift_on:
        mov  byte [shiftDown], 1
        jmp  .out

.shift_off:
        mov  byte [shiftDown], 0
        jmp  .out


; Int9Patch
;
; Glue for shift support. Reason for this being done as it is (and not through
; patch record), is to be safe and have interrupts off during the patching.
;
Int9Patch:
        calla CheckShiftCode, ebx
        nop
        nop
Int9PatchSize equ $ - Int9Patch


; additional keys FilterKeys will allow
shiftVersions:
    db "'", '(', ')', ',', '-', '.', '*', '?', ':', '+', '%',
additionalKeys:
    db "'", '9', '0', ',', '-', '.', '8', '/', ';', '+', '6', 0

; TestAdditionalKeys
;
; in:
;     ax - ASCII code of a key that is being tested whether to be allowed as input or not
;
; Patch FilterKeys and allow additional keys to go through when typing.
;
TestAdditionalKeys:
        and  eax, 0xffff
        test eax, eax
        jz   .out

        xor  ebx, ebx
        mov  esi, shiftVersions
        mov  edi, additionalKeys
        mov  ebp, [shiftDown]
        and  ebp, 0xff

.next_key:
        mov  bl, [edi]
        mov  cl, [esi]
        test ebx, ebx
        jz   .out

        inc  edi
        inc  esi
        cmp  eax, ebx
        jnz  .next_key

.check_shift:               ; non-shift key matched
        test ebp, ebp
        jz   .accept_key

        mov  eax, ecx       ; convert to shift character

.accept_key:
        mov  [D1], eax      ; set the matched key
        or   eax, eax
        pop  eax            ; return directly to the caller
        retn

.out:                       ; no match return as usual
        cmp  eax, 'A'
        retn


; PatchFilterKeys
;
; in:
;     esi -> bytes to patch in
;
PatchFilterKeys:
        mov  edi, FilterKeys + 0xe
        mov  ecx, FilterKeysPatchSize
        cld
        rep  movsb
        retn


global EnableAdditionalInputKeys
EnableAdditionalInputKeys:
        mov  esi, FilterKeysPatch
        mov  byte [convertKeysTable + 0x0c], "-"
        mov  word [convertKeysTable + 0x27], ";'"     ; patch conversion table too
        mov  dword [convertKeysTable + 0x33], ',' | '.' << 8 | '/' << 16 | 0x16 << 24
        jmp  PatchFilterKeys


global DisableAdditionalInputKeys
DisableAdditionalInputKeys:
        mov  esi, FilterKeysOrigBytes
        mov  byte [convertKeysTable + 0x0c], 0
        mov  word [convertKeysTable + 0x27], 0
        mov  dword [convertKeysTable + 0x33], 0x16 << 24
        jmp  PatchFilterKeys


FilterKeysPatch:
        calla TestAdditionalKeys, ebx
        nop
FilterKeysPatchSize equ $ - FilterKeysPatch


FilterKeysOrigBytes:
        cmp  word [D1], 'A'
FilterKeysOrigBytesSize equ $ - FilterKeysOrigBytes


%if FilterKeysPatchSize != FilterKeysOrigBytesSize
    %error "Patch and original bytes size for FilterKeys patch differ!"
%endif



section .data

    StartMenu controlsMenu, ControlsMenuInit, 0, 0, 2
        ; title
        StartEntry  92, 0, 120, 15
            EntryColor  0x17
            EntryString 0, "CONTROLS MENU"
        EndEntry

        ; exit
        StartEntry 102, 185, 100, 15
            NextEntries -1, -1, 7, -1
            EntryColor  12
            EntryString 0, aExit
            OnSelect    SetExitMenuFlag
        EndEntry

        StartTemplateEntry
        StartEntry
            EntryColor  14
            OnSelect    ControlsOnSelectCommon
        EndEntry

        StartEntry 92, 30, 120, 15
            NextEntries -1, -1, -1, 3
            EntryString 0, "KEYBOARD ONLY"
        EndEntry

        StartEntry 92, 55, 120, 15
            NextEntries -1, -1, 2, 4
            EntryString 0, "JOYSTICK ONLY"
        EndEntry

        StartEntry 92, 80, 120, 15
            NextEntries -1, -1, 3, 5
            EntryString 0, "JOYSTICK + KEYBOARD"
        EndEntry

        StartEntry 92, 105, 120, 15
            NextEntries -1, -1, 4, 6
            EntryString 0, "KEYBOARD + JOYSTICK"
        EndEntry

        StartEntry 92, 130, 120, 15
            NextEntries -1, -1, 5, 7
            EntryString 0, "JOYSTICK + JOYSTICK"
        EndEntry

        StartEntry 92, 155, 120, 15
            NextEntries -1, -1, 6, 1
            EntryString 0, "KEYBOARD + KEYBOARD"
        EndEntry
    EndMenu

; must be successive (7 bytes)
pl2Keyboard:
    db 0
pl2Codes:
codeUp:
    db 0x11 ; 'w'
codeDown:
    db 0x1f ; 's'
codeLeft:
    db 0x1e ; 'a'
codeRight:
    db 0x20 ; 'd'
codeFire1:
    db 0x2a ; left shift
codeFire2:
    db 0x29 ; `

%if codeFire2 - 6 != pl2Keyboard
    %error "Player 2 keyboard variables must be successive!"
%endif

; 0 = nothing
; 1 = set keyboard
; 2 = set joystick
;
; high byte = player 2
; low byte  = player 1
calibrateValues:
    dw 0x0001, 0x0002, 0x0102, 0x0201, 0x0202, 0x0101

aSelectKeysForPlayer:
    db "SELECT KEYS FOR PLAYER 1", 0
aUp:
    db "UP:", 0
aDown:
    db "DOWN:", 0
aLeft:
    db "LEFT:", 0
aRight:
    db "RIGHT:", 0
aFire:
    db "FIRE:", 0
aBench:
    db "BENCH:", 0

; scan codes to strings - first scan code, than length of string
; (+ terminator), and then the string itself
; table ends with 0xff scan code
keyTable:
    db 0x01, 7,  "ESCAPE", 0
    db 0x0f, 4,  "TAB", 0
    db 0x1c, 6,  "ENTER", 0
    db 0x39, 6,  "SPACE", 0
    db 0x48, 9,  "UP ARROW", 0
    db 0x4b, 11, "LEFT ARROW", 0
    db 0x4d, 12, "RIGHT ARROW", 0
    db 0x50, 11, "DOWN ARROW", 0
    db 0x0e, 10, "BACKSPACE", 0
    db 0x3a, 10, "CAPS LOCK", 0
    db 0x3b, 3,  "F1", 0
    db 0x3c, 3,  "F2", 0
    db 0x3d, 3,  "F3", 0
    db 0x3e, 3,  "F4", 0
    db 0x3f, 3,  "F5", 0
    db 0x40, 3,  "F6", 0
    db 0x41, 3,  "F7", 0
    db 0x42, 3,  "F8", 0
    db 0x43, 3,  "F9", 0
    db 0x44, 4,  "F10", 0
    db 0x57, 4,  "F11", 0
    db 0x58, 4,  "F12", 0
    db 0x4e, 12, "KEYPAD PLUS", 0
    db 0x4a, 13, "KEYPAD MINUS", 0
    db 0x53, 4,  "DEL", 0
    db 0x1d, 5,  "CTRL", 0
    db 0x2a, 11, "LEFT SHIFT", 0
    db 0x36, 12, "RIGHT SHIFT", 0
    db 0x38, 4,  "ALT", 0
    db 0x45, 9,  "NUM LOCK", 0
    db 0x37, 9,  "KEYPAD *", 0
    db 0x46, 12, "SCROLL LOCK", 0
    db 0x4c, 9,  "KEYPAD 5", 0
    db 0x52, 7,  "INSERT", 0
    db 0x47, 5,  "HOME", 0
    db 0x4f, 4,  "END", 0
    db 0x49, 8,  "PAGE UP", 0
    db 0x51, 10, "PAGE DOWN", 0
    db 0xff

aUnknownKey:
    db "UNKNOWN KEY", 0

oneCharTable:
    db "1234567890-=", 0, 0, "QWERTYUIOP[]", 0, 0, "ASDFGHJKL;'`", 0
    db "\ZXCVBNM,./"
oneCharTableSize equ $ - oneCharTable

redefineStr:
    db "SATISFIED WITH THE CHOICE? (Y/N)", 0

joyMenuTitle:
    db "CALIBRATE JOYSTICK FOR PLAYER 1", 0

joyLeftStrings:
    dd aJoyLeftX, aJoyRightX, aJoyCenterX

joyRightStrings:
    dd aJoyLeftY, aJoyBottomY, aJoyCenterY

aJoyLeftX:
    db "LEFT X:", 0

aJoyLeftY:
    db "TOP Y:", 0

aJoyRightX:
    db "RIGHT X:", 0

aJoyBottomY:
    db "BOTTOM Y:", 0

aJoyCenterX:
    db "CENTER X:", 0

aJoyCenterY:
    db "CENTER Y:", 0

kbdVars:
    dd UP, pl2Codes

; Matrix of x and y axis readings that will be drawn by joystick calibration menu.
; High words get filled with values read from joystick. If high bit in lo word is set,
; values are skipped (not read and displayed). Keep in sync with start of joyVars.
; lo word: (except hi bit)
;  lo byte - x position on screen
;  hi byte - y        -//-
joyCalibrationValues:
    dd 0x8000 | kCalibrationMenuBaseY + 10 << 8 | 132, 0x8000 | kCalibrationMenuBaseY + 10 << 8 | 212   ; joyXLeft, joyYUp
    dd 0x8000 | kCalibrationMenuBaseY + 20 << 8 | 132, 0x8000 | kCalibrationMenuBaseY + 20 << 8 | 212   ; joyXRight, joyYDown
    dd 0x8000 | kCalibrationMenuBaseY + 30 << 8 | 132, 0x8000 | kCalibrationMenuBaseY + 30 << 8 | 212   ; joyXCenter, joyYCenter


joyCalibrationXLeft   equ joyCalibrationValues + 2
joyCalibrationYUp     equ joyCalibrationValues + 6
joyCalibrationXRight  equ joyCalibrationValues + 10
joyCalibrationYDown   equ joyCalibrationValues + 14
joyCalibrationXCenter equ joyCalibrationValues + 18
joyCalibrationYCenter equ joyCalibrationValues + 22

aMoveUpLeftStr:
    db "MOVE JOYSTICK UP AND LEFT AND PRESS FIRE", 0

aMoveDownRightStr:
    db "MOVE JOYSTICK DOWN AND RIGHT AND PRESS FIRE", 0

aMoveCenterStr:
    db "MOVE JOYSTICK TO CENTER AND PRESS FIRE", 0

calibrationTitleStrings:
    dd aMoveUpLeftStr, aMoveDownRightStr, aMoveCenterStr

aPressEscapeToCancel:
    db "(PRESS ESCAPE TO CANCEL)", 0

; masks for joystick buttons
joyButtonsMask:
    db 3, 12

aJoystickError:
    db "JOYSTICK ERROR", 0

aCalibrateAgain:
    db "CALIBRATE AGAIN? (Y/N)", 0

joyVars:
    dd joy1Vars, joy2Vars

joyXLeftPtr         equ 0
joyYUpPtr           equ 4
joyXRightPtr        equ 8
joyYDownPtr         equ 12
joyXCenterPtr       equ 16
joyYCenterPtr       equ 20
joyXLeftDivPtr      equ 24
joyXRightDivPtr     equ 28
joyYTopDivPtr       equ 32
joyYBottomDivPtr    equ 36
joyNumLoopsPtr      equ 40

joy1Vars:
    dd g_joy1MinX
    dd g_joy1MinY
    dd g_joy1MaxX
    dd g_joy1MaxY
    dd g_joy1CenterX
    dd g_joy1CenterY
    dd g_joy1XLeftDivisor
    dd g_joy1XRightDivisor
    dd g_joy1YTopDivisor
    dd g_joy1YBottomDivisor
    dd g_numLoopsJoy1

joy2Vars:
    dd g_joy2MinX
    dd g_joy2MinY
    dd g_joy2MaxX
    dd g_joy2MaxY
    dd g_joy2CenterX
    dd g_joy2CenterY
    dd g_joy2XLeftDivisor
    dd g_joy2XRightDivisor
    dd g_joy2YTopDivisor
    dd g_joy2YBottomDivisor
    dd g_numLoopsJoy2

aPressNormalFire:
    db "PRESS NORMAL FIRE", 0

aPressSpecialFire:
    db "PRESS SPECIAL FIRE (NORMAL FIRE SKIPS)", 0

joyButtonPointers:        ; points to SWOS variables
    dd g_joy1Fire1
    dd g_joy1Fire2
    dd g_joy2Fire1
    dd g_joy2Fire2


section .bss

oldInt9Ofs:
    resd 1
oldInt9Seg:
    resw 1
keyStrBuf:
    resb 2
global shiftDown
shiftDown:
    resb 1

; to select values add player number * 4 to corresponding address
; values for joystick 1
joyXRead:
    resw 1
joyYRead:
    resw 1
; values for joystick 2
    resw 1
    resw 1

joyButtons:
    resb 1

; previous player 2 on keyboard flag
oldPl2Keyboard:
    resb 1
