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

extern int2str_
extern SaveOptionsIfNeeded_

section .text

extern replayStatus
extern rplFrameSkip
extern HandleMPKeys_


; RegisterControlsOptions [called from Watcom]
;
; in:
;      eax -> options register callback (see options.c for format details)
;
; Register our options with options manager, so we can save and load later without much effort.
;
global RegisterControlsOptions_
RegisterControlsOptions_:
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

        extern ToggleDebugOutput_
        call ToggleDebugOutput_

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
        call HandleMPKeys_
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


; ControlsInit
;
; Called every time controls menu is entered. Sets entry that corresponds to
; controls configuration to red.
;
ControlsInit:
        push byte 6
        pop  ecx                ; ecx = counter, 6 entries
        push byte 2
        pop  edi                ; edi = starting entry number

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
        push byte 7              ; it is entry 7
        jmp  short .set_entry_color

.not_pl2_keyboard:
        mov  ax, [joyKbdWord]   ; else use joyKbdWord as entry index
        inc  eax                ; entries start from 2 and are zero based
        push eax

.set_entry_color:
        pop  dword [D0]
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  word [esi + MenuEntry.backAndFrameColor], 13 ; set selected entry color
        retn


; ControlsOnSelectCommon
;
; Called when some entry is selected. Does all work with controls changing and
; calibrating.
;
ControlsOnSelectCommon:
        mov  esi, [A5]          ; esi -> this entry
        xor  ebx, ebx
        mov  bx, [esi + MenuEntry.ordinal] ; bx = this entry ordinal
        dec  ebx
        dec  ebx
        mov  ecx, ebx           ; ecx = controls index zero based
        cmp  ecx, 5             ; keyboard + keyboard?
        jz   .2pl_kbd

        xor  eax, eax
        call SetSecondPlayerOnKeyboard
        jmp  short .inc_ordinal

.2pl_kbd:
        sub  ecx, byte 2        ; make it look like keyboard + joystick

.inc_ordinal:
        inc  ecx                ; joyKbdWord starts from 1, increase ordinal
        mov  [joyKbdWord], cx
        or   ecx, byte -1       ; ecx will be player number, init to -1
        xor  eax, eax           ; clear eax so that we get zero when shifting
        mov  ax, [calibrate_values + 2 * ebx] ; get control code
        xor  esi, esi           ; esi will mark joystick number to calibrate
        xor  edi, edi           ; edi will mark keyboard number to redefine

.next_control:
        mov  word [scanCode], 0 ; reset last scan code to zero
        inc  ecx                ; increase player number
        mov  dl, al             ; get control code
        shr  eax, 8
        test dl, dl
        jz   .clear_controls_out ; if zero, no more controls

        mov  ebx, kbd_vars
        cmp  dl, 1
        jnz  .not_kbd

        mov  ebx, [ebx + 4 * edi] ; set ebx -> vars to set
        call RedefineKeys       ; keyboard
        test edi, edi
        jz   .keyboard1
        push eax
        or   eax, -1            ; patch code for second keyboard player
        call SetSecondPlayerOnKeyboard
        pop  eax
.keyboard1:
        inc  edi                ; next time it will be second keyboard
        jmp  short .next_control

.not_kbd:
        call CalibrateJoypad    ; joypad
        jc   .out               ; check if error

        inc  esi                ; next time it will be joypad 2
        jmp  short .next_control

.clear_controls_out:
        push byte 105           ; save changes to setup.dat
        mov  dword [A0], aSetup_dat
        mov  dword [A1], setup_dat_buffer
        pop  dword [D1]
WriteToLog "numLoopsJoy1 = %hd, numLoopsJoy2 = %hd", numLoopsJoy1, numLoopsJoy2
        calla WriteFile
WriteToLog "numLoopsJoy1 = %hd, numLoopsJoy2 = %hd", numLoopsJoy1, numLoopsJoy2
        call SaveOptionsIfNeeded_   ; save options if needed :)
