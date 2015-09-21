; SWOS++ main, contains startup routine

[list -]
%include "swos.inc"
%include "version.inc"
[list +]

global start
global FatalError

extern IPX_IsInstalled
extern InitializeOptions
extern aboutText
extern pl2Keyboard
%ifdef DEBUG
extern StartLogFile
%endif
extern qAllocInit
extern EnableShiftSupport
extern InstallCrashLogger

section .data
pressAnyKeyStr:
    db 13, 10, "<Press any key to terminate>", 13, 10
dosboxString:
    db "DOSBox", 0
dosboxStringLen equ $ - dosboxString - 1
detectedStr:
    db " detected!", 13, 10, '$', 0

%ifdef DEBUG
verStrAddr:
    db verStr, 0
%endif

section .text

; start
;
; in:
;      eax = starting address, fixed up
;      ebx = code & data base address
;      ecx = code & data size (+bss)
;      edx = code & data memory handle
;      ebp = SWOS code base
;
; Main init routine - perform entire program initialization. Loader will call us, right before SWOS proc.
; Note that SWOS Initialization() hasn't run yet.
;
start:
        mov  [codeSize], ecx
        mov  [codeHandle], edx
        mov  ax, ds
        mov  [dataSeg], ax
%ifdef DEBUG
        mov  [codeBase], ebx
%endif
        call SwitchStack
        call FixSWOSIntro
        call CheckWindowsVersion
        call DexorAbout
%ifdef DEBUG
        pushfd
        call UpdatePrevVideoMode
        call StartLogFile
        WriteToLog "SWOS++ v%s loaded at %#x, SWOS code segment starting at %#x, data starting at %#x", \
                    verStrAddr, dword [codeBase], swosCodeBase, swosDataBase
        calla swos_libc_stackavail_
        WriteToLog "_STACKTOP = %#x, _STACKLOW = %#x", dword [_STACKTOP], dword [_STACKLOW]
        WriteToLog "Stack top at entry = %#x, esp = %#x", dword [SWOS_StackTop], esp
        WriteToLog "Stack memory available: %d bytes.", eax
        popfd
        call IPX_IsInstalled    ; test presence of IPX for multiplayer
        WriteToLog "IPX Network supported: %#x", eax
%endif
        WriteToLog "Initializing qAlloc..."
        call qAllocInit         ; initialize QuickAlloc
        call InstallCrashLogger
        call InitializeOptions  ; try to load options
        call VideoBenchmark
        WriteToLog "Video memory index = %hd", eax
        mov  [videoSpeedIndex], ax
        call EnableShiftSupport
.out:
        WriteToLog "Returning control to SWOS..."
        retn


; EndProgram
;
; in:
;      eax - true if it's an abnormal exit
;
; Performs program uninitialization. Saves options in case they were changed.
; Makes sure log file is flushed. Ends program. Zero exit code is always returned.
; Only thing not done here is switching back to previous video mode, as the
; callers would probably want to print some messages first.
;
global EndProgram
extern replayStatus, CloseReplayFile, SaveOptionsIfNeeded
extern FinishMultiplayer, FinishMultiplayerGame, qAllocFinish, EndLogFile
EndProgram:
        push eax
        test byte [replayStatus], 2
        jz   .no_rpl_save

        calla SetHilFileHeader
        xor  eax, eax
        inc  eax                        ; set eax to true to mark normal call
        call CloseReplayFile            ; close replay file and set header

.no_rpl_save:
        call FinishMultiplayer          ; and get rid of network too
        call FinishMultiplayerGame      ; in case we were in a game
        call SaveOptionsIfNeeded        ; save options, but only after they have been restored

        pop  eax
        test eax, eax
        jnz  .skip_qalloc
        call qAllocFinish               ; check for memory leaks, but only if everything is ok

.skip_qalloc:
%ifdef DEBUG
        call EndLogFile
%endif
        jmpa SWOS_EndProgram            ; this will go back into SWOS main, and return 0


; SwitchToPrevVideoMode
;
; Return to video mode we were in before running the game.
;
global SwitchToPrevVideoMode
SwitchToPrevVideoMode:
        mov  ah, 0
        mov  al, [prevVideoMode]
        int  0x10
        retn


; SwitchStack
;
; Attempt to circumvent low stack situation. For now, simply manually copy stack to new location,
; and set esp to point to it. DOS4/GW keeps ss = ds so it's possible to do it like this.
;
extern memcpy
SwitchStack:
        cli                     ; probably not necessary, but just in case
        mov  ecx, [_STACKTOP]
        sub  ecx, esp           ; ecx = ammount of stack storage taken
        mov  eax, newStackTop
        mov  [_STACKTOP], eax
        sub  eax, ecx           ; new esp
        mov  ebx, eax
        mov  edx, esp           ; old esp
        call memcpy             ; copy current stack state
        mov  esp, ebx
        mov  eax, newStack
        mov  [_STACKLOW], eax
        sti
        retn


