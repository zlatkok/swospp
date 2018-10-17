[list -]
%include "swos.inc"
%include "swsmenu.inc"
[list +]

section .text

bits 32

global CheckForFastReplay
CheckForFastReplay:
%ifdef DEBUG
        extern DumpVariables
        call DumpVariables          ; write out debug variables
%endif
        test byte [replayStatus], 1
        jz   .out

        test byte [replayStatus], 4
        jz   .out

        xor  byte [rplFrameSkip], 1
        test byte [rplFrameSkip], 1
        jz   .out

        pop  eax                    ; get rid of return address

.out:
        cmp  word [EGA_graphics], -1
        retn


global MyHilPtrIncrement
MyHilPtrIncrement:
        mov  eax, [nextGoalPtr]
        cmp  [A1], eax
        jnz  .out

        mov  bx, [rplHandle]
        test bx, bx
        jz   .reset_ptr

        mov  cx, 19000
        mov  edx, [goalBasePtr]
        mov  ah, 0x40
        int  0x21
        jc   .set_error

        cmp  ax, 19000
        jz   .reset_ptr

.set_error:
        mov  byte [saveErrors], 1

.reset_ptr:
        mov  eax, [goalBasePtr]
        mov  [A1], eax

.out:
        retn


global MyHilPtrAdd
MyHilPtrAdd:
        mov  eax, [nextGoalPtr]
        cmp  [A0], eax
        jnz  .out

        test byte [replayStatus], 1
        jz   .reset_ptr

        mov  bx, [rplHandle]
        mov  cx, 19000
        mov  edx, [goalBasePtr]
        mov  ah, 0x3f
        int  0x21

.reset_ptr:
        mov  eax, [goalBasePtr]
        mov  [A0], eax

.out:
        retn


global OpenReplayFile
OpenReplayFile:
        mov  word [rplHandle], 0    ; rplHandle will be zero in case of an error
        mov  byte [saveErrors], 0
        mov  edx, tmp_filename
        xor  ecx, ecx
        mov  ah, 0x3c
        int  0x21                   ; try to open file for writing
        jc   .out

        mov  [rplHandle], ax
        mov  bx, ax
        xor  cx, cx
        mov  dx, 3626               ; leave room for header (3626 bytes)
        mov  ax, 0x4200
        int  0x21
        jc   .close_file

        or   byte [replayStatus], 2 ; set recording flag
        jmp  .out

.close_file:
        mov  ah, 0x3e               ; if can't seek, close file
        int  0x21
        mov  word [rplHandle], 0    ; set rplHandle to zero to indicate error

.out:
        mov  dword [A0], g_currentMenu
        retn


; CloseReplayFile
;
; in:
;      eax - if true it's normal call, just retn, otherwise called from SWOS
;
global CloseReplayFile
CloseReplayFile:
        push eax
        sub  esp, byte 4
        mov  esi, rplHandle
        mov  bx, [esi]
        test bx, bx
        jz   near .out                  ; if file wasn't opened, nothing to do

        inc  byte [saveErrors]          ; anticipate error
        mov  eax, [currentHilPtr]
        cmp  eax, [nextGoalPtr]
        jz   .nothing_left

        mov  ecx, eax
        mov  edx, [goalBasePtr]
        sub  ecx, edx
        mov  ah, 0x40
        int  0x21                       ; save remaining stuff
        jc   .close_file