.out:
        xor  eax, eax
        mov  [controlWord], ax  ; reset all control variables because int9
        mov  [joy1Status], ax   ; could happen in the middle of this function,
        mov  [joy2Status], ax   ; and variables could be erroneously set before
                                ; new scan codes are set
        call ControlsInit       ; set color of newly selected entry
        WriteToLog "New controls set, joyKbdWord = %hd", dword [joyKbdWord]
        WriteToLog "numLoopsJoy1 = %hd, numLoopsJoy2 = %hd", numLoopsJoy1, numLoopsJoy2
        retn


; Player2KeyProc
;
; When patched in, will be called directly from keyboard handler, from
; SetControlWord. Updates second player control bitfields (joy2_status).
;
Player2KeyProc:
        push eax                ; ax contains scan code
        movzx ebx, word [joy2Status]
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
        mov  [joy2Status], bx
        mov  bx, [controlWord]
        pop  eax                ; restore scan code
        retn


global SetSecondPlayerOnKeyboard
; SetSecondPlayerOnKeyboard
;
; in:
;      al - keyboard 2 on or off (boolean)
;
; Turns support for second player on keyboard on or off.
;
SetSecondPlayerOnKeyboard:
        push esi
        push edi
        push ecx

        mov  ecx, eax
        mov  edx, DummyInt9     ; install dummy int9 handler, to exclude
        or   eax, byte -1       ; possibility of keyboard handler running
        call InstallKeyboardHandler ; while we are changing it
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
        sti                     ; restore interrupts
        xor  eax, eax
        call InstallKeyboardHandler ; return old keyboard handler
        pop  ecx
        pop  edi
        pop  esi
        retn

.patch_it_back:                 ; restore as it was
        mov  byte [pl2Keyboard], 0
        push byte 7
        mov  byte [Joy2SetStatus], 0x66
        mov  byte [Player1StatusProc + 8], 0x75
        mov  byte [Player2StatusProc + 8], 0x74
        pop  ecx
        mov  esi, UnpatchSetControlWord
        rep  movsb
        jmp  short .out

PatchSetControlWord:            ; just a data to patch
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
        mov  word [scanCode], 0

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
        mov  [D2], edi       ; y = edi
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
        mov  dword [A0], redefine_str
        mov  dword [A1], smallCharsTable
        pushad
        calla DrawMenuTextCentered
        calla FlipInMenu
        popad

.get_key:
        mov   al, [scanCode]
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
;      carry flag: set = scan code invalid, clear = ok
;
; Checks if scan code has been used before.
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
        mov  edi, [kbd_vars]
        repnz scasb
        setnz al
        jmp  short .out

.keyboard2:
        mov  edi, [kbd_vars + 4] ; zero flag already clear because edi != 0
        repnz scasb
        setnz bl
        inc  edi                ; clear zero flag in case ecx = 0
        push byte 5
        mov  edi, [kbd_vars]
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
;      edi - keyboard nmber (0 or 1)
;
; Draws menu for selecting keys.
;
DrawSelectKeysMenu:
        pushad
        push edi
        add  cl, '1'
        mov  [select_keys_str + 23], cl
        mov  ecx, edx
        mov  esi, [lin_adr_384k]
        mov  edi, esi
        add  esi, 131072
        mov  ecx, 16000
        rep  movsd              ; draw background
        mov  edx, smallCharsTable
        mov  word [D1], 160
        mov  word [D2], 30
        mov  word [D3], 2
        mov  dword [A0], select_keys_str
        mov  [A1], edx
        calla DrawMenuTextCentered
        mov  word [D1], 82
        add  word [D2], 40
        mov  dword [A0], up_str
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], down_str
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], left_str
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], right_str
        mov  [A1], edx
        calla DrawMenuText
        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], fire_str
        mov  [A1], edx
        calla DrawMenuText
        pop  ecx
        test ecx, ecx
        jz   .out

        mov  word [D1], 82
        add  word [D2], 10
        mov  dword [A0], sec_fire_str
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
        mov  byte [scan_code], 0