; FatalError
;
; in:
;      edx -> string to print
;
; Terminates program after showing message to user. Waits for keypress from
; user to terminate program. Do not use in startup code.
;
FatalError:
        xor  eax, eax
        xor  ecx, ecx
        dec  ecx
        mov  edi, edx
        repnz scasb
        not  ecx                ; get string lenght
        dec  ecx                ; without ending zero
        mov  al, [prevVideoMode]
        int  0x10
        push byte 2             ; stderr
        mov  ah, 0x40
        pop  ebx
        int  0x21               ; show message
        push byte 32
        mov  edx, pressAnyKeyStr
        pop  ecx
        mov  ah, 0x40
        int  0x21
        calla StopAIL
        calla CDROM_StopAudio
        calla RestoreOldInt9Handler
        xor  eax, eax
        int  0x16
        xor  eax, eax
        inc  eax                ; return exit code 1
        mov  esp, [SWOS_StackTop]
        retn                    ; go away...


; DetectDOSBox
;
; Detects if DOSBox is running. No way to do it through Int 21/AX=3306h so we
; need a special method - search 128 bytes from F000:E000 for string "DOSBox".
;
; out:
;      zero flag clear = DOSBox successfully detected, set otherwise
;
DetectDOSBox:
        mov  esi, 0xf000 << 4 | 0xe000 - 1 ; DOS/4GW maps first 1mb of low memory
        mov  ecx, 128

.loop:
        mov  eax, esi
        mov  ebx, dosboxStringLen
        mov  edx, dosboxString

        calla swos_libc_strnicmp_, edi
        test eax, eax
        jz   .found
        inc  esi
        dec  ecx
        jnz  .loop

        xor eax, eax
        retn

.found:
        inc eax
        retn


; Locations in the executable that will be modified if any of known patches is installed. Ends with null pointer.
patchCheckTable:
        dd 0xaf0e   ; SWOS HD patches next 3
        db 1
        db 0x33

        dd 0xaf1e   ; this one is patched by Frankenstein SWOS too
        db 1
        db 0x60

        dd 0x77404
        db 1
        db 0xc7

        dd 0x7dd0   ; Frankenstein SWOS specific
        db 1
        db 0x74

        dd 0x956e
        db 6
        db 0x0f, 0x84, 0xb5, 0x00, 0x00, 0x00

        dd 0

; DetectOriginalSWOS
;
; Detect if we're running on original, unpatched version of SWOS.
; It's just a quick and dirty method, don't wanna CRC the whole program.
;
; out:
;      zero flag set = we are running original SWOS, otherwise it's patched
;
extern memcmp
DetectOriginalSWOS:
        mov  esi, patchCheckTable
        xor  ebx, ebx

.next_check:
        lodsd
        test eax, eax
        jz   .out

        mov  bl, [esi]
        inc  esi
        mov  edx, esi
        add  esi, ebx
        add  eax, swosCodeBase
        mov  ecx, ebx
        call memcmp
        test eax, eax
        jz   .next_check

.out:
        retn


; CheckWindowsVersion
;
; Check Windows/DOS version we're running on, and issue a warning if SWOS++ might not work well with it.
; Left-over from real DOS/Win 9x days, nowadays it's gonna be DOSBox mostly ;)
;
CheckWindowsVersion:
        mov  ax, 0x1600         ; do Windows version check
        int  0x2f               ; first we try int 0x2f
        test al, al
        jnz  .windows_ok        ; if this succedes, Windows 9x is loaded - ok

        mov  ax, 0x3306         ; this has failed, either no Windows or
        int  0x21               ; Windows NT/2000/XP is loaded, so we check
        xchg bh, bl             ; "true" MS-DOS version, and if it is below

        cmp  bx, 0x0600         ; 6.0, issue a warning
        jae  .windows_ok

        call DetectDOSBox       ; DOSBox is masquerading as DOS 5.0
        jz   .show_warning

        mov  edx, dosboxString
        mov  [edx + dosboxStringLen], byte '$'
        mov  ah, 9
        int  0x21
        mov  edx, detectedStr
        mov  ah, 9
        int  0x21

        jmp  short .windows_ok

.show_warning:
        mov  edx, badOS        ; some sort of NT running...
        mov  ah, 9
        int  0x21
        xor  eax, eax
        int  0x16

.windows_ok:
        retn


