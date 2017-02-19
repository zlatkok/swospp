org 0x100

start:

; variables - startup code will be overwritten
menu_result    equ start + 0

        cld
        mov  dx, introstr
        call PrintString

        mov  dx, menustr
        call PrintString

.wait_for_key:
        call GetKey
        cmp  al, '1'
        jz   .open_file
        cmp  al, '2'
        jz   .open_file
        cmp  al, '3'
        jz   .write_about
        cmp  ax, 0x011b
        jnz  .wait_for_key
        xor  ax, ax
        call Exit

.write_about:
        mov  dx, aboutstr
        call PrintString
        mov  dx, menustr
        call PrintString
        jmp  .wait_for_key

.open_file:
        mov  [menu_result], al

        mov  si, swos_exe_names

.try_next_filename:
        mov  dx, [si]
        test dx, dx
        jnz  .got_filename

        mov  dx, no_file
        call MsgAndQuit

.got_filename:
        call OpenFileForWrite
        jnc  .opened_ok

        inc  si
        inc  si
        jmp  .try_next_filename

.opened_ok:
        mov  bx, ax                 ; handle must go to bx
        mov  al, 2                  ; check SWOS executable filesize
        xor  cx, cx
        xor  dx, dx
        call SeekFile

        cmp  dx, 0x0020
        jnz  .size_error
        cmp  ax, 0x942f
        jz  .patch_loop_init

.size_error:
        mov  dx, size_mismatch
        call PrintString

.patch_loop_init:
        mov  di, offsets
        mov  si, lengths
        mov  dx, orig_data
        cmp  byte [menu_result], '1'
        jnz  .patch_loop
        mov  dx, patch_data

.patch_loop:
        xor  ax, ax
        push dx
        mov  dx, [di]
        mov  cx, [di + 2]
        call SeekFile
        pop  dx
        jc   .patch_error
        xor  cx, cx
        mov  cl, [si]
        inc  si
        call WriteFile
        jc   .patch_error
        add  dx, cx
        add  di, 4
        cmp  di, offsets_end
        jb   .patch_loop

        call CloseFile
        mov  dx, patched_ok
        call PrintString
        xor  ax, ax
        call Exit

.patch_error:
        int  1
        call CloseFile
        mov  dx, patch_error
        call MsgAndQuit


introstr:
        db "SWOS++ patcher/unpatcher", 13, 10
        db "by Zlatko Karakas <zlatko.karakas@gmail.com>", 13, 10, 13, 10, "$"
menustr:
        db "1)   - install", 13, 10
        db "2)   - deinstall", 13, 10
        db "3)   - about SWOS++", 13, 10
        db "ESC) - quit", 13, 10, 13, 10, "$"
aboutstr:
        db "SWOS++ is an add-on for SWOS. It enables many features originally ", 13, 10
        db "unavailable, such as network games and full match recordings.", 13, 10,
        db "Created by Zlatko Karakas <zkz@eunet.rs>/<zlatko.karakas@gmail.com>", 13, 10, 13, 10, "$"

; various crazy names for SWOS executable out there... and it's only gotten worse with time :P
swos_exe:
        db "swos.exe", 0        ; Franky SWOS
sws_exe:
        db "sws.exe", 0         ; original SWOS name
underdogs_swos:
        db "sws!!!_!.exe", 0
swsengpp_exe:
        db "swsengpp.exe", 0

align 4
swos_exe_names:
        dw underdogs_swos   ; first try the Underdogs SWOS version exe, because they seem to have swos.exe too
        dw swos_exe
        dw swsengpp_exe
        dw sws_exe          ; at the end, try original SWOS name
        dw 0

no_file:
        db "Can't open SWOS executable.", 13, 10, "$"
size_mismatch:
        db "Warning: size mismatch! File might not be a SWOS executable."
        db 13, 10, "$"
patch_error:
        db "Error while patching!", 13, 10, "$"
patched_ok:
        db "Patched OK.", 13, 10, "$"

    %include "pdata.asm"
    %include "util.asm"
