; in:
;      ebp -  base of data
;      edx -> abort string ('$' terminated)
;
; Clears screen, writes message in edx, and aborts SWOS
;

AbortWithMessage:
    push edx
    mov  ax, 2
    int  0x10                   ; set text mode, clearing the screen
    mov  ah, 9
    lea  edx, [ebp + swospp]
    int  0x21                   ; print SWOSPP:
    pop  edx
    int  0x21                   ; print the error message
    mov  eax, [ebp + swos_code_base]
    add  eax, EndProgram2
    jmp  eax                    ; go end the program

swospp: db "SWOSPP: $"


; in:
;      eax - memory size requested
;      ebp - base of data
;
; out:
;      eax -> allocated memory
;      ebx -  memory handle
;
; If there is error allocating memory, program will be terminated with
; appropriate message.
;

SafeAlloc:
    push esi
    push edi
    push ecx
    xor  ebx, ebx
    xor  ecx, ecx
    mov  cx, ax
    shr  eax, 16
    mov  bx, ax                 ; bx:cx - block size
    mov  ax, 0x0501             ; allocate memory for code & data
    int  0x31
    jc   .out_of_mem
    mov  ax, bx
    shl  eax, 16
    mov  ax, cx                 ; eax -> allocated memory (bx:cx)
    mov  bx, si
    shl  ebx, 16
    mov  bx, di                 ; ebx - memory handle (si:di)
    pop  ecx
    pop  edi
    pop  esi
    retn

.out_of_mem:
    lea  edx, [ebp + out_of_memory]
    jmp  short AbortWithMessage


; in:
;      eax - handle of block to free
;
; Frees allocated block whose handle is in eax.
;

Free:
    push esi
    push edi
    push eax
    mov  esi, eax
    mov  edi, eax
    shr  esi, 16
    and  edi, 0xffff
    mov  ax, 0x0502
    int  0x31
    pop  eax
    pop  edi
    pop  esi
    retn


; in:
;      eax - position to move to
;      ebx - file handle
;      ebp - base of data
;
; out:
;      eax - new file position
;
; If file can't be seeked to, program will be terminated with appropriate
; message.

SafeFseek:
    push ecx
    push edx
    mov  dx, ax
    shr  eax, 16
    mov  cx, ax                 ; cx:dx - offset to seek to
    mov  ax, 0x4200
    int  0x21
    jc   .seek_error
    shl  edx, 16
    or   eax, edx               ; eax - new offset
    pop  edx
    pop  ecx
    retn

.seek_error:
    lea  edx, [ebp + io_error]
    jmp  AbortWithMessage


; in:
;      ebx - file handle
;      ebp - base of data
;      ecx - number of bytes to read
;      edx - pointer to buffer to read to
;
; Reads from file 64k at a time. If some error happens during read operation,
; program will be terminated with appropriate message.

SafeFread:
    push eax

    mov  ah, 0x3f
    int  0x21
    jc   .read_error

    pop  eax
    retn

.read_error:
    lea  edx, [ebp + read_error]
    jmp  AbortWithMessage