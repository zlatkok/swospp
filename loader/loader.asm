; SWOSPP loader for version 1.0
;
; started: 21.08.2003. v1.0
; Karakas Zlatko
;

; pitchDatBuffer is our base
%define BASE 0x5535e

; ZK Binary Format structure
struc ZK_header
    .signature:   resd 1
    .ver_major:   resw 1
    .ver_minor:   resw 1
    .header_size: resd 1
    .checksum:    resd 1
    .code_ofs:    resd 1
    .code_size:   resd 1
    .patch_ofs:   resd 1
    .patch_size:  resd 1
    .reloc_ofs:   resd 1
    .reloc_size:  resd 1
    .bss_size:    resd 1
    .entry_point: resd 1
endstruc

%define ZK_SIG 'ZKBF'
%define VERSION 1

org BASE
bits 32

%include "misc.inc"

; in:
;      eax - data base
;      ebx - code base
;
start:
    nop                         ; alignment
    nop
    jmp code_start

;
; data
;

xalign 4, BASE, ' '

zkh:             istruc ZK_header
                 db "SWOS++ loader v1.0 by Zlatko Karakas "
                 db "[DO NOT EDI"
                 iend
swos_data_base:  dd 'T - '
swos_code_base:  dd 'CONT'
reloc_ptr:       dd 'AINS'
reloc_handle:    dd ' COD'
                 db 'E] ', 0
filename:        db "swospp.bin", 0
cant_open_file:  db "Can't open swospp.bin.", 13, 10, "$"
file_corrupt:    db "swospp.bin is corrupted.", 13, 10, "$"
invalid_version: db "Can't load this version of binary format."
                 db 13, 10, "$"
out_of_memory:   db "Out of memory error.", 13, 10, "$"
io_error:        db "I/O error.", 13, 10, "$"
read_error:      db "Error reading file.", 13, 10, "$"
checksum_failed: db "File integrity violation.", 13, 10, "$"
code_ptr:        dd ' SWO'
code_handle:     dd 'S RU'
patch_ptr:       dd 'LEZ!'
patch_handle:    dd '!!  '


;
; start of code
;

code_start:
        xalign 4, BASE
        mov  [eax + swos_data_base], eax ; first store code and data base
        mov  [eax + swos_code_base], ebx
        mov  ebp, eax               ; ebp will be used for data referencing

        lea  edx, [ebp + filename]  ; try to open main file
        mov  ax, 0x3d00
        int  0x21
        jc   near .error_opening
        mov  ebx, eax               ; move handle to ebx

        mov  ah, 0x3f
        mov  ecx, ZK_header_size
        lea  edx, [ebp + zkh]
        int  0x21                   ; try reading header
        jc   near .file_corrupt

        cmp  eax, ZK_header_size    ; check for truncated file
        jnz  near .file_corrupt

        lea  esi, [ebp + zkh]
        cmp  dword [esi + ZK_header.signature], ZK_SIG
        jnz  near .file_corrupt     ; check signature

        cmp  word [esi + ZK_header.ver_major], VERSION
        ja   near .invalid_version  ; check major version

        cmp  dword [esi + ZK_header.header_size], ZK_header_size
        jb   near .invalid_version  ; hdr size must never be smaller than ours

        ; load code

        mov  eax, [esi + ZK_header.code_ofs]
        call SafeFseek              ; seek to code offset

        mov  eax, [esi + ZK_header.code_size]
        push eax
        push ebx
        add  eax, [esi + ZK_header.bss_size]
        call SafeAlloc              ; allocate memory for code & data + bss
        mov  [ebp + code_ptr], eax
        mov  [ebp + code_handle], ebx
        pop  ebx

        mov  edx, eax
        pop  ecx
        call SafeFread              ; read in code and data

        ; load patch data

        mov  eax, [esi + ZK_header.patch_ofs]
        call SafeFseek              ; seek to patch data offset

        mov  eax, [esi + ZK_header.patch_size]
        push eax
        push ebx
        call SafeAlloc              ; allocate memory for patch data
        mov  [ebp + patch_ptr], eax
        mov  [ebp + patch_handle], ebx
        pop  ebx

        mov  edx, eax
        pop  ecx
        call SafeFread              ; read in patch data

        ; load relocations

        mov  eax, [esi + ZK_header.reloc_ofs]
        call SafeFseek              ; seek to relocations offset

        mov  eax, [esi + ZK_header.reloc_size]
        push eax
        push ebx
        call SafeAlloc              ; allocate memory for relocations
        mov  [ebp + reloc_ptr], eax
        mov  [ebp + reloc_handle], ebx
        pop  ebx

        mov  edx, eax
        pop  ecx
        call SafeFread              ; read in relocations

        mov  ah, 0x3e
        int  0x21                   ; close the file

        ; calculate checksum of file
        xor  eax, eax
        xor  ebx, ebx
        mov  edi, [ebp + code_ptr]
        mov  ecx, [esi + ZK_header.code_size]
        call Checksum               ; checksum code & data

        mov  edi, [ebp + patch_ptr]
        mov  ecx, [esi + ZK_header.patch_size]
        call Checksum               ; checksum patch data

        mov  edi, [ebp + reloc_ptr]
        mov  ecx, [esi + ZK_header.reloc_size]
        call Checksum               ; checksum relocations

        cmp  eax, [esi + ZK_header.checksum]
        jnz  near .checksum_failed  ; compare with checksum in header

        ; fixup code, data and patch data

        ; (1) fixup code, adding SWOS code base value
        mov  edi, [ebp + code_ptr]
        mov  ebx, [ebp + reloc_ptr]
        mov  edx, [ebp + swos_code_base]
        call Fixup

        ; (2) fixup code, adding SWOS data base value
        mov  edx, [ebp + swos_data_base]
        call Fixup

        ; (3) fixup code, adding load address
        mov  edx, [ebp + code_ptr]
        call Fixup

        ; (4) fixup patch data, adding SWOS code base value
        mov  edi, [ebp + patch_ptr]
        mov  edx, [ebp + swos_code_base]
        call Fixup

        ; (5) fixup patch data, adding SWOS data base value
        mov  edx, [ebp + swos_data_base]
        call Fixup

        ; (6) fixup patch data, adding load address
        mov  edx, [ebp + code_ptr]
        call Fixup

        ; relocations are not needed any more, free them
        mov  eax, [ebp + reloc_handle]
        call Free

        ; patch SWOS in memory

        mov  edi, [ebp + patch_ptr] ; edi -> patch data