.get_code_loop:
        mov  al, [scan_code]
        or   al, al
        jz   .get_code_loop

.key_up_loop:
        or   al, al
        jge  .return_old_handler
        mov  al, [scan_code]
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
; Reads a scan code from keyboard controller and puts it into scan_code
; variable.
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
        mov  [scan_code], ah
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
        mov  esi, key_table
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

.found_it:                      ; got it!
        lodsb                   ; skip length byte
        mov  edx, esi
        jmp  short .out

.not_found:
        sub  bl, 2
        mov  al, bl
        mov  esi, one_char_table
        and  al, 0x7f
        cmp  al, one_char_table_size
        ja   .unknown
        mov  al, [esi + ebx]
        test al, al
        jz   .unknown
        mov  [key_str_buf], al
        mov  byte [key_str_buf + 1], 0
        mov  edx, key_str_buf
        jmp  short .out

.unknown:
        mov  edx, unknown       ; don't know this scan code
.out:
        pop  ebx
        pop  esi
        retn


; CalibrateJoypad
;
; in:
;      ecx - player number (0 or 1)
;      esi - joypad number (0 or 1) to calibrate
; out:
;      carry flag: set = error, clear = ok
;
; Draws menu and calibrates joypad. Joypad number decides which joypad.
;
CalibrateJoypad:
        push eax
        push edx
        push ebp
        push edi
        push esi                        ; esi must be pushed one before last
        push ecx                        ; ecx must be pushed last

.recalibrate:
        mov  word [scanCode], 0
        mov  bl, [joy_buttons_mask + esi]
        push byte 4
        mov  esi, joy_values + 8
        pop  ecx                        ; undefine 4 values

.undefine_values_loop:
        or   dword [esi], 0x8000        ; undefine all values except first two
        add  esi, byte 4
        dec  ecx
        jnz  .undefine_values_loop

        mov  eax, joy_values
        sub  esi, byte 24
        and  dword [esi], ~0x8000       ; set first two values as defined - they will
        and  dword [esi + 4], ~0x8000   ; be read now
        mov  edx, smallCharsTable
        call MyReadGamePort
        jc   .joy_error
        call ClearJoystickButtons       ; get rid of possible previous button press

.up_left_loop:
        pop  ecx
        push ecx                        ; restore ecx for DrawJoypadMenu
        push ebx
        call DrawJoypadMenu
        mov  word [D1], 160
        mov  word [D2], 55
        mov  word [D3], 13
        mov  dword [A0], move_up_left_str
        mov  [A1], edx
        calla DrawMenuTextCentered
        calla FlipInMenu
        pop  ebx
        call MyReadGamePort
        jc   .joy_error
        mov  ecx, [esp + 4]             ; get joypad number
        shl  ecx, 2
        mov  ebp, [joy_vars + ecx]
        mov  ax, [joy_x_read + ecx]
        mov  [joy_x_left], ax
        mov  esi, [ebp + joy_x_left_ptr]
        mov  [esi], ax
        mov  ax, [joy_y_read + ecx]
        mov  [joy_y_up], ax
        mov  esi, [ebp + joy_y_up_ptr]
        mov  [esi], ax
        mov  al, [joy_buttons]
        and  al, bl
        jz   .up_left_loop

        mov  esi, joy_values
        and  dword [esi + 8], ~0x8000
        and  dword [esi + 12], ~0x8000
        call ClearJoystickButtons

.down_right_loop:
        pop  ecx
        push ecx
        push ebx
        call DrawJoypadMenu
        mov  word [D1], 160
        mov  word [D2], 55
        mov  word [D3], 13
        mov  dword [A0], move_down_right_str
        mov  [A1], edx
        calla DrawMenuTextCentered
        calla FlipInMenu
        pop  ebx
        call MyReadGamePort
        jc   .joy_error
        mov  ecx, [esp + 4]             ; get joypad number
        shl  ecx, 2
        mov  ebp, [joy_vars + ecx]
        mov  ax, [joy_x_read + ecx]
        mov  [joy_x_right], ax
        mov  esi, [ebp + joy_x_right_ptr]
        mov  [esi], ax
        mov  ax, [joy_y_read + ecx]
        mov  [joy_y_down], ax
        mov  esi, [ebp + joy_y_down_ptr]
        mov  [esi], ax
        mov  al, [joy_buttons]
        and  al, bl
        jz   .down_right_loop

        mov  esi, joy_values
        and  dword [esi + 16], ~0x8000
        and  dword [esi + 20], ~0x8000
        call ClearJoystickButtons