.nothing_left:
        xor  cx, cx
        xor  dx, dx
        mov  ax, 0x4201
        int  0x21                       ; get current position
        jc   .close_file

        mov  [esp], ax                  ; save current position
        mov  [esp + 2], dx

        xor  dx, dx
        xor  cx, cx
        mov  ax, 0x4200                 ; seek to beginning
        int  0x21
        jc   .close_file

        push word [hilNumGoals]         ; save number of goals for normal hils
        mov  word [hilNumGoals], 1      ; important - this will look just like one highlight

        mov  ah, 0x40
        mov  ecx, 3626
        mov  edx, hilFileBuffer
        int  0x21                       ; write hil header
        pop  word [hilNumGoals]         ; restore original number of goals
        jc   .close_file

        mov  dx, [esp]                  ; restore previous position
        mov  cx, [esp + 2]
        mov  ax, 0x4200
        int  0x21

        mov  dword [esi], -1
        push byte 4
        mov  ah, 0x40
        mov  edx, esi
        pop  ecx
        int  0x21                       ; save replay terminator (-1)
        jc   .close_file

        or   byte [replayStatus], 8     ; all OK, we have a replay - set flag
        mov  esi, tmp_filename
        mov  edi, selectedFile
        push byte 13
        pop  ecx
        rep  movsb
        dec  byte [saveErrors]          ; restore errors as it was

.close_file:
        mov  ah, 0x3e
        int  0x21                       ; close file
        mov  word [rplHandle], 0        ; clear handle

.out:
        and  byte [replayStatus], ~2    ; clear recording flag
        add  esp, byte 4
        pop  eax
        test eax, eax
        jnz  .retn

        mov  ax, [isGameFriendly]       ; recreate the test
        or   ax, ax

.retn:
        retn


global ShowReplaysMenu
ShowReplaysMenu:
        mov  dword [A6], replaysMenu
        calla ShowMenu
        retn


InitReplays:
        xor  ebx, ebx
        mov  al, [replayStatus]
        and  al, 8
        cmp  bl, al                 ; al greater than zero?
        sbb  ebx, ebx               ; ebx = al > 0 ? -1 : 0
        neg  ebx                    ; ebx = al > 0 ? 1 : 0
        mov  ecx, ebx
        mov  edx, ebx
        shl  ecx, 2
        add  ecx, edx               ; ecx = al > 0 ? 5 : 0
        add  cl, 9                  ; ecx = al > 0 ? 14 : 9
        neg  ebx                    ; ebx = al > 0 ? -1 : 0
        not  ebx                    ; ebx = al > 0 ? 0 : -1
        neg  ebx                    ; ebx = al > 0 ? 0 : 1
        mov  edi, ebx               ; CalcMenuEntryAddress trashes bx
        mov  [D0], word 2
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  [esi + MenuEntry.isDisabled], di           ; disable or enable view/save replay buttons
        mov  [esi + MenuEntry.backAndFrameColor], cx    ; set color - green or gray for disabled entry
        mov  [D0], word 4
        calla CalcMenuEntryAddress
        mov  esi, [A0]
        mov  [esi + MenuEntry.isDisabled], di
        mov  [esi + MenuEntry.backAndFrameColor], cx

        mov  al, [saveErrors]                          ; check rpl file errors
        test al, al
        jz   .out

        mov  dword [A0], err_msg
        calla ShowErrorMenu

.out:
        retn


ViewReplay:
        mov  edx, selectedFile
        mov  ax, 0x3d00
        int  0x21
        jnc  play_it

.cant_open:
        mov  dword [A0], cant_open_str
        calla ShowErrorMenu
        retn

play_it:
        mov  [rplHandle], ax
        mov  bx, ax
        mov  ecx, 19000 + 3626
        mov  edx, hilFileBuffer
        mov  ah, 0x3f
        int  0x21
        jc   read_error

        test eax, eax
        jz   read_error                 ; check for zero-sized file

        cmp  eax, ecx
        jnz  read_error                 ; check for too small filesize

        mov  edi, hilFilename
        mov  esi, selectedFile
        push byte 13
        pop  ecx
        rep  movsb

        and  byte [replayStatus], ~4    ; turn off fast replay
        or   byte [replayStatus], 1     ; set playing flag
        calla ShowHighlights
        and  byte [replayStatus], ~1
        mov  bx, [rplHandle]            ; close file
        mov  ah, 0x3e
        int  0x21
        mov  word [hilNumGoals], 0      ; reset highlights
        retn

read_error:
        mov  ah, 0x3e
        int  0x21
        mov  dword [A0], err_reading
        calla ShowErrorMenu
        retn