.do_patch_record:
        xor  ecx, ecx
        mov  cl, [edi]              ; read count byte
        inc  edi
        mov  ebx, [edi]             ; read address
        add  edi, 4
        cmp  ebx, -1                ; address -1 terminates patching
        jz   .patch_ended

        test cl, cl                 ; if count byte = 0, it is fill record
        jz   .patch_fill

.patch_byte:
        mov  al, [edi]              ; patch those bytes
        mov  [ebx], al
        inc  edi
        inc  ebx
        dec  ecx
        jnz  .patch_byte
        jmp  .do_patch_record

.patch_fill:
        mov  cl, [edi]              ; repeat byte follows address
        inc  edi
        mov  al, [edi]              ; then the fill byte
        inc  edi
.fill_byte:
        mov  [ebx], al              ; fill memory with data byte
        inc  ebx
        dec  ecx
        jnz  .fill_byte
        jmp  .do_patch_record

.patch_ended:
        mov  eax, [ebp + patch_handle]
        call Free                   ; free patch data

        mov  ecx, [esi + ZK_header.bss_size]
        mov  edi, [ebp + code_ptr]
        add  edi, [esi + ZK_header.code_size]
        xor  eax, eax
        mov  edx, ecx
        shr  ecx, 2
        rep  stosd                  ; zero fill bss section
        and  edx, 3
        mov  ecx, edx
        rep  stosb

        mov  eax, [ebp + code_ptr]
        mov  ebx, eax               ; ebx = load address
        add  eax, [esi + ZK_header.entry_point] ; eax -> init routine
        mov  ecx, [esi + ZK_header.code_size]
        add  ecx, [esi + ZK_header.bss_size] ; ecx = code + data + bss size
        mov  edx, [ebp + code_handle] ; edx = code block memory handle
        call eax                    ; call init routine, ebp -> SWOS data
        cld                         ; guarantee cleared direction flag at start
        retn                        ; return to SWOS

.file_corrupt:
        lea  edx, [ebp + file_corrupt]
        jmp  short .close_file
.invalid_version:
        lea  edx, [ebp + invalid_version]
        jmp  short .close_file
.error_opening:
        lea  edx, [ebp + cant_open_file]
        jmp  short AbortWithMessage
.checksum_failed:
        lea  edx, [ebp + checksum_failed]
        mov  eax, [ebp + reloc_handle]
        call Free
        mov  eax, [ebp + patch_handle]
        call Free
        mov  eax, [ebp + code_handle]
        call Free
.close_file:
        mov  ah, 0x3e
        int  0x21                   ; close the file
        jmp  short AbortWithMessage


; ----

; Checksum
;
; in:
;      eax -  checksum so far
;      ebx -  index so far
;      edi -> memory block to checksum
;      ecx -  length of memory block
;
; out:
;      eax -  new checksum
;      ebx -  new index
;
Checksum:
        push edx
@@
        movzx edx, byte [edi]
        inc  edi
        add  edx, ebx
        inc  ebx
        add  eax, edx
        rol  eax, 1
        xor  eax, edx
        dec  ecx
        jnz  @B
        pop  edx
        retn


; Fixup
;
; in:
;      ebx -> relocations section (current offset)
;      edi -> memory block to fix
;      edx -  value to add
;
Fixup:
        push eax
        push ecx

.fix_loop:
        mov  eax, [ebx]             ; get reloc offset
        add  ebx, 4
        cmp  eax, byte -1           ; -1 marks end of section
        jz   .out
        add  [edi + eax], edx       ; fixup location
        jmp  .fix_loop

.out:
        pop  ecx
        pop  eax
        retn

; various interface and helper routines
%include "util.asm"