.center_loop:
        pop  ecx
        push ecx
        push ebx
        call DrawJoypadMenu
        mov  word [D1], 160
        mov  word [D2], 55
        mov  word [D3], 13
        mov  dword [A0], move_center_str
        mov  [A1], edx
        calla DrawMenuTextCentered
        calla FlipInMenu
        pop  ebx
        call MyReadGamePort
        jc   .joy_error
        mov  ecx, [esp + 4]             ; get joypad number
        shl  ecx, 2
        mov  ebp, [joy_vars + ecx]
        mov  ax, [joy_x_read + ecx]
        mov  [joy_x_center], ax
        mov  esi, [ebp + joy_x_center_ptr]
        mov  [esi], ax
        mov  ax, [joy_y_read + ecx]
        mov  [joy_y_center], ax
        mov  esi, [ebp + joy_y_center_ptr]
        mov  [esi], ax
        mov  al, [joy_buttons]
        and  al, bl
        jz   .center_loop

        mov  ecx, [esp + 4]             ; get joypad number
        push edx                        ; edx must be saved; it points to chars table
        lea  esi, [joy_button_pointers + 8 * ecx] ; select joy 1 or 2
        xor  edx, edx

.set_buttons_loop:
        call ClearJoystickButtons       ; clear superfluous button presses
        call MyReadGamePort
        jc   .joy_error2
        call DrawJoypadMenu
        call ConvertJoystickButtons
        push edx
        push ecx
        calla FlipInMenu, edx
        pop  ecx
        pop  edx
        test al, al
        jz   .set_buttons_loop

.save_button:
        mov  edi, [esi + 4 * edx]       ; select button 1 or 2
        mov  [edi], al                  ; write selected button value

        inc  edx
        cmp  edx, byte 2
        jl   .set_buttons_loop

        pop  edx                        ; restore chars table pointer

        ; write out divisors and number of loops
        mov  esi, [joy_vars + 4 * ecx]
        mov  ebp, [esi + joy_x_neg_div_ptr]
        mov  ax, [joy_x_center]
        sub  ax, [joy_x_left]
        mov  [ebp], ax
        mov  ebp, [esi + joy_x_pos_div_ptr]
        mov  ax, [joy_x_right]
        sub  ax, [joy_x_center]
        mov  [ebp], ax
        mov  ebp, [esi + joy_y_neg_div_ptr]
        mov  ax, [joy_y_center]
        sub  ax, [joy_y_up]
        mov  [ebp], ax
        mov  ebp, [esi + joy_y_pos_div_ptr]
        mov  ax, [joy_y_down]
        sub  ax, [joy_y_center]
        mov  [ebp], ax
        mov  ebp, [esi + joy_num_loops_ptr]
        xor  eax, eax
        xor  ecx, ecx
        mov  ax, [joy_y_down]
        mov  cx, [joy_x_right]
        sub  eax, ecx
        sbb  ebx, ebx                   ; ebx = eax < ecx ? -1 : 0
        neg  ebx                        ; ebx = eax < ecx ? 0 : -1
        and  ebx, eax                   ; ebx = eax < ecx ? 0 : eax - ecx
        add  ecx, ebx                   ; ecx = eax < ecx ? ecx : eax
        mov  [ebp], cx                  ; cx = max(ax, bx) for number of loops

        call ClearJoystickButtons       ; get rid of left over button presses
        pop  ecx
        push ecx
        call DrawJoypadMenu             ; get rid of "press special fire" message
        mov  word [D1], 160
        mov  word [D2], 55
        mov  word [D3], 2
        mov  dword [A0], calibrate_again_str
        mov  [A1], edx
        calla DrawMenuTextCentered      ; ask user if (s)he wishes to do calibration again
        calla FlipInMenu
        mov  esi, [esp + 4]             ; restore esi for another possible loop

