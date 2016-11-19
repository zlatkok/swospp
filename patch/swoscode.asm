; fiktivni fajl - ovo je citljiva verzija koda koji se ubacuje u SWOS
; ukratko - ucitati loader.bin u pitchDatBuffer i pozvati ga
; vrlo cudno - ako pridodam data sekciji atribut executable (sto mi se cini
; logicnim da bih mogao da izvrsim svoj kod koji ucitavam u pitchDatBuffer)
; SWOS puca pri pristupu alociranoj memoriji. Cini mi se da je to bag glupog
; DOS4GW extendera, a kod se izvrsava u data segmentu bez ikakvih problema (i
; bez executable atributa)

bits 32

pitchDatBuffer   equ 0x5535e
LoadFile         equ 0xa1a8
SWOS             equ 0x5758
stack_top        equ 0xb16eb
code_base        equ 0
data_base        equ 0

tmp01 equ 0x3146d
tmp02 equ 0x31471
tmp03 equ 0x31475
tmp04 equ 0x31479
tmp05 equ 0x3147d
tmp06 equ 0x31481
tmp07 equ 0x31485
tmp08 equ 0x31489
tmp09 equ 0x3148d
tmp10 equ 0x31491
tmp11 equ 0x31495
tmp12 equ 0x31499
tmp13 equ 0x3149d
tmp14 equ 0x314a1
tmp15 equ 0x314a5

; ubacujemo na 0x54f4, preko "SAVE DISK FILING"
filename:
    db "loader.bin"

; patchujemo funkciju DumpTimerVariables na 0xa929
start:
    ;int  1                          ; za debug
    nop
    nop
    mov  [stack_top], esp           ; ovo cemo odmah uraditi u slucaju da
                                    ; loader.bin nije pronadjen
    pushfd
    pushad                          ; cuvamo sve registre i flegove
    mov  ebx, data_base             ; ebx ce biti bazni registar za pristup
                                    ; promenljivama, da ustedimo na fixup
                                    ; rekordima
    push dword [ebx + tmp01]
    push dword [ebx + tmp02]
    push dword [ebx + tmp09]        ; sacuvacemo za svaki slucaj i sve
    push dword [ebx + tmp10]        ; koriscene pseudo-registre
    lea  eax, [ebx + filename]
    mov  [ebx + tmp09], eax         ; tmp09 -> filename
    lea  eax, [ebx + pitchDatBuffer]
    mov  [ebx + tmp10], eax         ; tmp10 -> buffer (pitchDatBuffer)
                                    ; pitchDatBuffer je odabran jer se
                                    ; inicijalizuje na nulu pre svake partije
    call LoadFile                   ; ukoliko ne nadje fajl, funkcija prekida
                                    ; program
    mov  eax, [ebx + tmp02]         ; tmp02 = duzina fajla
    cmp  eax, 10032                 ; velicina pitchDatBuffer-a
    jbe  .size_ok
.endless_loop:
    int  3
    jmp  short .endless_loop        ; njihov sistem
.size_ok:
    mov  ecx, ebx
    mov  eax, ebx                   ; eax = bazna relokacija podataka
    mov  ebx, code_base             ; ebx = bazna relokacija koda
    add  ecx, pitchDatBuffer
    ;int  1                          ; za debug
    nop
    nop
    call ecx                        ; startuj loader
    pop  dword [tmp10]
    pop  dword [tmp09]
    pop  dword [tmp02]
    pop  dword [tmp01]
    popad
    popfd                           ; sada sve je cisto
    ;int 1
    nop
    nop
    jmp  SWOS                       ; nastavi sa normalnim izvrsavanjem
    retn
    retn
    retn                            ; onako bez veze, da popunim do sledece
    retn                            ; instrukcije