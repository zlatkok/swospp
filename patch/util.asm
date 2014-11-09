; PrintString
;
; in:
;      dx -> string to print ("$" terminated)

PrintString:
        mov  ah, 0x09
        int  0x21
        retn


; GetKey
;
; returns: ah - scan code, al - ascii code, or zero
;
; Does not return until a key is pressed.

GetKey:
        mov  ah, 0x10
        int  0x16
        retn


; Exit
;
; in:
;      al - return code
;
; End program with return code in al

Exit:
        mov  ah, 0x4c
        int  0x21


; OpenFileForWrite
;
; in:
;      dx -> filename, zero terminated
;
; out:
;      carry flag - set if error, clear if ok
;      ax - error code if carry flag set
;
; Opens file for writing. Sets carry flag in case of an error.

OpenFileForWrite:
        mov  ax, 0x3d01
        int  0x21
        retn


; SeekFile
;
; in:
;      al - origin of move
;           0 - beginning of file plus offset
;           1 - current location plus offset
;           2 - end of file plus offset
;      bx - file handle
;      cx:dx - offset to seek to
;
; out:
;      ax - error code if carry flag set
;      dx:ax - new pointer location if carry flag clear
;      carry flag - set if error, clear if ok
;
; Seeks to specified location in file. Sets carry flag in case of an error.

SeekFile:
        mov  ah, 0x42
        int  0x21
        retn


; CloseFile
;
; in:
;      bx - file handle
;
; Closes file.

CloseFile:
        mov  ah, 0x3e
        int  0x21
        retn

; WriteFile
;
; in:
;      bx - file handle
;      cx - number of bytes to write
;      ds:dx -> write buffer
;
; Writes cx number of bytes from buffer pointed to by ds:dx. Sets carry flag
; in case of an error.

WriteFile:
        mov  ah, 0x40
        int  0x21
        retn


; MsgAndQuit
;
; in:
;      dx -> quitting message ("$" terminated)
;
; Writes a message in dx, and quits with return code 1

MsgAndQuit:
        call PrintString
        mov  al, 1
        call Exit