.get_key:
        mov  al, [scanCode]             ; get answer to recalibrate question
        cmp  al, 0x15                   ; 'y'
        jz   .recalibrate
        cmp  al, 0x31                   ; 'n'
        jnz  .get_key
        clc

.out:
        pop  ecx
        pop  esi
        pop  edi
        pop  ebp
        pop  edx
        pop  eax
        retn

.joy_error2:
        pop  edx

.joy_error:
        mov  word [joyKbdWord], 1       ; set to keyboard only
        cmp  word [scanCode], 0x01      ; if user pressed escape, than there
        jz   .out                       ; is no need for showing error menu
        mov  dword [A0], joy_error_str
        calla ShowErrorMenu
        stc
        jmp  short .out


; DrawJoypadMenu
;
; in:
;      ecx - player number (0 or 1)
;
; Draws joypad calibration menu, drawing values for extreme readings if
; defined.
;
DrawJoypadMenu:
        push ecx
        push ebx
        push edx

        add  cl, '1'
        mov  byte [joy_menu_title + 30], cl
        mov  esi, [lin_adr_384k]
        mov  edi, esi
        add  esi, 131072
        mov  ecx, 16000
        rep  movsd              ; draw background
        mov  edx, smallCharsTable
        mov  word [D1], 160
        mov  word [D2], 30
        mov  word [D3], 2
        mov  dword [A0], joy_menu_title
        mov  [A1], edx
        calla DrawMenuTextCentered
        push byte 82
        xor  eax, eax
        mov  esi, joy_left_strings ; first draw left strings
        pop  edi

.prepare_loop:
        push byte 3
        mov  word [D2], 70
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
        mov  esi, joy_right_strings ; then draw right strings
        mov  edi, 162
        jmp  short .prepare_loop

.write_joy_values:
        push byte 6
        mov  esi, joy_values
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
        call int2str_           ; convert to string
        mov  [D1], ecx
        mov  [D2], ebx
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


; MyReadGamePort
;
; out:
;      carry flag: set = error, clear = ok
;
; Reads game port and writes values to joy_readings array. My is because of
; name clash with SWOS. Also checks keyboard on start, if escape is pressed
; returns as if joypad error was detected.
;
MyReadGamePort:
        pushad

        cmp  word [scanCode], 0x01
        jz   .err_out

        xor  ecx, ecx
        xor  ebx, ebx
        xor  ebp, ebp
        xor  esi, esi
        xor  edi, edi
        mov  dx, 0x201
        or   cx, byte -1
        cli
        out  dx, al                 ; trigger timer chip

        jmp  $ + 2
        jmp  $ + 2

.read_loop:
        in   al, dx
        shr  al, 1
        adc  ebx, 0
        shr  al, 1
        adc  ebp, 0
        shr  al, 1
        adc  esi, 0
        shr  al, 1
        adc  edi, 0
        loop .read_loop
        sti

        cmp  bx, -1                 ; if bx is -1 then joypad is not connected
        jz   .err_out

.set_values:
        mov  [joy_x_read], bx       ; joy1 x
        mov  [joy_y_read], bp       ; joy1 y
        mov  [joy_x_read + 4], si   ; joy2 x
        mov  [joy_y_read + 4], di   ; joy2 y
        not  al
        and  al, 0x0f
        mov  [joy_buttons], al      ; buttons in low nibble
        clc
        jmp  short .out

.err_out:
        or   eax, byte -1           ; error, set al to -1
        mov  [joy_x_read], eax
        mov  [joy_x_read + 4], eax
        mov  [joy_buttons], al
        stc

.out:
        popad
        retn