LoadReplay:
        mov  eax, load_replay_str
        mov  [A0], eax
        mov  [A1], eax
        mov  dword [D0], 'LPR.'
        calla GetFilenameAndExtension
        jnz  .out

        mov  esi, [A0]
        mov  edi, selectedFile
        push byte 13
        pop  ecx
        rep  movsb                      ; copy selected name to our buffer
        or   byte [replayStatus], 8

.out:
        jmp  InitReplays


SaveReplay:
        mov  eax, saveFilename          ; zero out default save name
        mov  byte [eax], 0
        mov  [A0], eax
        mov  dword [A1], save_replay_str
        mov  dword [D0], 'LPR.'
        calla SelectFileToSaveDialog
        jnz  near .out
        mov  esi, [A0]
        mov  edi, selectedFile
        push byte 13
        pop  ecx
        rep  cmpsb                      ; compare selected file with destination
        jne  .copy

        mov  dword [A0], cant_copy_to_itself
        calla ShowErrorMenu
        jmp  short SaveReplay

.copy:
        mov  edx, selectedFile
        mov  ax, 0x3d00
        int  0x21
        jc   ViewReplay.cant_open

        mov  esi, eax
        mov  edx, [A0]
        xor  ecx, ecx
        mov  ah, 0x3c
        int  0x21
        jc   ViewReplay.cant_open

        mov  edi, eax
        mov  edx, foundFilenamesBuffer

.copy_loop:
        mov  ebx, esi
        mov  cx, 0x4000
        mov  ah, 0x3f
        int  0x21
        jc   read_error

        test eax, eax
        jz   .copy_over

        mov  ecx, eax
        mov  ah, 0x40
        mov  ebx, edi
        int  0x21
        jnc  .copy_loop

        mov  dword [A0], err_writing
        calla ShowErrorMenu

.copy_over:
        mov  ebx, esi
        mov  ah, 0x3e
        int  0x21
        mov  ebx, edi
        int  0x21

.out:
        retn


section .bss

rplHandle:
    resd 1
selectedFile:
    resb 13
saveFilename:
    resb 13
saveErrors:
    resb 1

; bit 0 set = playing                (1)
; bit 1 set = recording              (2)
; bit 2 set = fast replay            (4)
; bit 3 set = we have replay to show (8)
;
global replayStatus
replayStatus:
    resb 1

global rplFrameSkip
rplFrameSkip:
    resb 1


section .rdata

load_replay_str:
    db "LOAD REPLAY", 0
save_replay_str:
    db "SAVE REPLAY", 0
tmp_filename:
    db "LASTGAME.RPL", 0
err_msg:
    db "WARNING: FILE MAY BE CORRUPTED", 0
cant_open_str:
    db "FILE COULD NOT BE OPENED", 0
err_reading:
    db "ERROR READING FILE", 0
err_writing:
    db "ERROR WRITING FILE", 0
cant_copy_to_itself:
    db "CANT COPY FILE TO ITSELF", 0
error_copying:
    db "ERROR COPYING FILE", 0


section .data

    StartMenu replaysMenu, InitReplays, 0, 0, 3

        StartEntry 92, 0, 120, 15
            EntryColor 0x17
            EntryString 0, "REPLAYS"
        EndEntry

        StartEntry 102, 185, 100, 15
            NextEntries -1, -1, 4, -1
            EntryColor  12
            EntryString 0, aExit
            OnSelect    SetExitMenuFlag
        EndEntry

        StartEntry 82, 60, 140, 15
            NextEntries -1, -1, -1, 3
            EntryColor  14
            EntryString 0, "VIEW REPLAY"
            OnSelect    ViewReplay
        EndEntry

        StartEntry 82, 90, 140, 15
            NextEntries -1, -1, 2, 4
            EntryColor  14
            EntryString 0, "LOAD REPLAY"
            OnSelect    LoadReplay
        EndEntry

        StartEntry 82, 120, 140, 15
            NextEntries -1, -1, 3, 1
            EntryColor  14
            EntryString 0, "SAVE REPLAY"
            OnSelect    SaveReplay
        EndEntry
    EndMenu