; FixSWOSIntro
;
; On Franky SWOS if you let intro time out, instead of going to main screen the game will freeze. So
; take care of that, but be careful not to patch original SWOS.
;
FixSWOSIntro:
        call DetectOriginalSWOS         ; following patch kills intro, so don't do it on vanilla SWOS
        jz   .out

        mov  byte [SWOS + 0x3f], 0xeb   ; fix intro screen bug when no keys are pressed
.out:
        retn


%ifdef DEBUG
; UpdatePrevVideoMode
;
; Update variable used for restoring video mode after the game with current video mode. SWOS' Initialization()
; already does this but we will repeat it here in case of assertions triggering in the start up code.
;
UpdatePrevVideoMode:
        mov  ah, 0x0f
        int  0x10
        mov  [prevVideoMode], al
        retn
%endif


; VideoBenchmark
;
; out:
;      eax - video memory speed index
;
; Taken from install.exe and adapted to 32-bit DOS4/GW. Tries to measure video
; graphics memory speed.
;
VideoBenchmark:
        mov  ebx, 176
        mov  esi, 0x46c     ; bios daily timer counter
        xor  ecx, ecx
        mov  ebp, -1        ; original value of 10,000 seems so naive now
        sti
        mov  dx, [esi]      ; bios daily timer counter, increments roughly 18.206 times per second (every ~54.936ms)

.tick_edge_loop:
        mov  ax, [esi]
        xchg ax, dx         ; while current and previous equal, loop
        xor  ax, dx
        loopz .tick_edge_loop
        jnz  short .time_tick_start
        dec  ebp
        jnz  short .tick_edge_loop
        jmp  short .out

.time_tick_start:
        xor  ecx, ecx        ; we're on tick limit now
.loop_write_to_vmem:
        push ecx
        call Write16BytesToVideoMemory ; how many times will this execute during 1 bios timer cycle
        pop  ecx
        cmp  dx, [esi]
        loopz .loop_write_to_vmem
        neg  ecx
        mov  eax, 20
        mul  ecx
        div  ebx            ; x * 20 / 176
        shl  edx, 1         ; 2 times the remainder
        cmp  ebx, edx
        adc  eax, 0         ; round up the number, if remainder is fairly large (quotient has >.5 decimal part)
        mov  ecx, 5
        mul  ecx
        mov  ebx, 100
        xor  edx, edx
        div  ebx            ; the thing equals: (round up((num. video loops) * 20 / 176)) * 5 / 100
.out:
        retn


; Write16BytesToVideoMemory
;
; Taken from install.exe. It will write 16 bytes to video memory for
; benchmarking purposes.
;
Write16BytesToVideoMemory:
        mov  edi, 0xa0000
        push byte 15
        pop  ecx
.video_mem_write_loop:
        mov  eax, ecx
        mov  [edi], eax
        cmp  eax, 1
        jmp  short $+2
        loop .video_mem_write_loop
        inc  eax
        push eax
        pop  eax
        retn


; DexorAbout
;
; Dexors about string back to ascii.
;
DexorAbout:
        xor  ecx, ecx
        mov  cl, [aboutText]    ; first byte = number of strings
        mov  esi, aboutText + 1
        mov  edi, esi

.next_string:
        dec  ecx
        js   .out
        xor  edx, edx

.next_byte:
        inc  edx
        lodsb
        test al, al
        jz   .zero_terminate
        xor  al, dl
        stosb
        jmp  short .next_byte

.zero_terminate:
        stosb
        jmp  short .next_string

.out:
        retn


; WriteYULetters
;
; Support for Serbo-Croatian latin letters, called from SWOS
;
extern DrawBitmap
global WriteYULetters
WriteYULetters:
        mov  eax, [D0]          ; get letter
        cmp  al, 0xf0
        jz   .dj
        cmp  al, 0xd0
        jz   .dj
        cmp  al, 0xe6
        jz   .tj
        cmp  al, 0xc6
        jz   .tj
        cmp  al, 0xe8
        jz   .ch
        cmp  al, 0xc8
        jz   .ch
        cmp  al, 0x9e
        jz   .zh
        cmp  al, 0x8e
        jz   .zh
        cmp  al, 0x9a
        jz   .sh
        cmp  al, 0x8a
        jz   .sh
        jmp  .return

.dj:
        mov  dl, 'D'
        mov  esi, letter_dj_big
        cmp  dword [A3], conversion_table_small
        jnz  .draw_char
        mov  esi, letter_dj_small
        jmp  short .draw_char
.tj:
        mov  dl, 'C'
        mov  esi, accent_big
        cmp  dword [A3], conversion_table_small
        jnz  .draw_char
        mov  esi, accent_small
        jmp  short .draw_char
.ch:
        mov  dl, 'C'
        jmp  short .draw_hook
.zh:
        mov  dl, 'Z'
        jmp  short .draw_hook
.sh:
        mov  dl, 'S'            ; based on 'S'