; ClearJoystickButtons
;
; out:
;      carry flag: set = error, clear = ok
;
; Waits for joystick buttons to be depressed.
;
ClearJoystickButtons:
        mov  al, [joy_buttons]
        test al, al
        jz   .out_ok
        call MyReadGamePort
        jc   .out
        jmp  short ClearJoystickButtons
.out_ok:
        clc
.out:
        retn


; ConvertJoystickButtons
;
; in:
;      ebx - joystick buttons mask
;      ecx - joystick number (0 or 1)
;      edx - button number (0 or 1)
; out:
;      eax - button pressed: 0 = none, -1 = button 1, -2 = button 2
;
; Converts joystick buttons presses into negative button indices. Also draw
; "press ... button" string.
;
ConvertJoystickButtons:
        push ecx
        call DrawJoypadMenu
        mov  word [D1], 160
        mov  word [D2], 130
        mov  word [D3], 13
        mov  dword [A0], press_normal_fire_str
        test edx, edx
        jz   .normal_fire

        mov  dword [A0], press_special_fire_str

.normal_fire:
        mov  dword [A1], smallCharsTable
        push edx
        push ecx
        push ebx
        calla DrawMenuTextCentered
        pop  ebx
        pop  ecx
        pop  edx
        mov  al, [joy_buttons]
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
        pop  ecx
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
        int  0x21                   ; get old kbd handler, es:ebx
        mov  [old_int9_ofs], ebx    ; save it for later
        mov  [old_int9_seg], es
        mov  ax, cs
        mov  ds, ax
        mov  ax, 0x2509             ; now setup new one
        int  0x21
        jmp  short .out

.uninstall:
        cli
        mov  ax, 0x2509
        mov  edx, [old_int9_ofs]
        mov  ds, [old_int9_seg]
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
; Patch existing int 9 keyboard handler with support for detecting wether shift
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
        cmp  al, 0x2a   ; lshift down
        jz   .shift_on
        cmp  al, 0x36   ; rshift down
        jz   .shift_on
        cmp  al, 0xaa   ; lshift up
        jz   .shift_off
        cmp  al, 0xb6   ; rshift up
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
;     ax - converted key, from scan code to ascii
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
        mov  byte [convert_keys_table + 0x0c], "-"
        mov  word [convert_keys_table + 0x27], ";'"     ; patch conversion table too
        mov  dword [convert_keys_table + 0x33], ',' | '.' << 8 | '/' << 16 | 0x16 << 24
        jmp  PatchFilterKeys


global DisableAdditionalInputKeys
DisableAdditionalInputKeys:
        mov  esi, FilterKeysOrigBytes
        mov  byte [convert_keys_table + 0x0c], 0
        mov  word [convert_keys_table + 0x27], 0
        mov  dword [convert_keys_table + 0x33], 0x16 << 24
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

    StartMenu controlsMenu, ControlsInit, 0, 0, 2
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
calibrate_values:
    dw 0x0001, 0x0002, 0x0102, 0x0201, 0x0202, 0x0101

select_keys_str:
    db "SELECT KEYS FOR PLAYER 1", 0
up_str:
    db "UP:", 0
down_str:
    db "DOWN:", 0
left_str:
    db "LEFT:", 0
right_str:
    db "RIGHT:", 0
fire_str:
    db "FIRE:", 0
sec_fire_str:
    db "BENCH:", 0
invalid_key_str:
    db "INVALID KEY", 0

; scan codes to strings - first scan code, than length of string
; (+ terminator), and then the string itself
; table ends with 0xff scan code
key_table:
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

unknown:
    db "UNKNOWN KEY", 0

one_char_table:
    db "1234567890-=", 0, 0, "QWERTYUIOP[]", 0, 0, "ASDFGHJKL;'`", 0
    db "\ZXCVBNM,./"
one_char_table_size equ $ - one_char_table

redefine_str:
    db "SATISFIED WITH THE CHOICE? (Y/N)", 0

joy_menu_title:
    db "CALIBRATE JOYSTICK FOR PLAYER 1", 0

joy_left_strings:
    dd joy_leftx, joy_rightx, joy_centerx

joy_right_strings:
    dd joy_topy, joy_bottomy, joy_centery

joy_leftx:
    db "LEFT X:", 0

joy_topy:
    db "TOP Y:", 0

joy_rightx:
    db "RIGHT X:", 0

joy_bottomy:
    db "BOTTOM Y:", 0

joy_centerx:
    db "CENTER X:", 0

joy_centery:
    db "CENTER Y:", 0

kbd_vars:
    dd UP, pl2Codes

; high words will be values read
; if high bit in lo word is set, values are undefined
; lo word: (except hi bit)
; lo byte - x
; hi byte - y
joy_values:
    dd 0x8000 | 80 << 8 | 132, 0x8000 | 80 << 8 | 212
    dd 0x8000 | 90 << 8 | 132, 0x8000 | 90 << 8 | 212
    dd 0x8000 | 100 << 8 | 132, 0x8000 | 100 << 8 | 212

joy_x_left   equ joy_values + 2
joy_y_up     equ joy_values + 6
joy_x_right  equ joy_values + 10
joy_y_down   equ joy_values + 14
joy_x_center equ joy_values + 18
joy_y_center equ joy_values + 22

move_up_left_str:
    db "MOVE JOYSTICK UP AND LEFT AND PRESS FIRE", 0

move_down_right_str:
    db "MOVE JOYSTICK DOWN AND RIGHT AND PRESS FIRE", 0

move_center_str:
    db "MOVE JOYSTICK TO CENTER AND PRESS FIRE", 0

; masks for joystick buttons
joy_buttons_mask:
    db 3, 12

joy_error_str:
    db "JOYSTICK ERROR", 0

calibrate_again_str:
    db "CALIBRATE AGAIN? (Y/N)", 0

joy_vars:
    dd joy1_vars, joy2_vars

joy_x_left_ptr    equ 0
joy_y_up_ptr      equ 4
joy_x_right_ptr   equ 8
joy_y_down_ptr    equ 12
joy_x_center_ptr  equ 16
joy_y_center_ptr  equ 20
joy_x_neg_div_ptr equ 24
joy_x_pos_div_ptr equ 28
joy_y_neg_div_ptr equ 32
joy_y_pos_div_ptr equ 36
joy_num_loops_ptr equ 40

joy1_vars:
    dd joy1_min_X
    dd joy1_min_Y
    dd joy1_max_X
    dd joy1_max_Y
    dd joy1_center_X
    dd joy1_center_Y
    dd joy1_X_negative_divisor
    dd joy1_X_positive_divisor
    dd joy1_Y_negative_divisor
    dd joy1_Y_positive_divisor
    dd numLoopsJoy1

joy2_vars:
    dd joy2_min_X
    dd joy2_min_Y
    dd joy2_max_X
    dd joy2_max_Y
    dd joy2_center_X
    dd joy2_center_Y
    dd joy2_X_negative_divisor
    dd joy2_X_positive_divisor
    dd joy2_Y_negative_divisor
    dd joy2_Y_positive_divisor
    dd numLoopsJoy2

press_normal_fire_str:
    db "PRESS NORMAL FIRE", 0

press_special_fire_str:
    db "PRESS SPECIAL FIRE (NORMAL FIRE SKIPS)", 0

joy_button_pointers:        ; points to SWOS variables
    dd joy1_fire_1
    dd joy1_fire_2
    dd joy2_fire_1
    dd joy2_fire_2


section .bss

old_int9_ofs:
    resd 1
old_int9_seg:
    resw 1
key_str_buf:
    resb 2
scan_code:
    resb 1
global shiftDown
shiftDown:
    resb 1

; to select values add player number * 4 to corresponding address
; values for joypad 1
joy_x_read:
    resw 1
joy_y_read:
    resw 1
; values for joypad 2
    resw 1
    resw 1

joy_buttons:
    resb 1

; previous player 2 on keyboard flag
oldPl2Keyboard:
    resb 1