.draw_hook:
        mov  esi, hook_big
        cmp  dword [A3], conversion_table_small
        jnz  .draw_char
        mov  esi, hook_small

.draw_char:
        xor  eax, eax
        xor  edi, edi
        mov  ax, [D1]           ; get x (eax)
        mov  di, [D2]           ; get y (edi)
        cmp  dl, 'D'
        jz   .dont_test         ; dj doesnt need extra rows
        cmp  di, 3              ; we need 3 extra pixels
        jb   .draw_base         ; if text is too high, draw common latin letter

.dont_test:
        mov  ebx, eax           ; ebx = x
        movzx ecx, byte [esi]   ; get width (ecx)
        inc  esi
        add  ebx, ecx           ; ebx = x + width
        cmp  bx, [D6]           ; can it fit?
        jb   .can_fit

        pop  eax                ; get rid of return location
        xor  eax, eax           ; set character to zero
        jmpa DrawMenuText + 0x53 ; jump to end of string test

.can_fit:
        push edx                ; save base letter
        mov  [D7], ebx          ; save next char x coordinate
        movzx ebx, byte [esi]   ; get height (ebx)
        inc  esi                ; esi -> letter data
        cmp  dl, 'D'
        jz   .dont_sub
        sub  edi, 3             ; make room for extra three lines, if not 'D'

.dont_sub:
        push esi
        push ebx
        mov  edx, edi
        call DrawBitmap

        pop  edx                ; dl = base letter
        cmp  dl, 'D'            ; dj is completely drawn
        jnz  .draw_base

        mov  eax, [D7]
        mov  [D1], eax          ; set new x coordinate
        pop  eax                ; get rid of return location
        jmpa DrawMenuText + 0x44 ; go get next character

.draw_base:
        movzx eax, dl
        mov  [D0], eax

.return:
        sub  byte [D0], 32
        retn


global FixGetTextSize
FixGetTextSize:
        mov  al, [D0]
        cmp  al, 'ð'
        jz   .fix_it
        cmp  al, 'Ð'
        jz   .fix_it
        sub  byte [D0], 32
        retn

.fix_it:
        mov  ax, 7
        cmp  dword [A3], conversion_table_small
        jz   .small_char
        mov  ax, 9
.small_char:
        mov  word [D0], 18
        retn


; data section
section .data
hook_big:
    db 8, 3                     ; 8 x 3
    db 0, 2, 2, 0, 2, 2, 8, 0
    db 0, 0, 2, 2, 2, 8, 8, 0
    db 0, 0, 0, 8, 8, 8, 0, 0

hook_small:
    db 6, 3                     ; 6 x 3
    db 0, 2, 8, 0, 2, 8
    db 0, 0, 2, 2, 8, 8
    db 0, 0, 0, 8, 8, 0

accent_big:
    db 8, 3                     ; 8 x 3
    db 0, 0, 0, 0, 2, 2, 8, 0
    db 0, 0 ,0, 2, 2, 8, 0, 0
    db 0, 0, 0, 8, 8, 8, 0, 0

accent_small:
    db 6, 3                     ; 6 x 3
    db 0, 0, 0, 0, 2, 8
    db 0, 0, 0, 2, 8, 0
    db 0, 0, 0, 8, 0, 0

letter_dj_big:
    db 9, 8                     ; 9 x 8
    db 0, 2, 2, 2, 2, 2, 2, 0, 0
    db 0, 2, 2, 8, 8, 8, 2, 2, 0
    db 0, 2, 2, 8, 0, 0, 2, 2, 8
    db 2, 2, 2, 2, 2, 8, 2, 2, 8
    db 8, 2, 2, 8, 8, 8, 2, 2, 8
    db 0, 2, 2, 8, 0, 0, 2, 2, 8
    db 0, 2, 2, 2, 2, 2, 2, 8, 8
    db 0, 0, 8, 8, 8, 8, 8, 8, 0

letter_dj_small:
    db 7, 6                     ; 7 x 6
    db 0, 2, 2, 2, 2, 0, 0
    db 0, 2, 8, 8, 8, 2, 0
    db 2, 2, 2, 8, 0, 2, 8
    db 0, 2, 8, 0, 0, 2, 8
    db 0, 2, 2, 2, 2, 0, 8
    db 0, 0, 8, 8, 8, 8, 0

badOS:
    db "Warning: This Windows version is not supported.", 13, 10
    db "Press ENTER to continue", 13, 10, "$", 0


; bss section
section .bss
newStack:       resb 64 * 1024  ; increase stack so we can go wild with recursion ;)
newStackTop:
codeSize:       resd 1
codeHandle:     resd 1
dataSeg:        resw 1
%ifdef DEBUG
codeBase:       resd 1
%endif