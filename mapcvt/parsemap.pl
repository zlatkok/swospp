# Made specifically for SWOS++
# Assumes map file is correct.
# Also contains (generates) various useful NASM macros.

$make68kRegsUnions = 0; # turn this on to declare 68k registers as unions

defined($ARGV[0]) or die "Usage $0 <map file>\n";
$fileName = shift;
open IN, "<$fileName" or die "Can't open `$fileName' for reading!\n";
open SWOS_ASM, ">swos.asm" or die "Can't open `swos.asm' for writing!\n";
open SWOS_INC, ">swos.inc" or die "Can't open `swos.inc' for writing!\n";
open SWOSSYM_H, ">swossym.h" or die "Can't open `swossym.h' for writing!\n";
open FILT, "<filter.txt" or die "Can't open filter file `filter.txt'!\n";

# form a hash of allowed symbols and their attributes, from our filter file
# everything will be stored to %symbols when done, key will be variable name
# '###' comments will pass over to generated .h file
while (<FILT>) {
    $line++;
    chomp;
    /\s*\#\#\#(.*)/;        # remember '###' comments
    $lastPassOverComment .= "$1\n" if (defined($1));
    s/\s*([^#]*)#.*/$1/;    # remove ordinary comments
    next if !length;        # skip entirely commented or empty lines
    @tokens = split(',', $_);
    @tokens = map { s/^\s+|\s+$//g; $_; } @tokens;  # remove whitespaces

    if ($#tokens >= 0) {
        $#tokens != 1 || length($tokens[1]) or die "Symbol attributes missing at line $line.\n";

        $symbols{$tokens[0]} and die "Duplicate symbol `$tokens[0]' found at line $line.\n",
            "Previous occurence at line ", $symbols{$tokens[0]}->{'line'}, ".\n";
        $symbols{$tokens[0]} = {'line' => $line};
        $symbols{$tokens[0]}->{'comment'} = $lastPassOverComment if defined($lastPassOverComment);

        $lastPassOverComment = undef;
        $i = 1;
        next if @tokens < 2;                # no prefix or type

        if ($tokens[1] =~ /prefix\s*(.*)/) {
            length $1 or die "Missing prefix at line $line.\n";
            $symbols{$tokens[0]}->{'prefix'} = $1;
            $i++;
        }

        next if $i > $#tokens;              # only got prefix - that's valid too

        $symbols{$tokens[0]}->{'C'} = 1;    # register as a C symbol
        $tokens[$i] =~ /prefix($|\s+)/ and die "Syntax error at line $line, duplicate prefix found.\n";

        if ($tokens[$i] =~ /\s*function\s*(.*)/) {
            !$symbols{$tokens[0]}->{'function'} or die "Duplicate function found at line $line.\n";
            !length $1 or die "Unexpected input after keyword `function' found at line $line.\n";
            $symbols{$tokens[0]}->{'function'} = 1;
        } elsif ($tokens[$i] =~ /(array|pointer|ptr)($|\s+)(.*)/) { # pointer will be an alias for array
            $type = ('pointer', 'array')[$1 eq 'array'];
            !$symbols{$tokens[0]}->{'array_type'} or die "Duplicate $type found at line $line.\n";
            length $3 or die "Missing $type type at line $line.\n";
            if ($tokens[$i] =~ /^(array|pointer|ptr)\s+([^[]+)(\[(.*)\])?$/) {
                $2 ne '[]' or die "Empty array dimension at line $line.\n";
                $symbols{$tokens[0]}->{'array_size'} = $4;
                $symbols{$tokens[0]}->{'array_type'} = $2;
            } else {
                die("Syntax error at line $line\n");
            }
        } else {    # assume it's a type
            $symbols{$tokens[0]}->{'type'} = $tokens[$i];
        }
    }
}

$fileName =~ s/[^\\\/]+[\\\/]//g;

#
# Output swos.asm and swos.inc
#

print SWOS_ASM <<EOF;
; Automatically generated from $fileName - do not edit

[list -]

section .swos bss

global swosCodeBase
swosCodeBase:

EOF

print SWOS_INC <<EOF;
; Automatically generated from $fileName - do not edit

; screen width and height
%define WIDTH       320
%define HEIGHT      200

; screen width during the game
%define GAME_WIDTH  384

%define SWOS_MAIN_MENU_SIZE 480

%define TEAM_SIZE 684
%define TACTICS_SIZE 370
%define TEAM_FILE_HEADER_SIZE 76

struc PlayerFile
    .nationality:           resb 1
    .field_1:               resb 1
    .shirtNumber:           resb 1
    .name:                  resb 22
    .field_19:              resb 1
    .positionAndFace:       resb 1
    .cardsInjuries:         resb 1
    .passing:               resb 1
    .shootingHeading:       resb 1
    .tacklingBallControl:   resb 1
    .speedFinishing:        resb 1
    .price:                 resb 1
    .field_21:              resb 1
    .field_22:              resb 1
    .field_23:              resb 1
    .field_24:              resb 1
    .some_flag:             resb 1
endstruc

; Declare and put string into destination.
; %1 - destination, %2 - string
;
%macro movstr 2.nolist
    [section .strdata]
    %%str:
        db %2, 0
    __SECT__
    mov  %1, %%str
%endmacro

; Push offset of a string, or declare it first if needed
; %1 - string (offset or string literal)
;
%macro pushstr 1.nolist
    %ifstr %1
        [section .strdata]
        %%str:
            db %1, 0
        __SECT__
        push %%str
    %else
        push %1
    %endif
%endmacro

; Declare global variable with specified size.
; %1 - variable name, %2 - variable size
;
%macro declareGlobal 2.nolist
    global %1
    [section .bss]
    %1:
        resb %2
    __SECT__
%endmacro

; Declare static variable with specified size.
; %1 - variable name, %2 - variable size
;
%macro declareStatic 2.nolist
    [section .bss]
    %1:
        resb %2
    __SECT__
%endmacro

; Declare pointer to string, and string itself
%macro declareStr 1.nolist
%ifstr %1
    [section .strdata]
    %%str:
        db %1, 0
    __SECT__
    dd %%str
%else
    %error "Expecting string parameter"
%endif
%endmacro

extern swosCodeBase
extern swosDataBase
EOF

# parse the map file now
while (<IN>) {
    if (/^\s+000(1|2):([a-fA-F0-9]{8})\s+([0-9a-zA-Z_]+)\s*$/) {
        # $1 - section, $2 - address, $3 - name of symbol
        next if (!$symbols{$3});        # skip symbol if not in allowed list
        $symbols{$3}->{'present'} = 1;  # mark it as handled
        $numPresent++;
        $name = $symbols{$3}->{'prefix'} . $3;
        $address = hex($2);
        $address += 0xb0000 if $1 eq "2";
        if ($address > 0xb0000 && !$data_start) {
            print SWOS_ASM "\nglobal swosDataBase\n";
            printf SWOS_ASM "times 0xb0000-(\$-\$\$) resb 1\nswosDataBase:\n\n", $$name;
            $data_start = 1;
        }
        printf SWOS_ASM "global %s\ntimes 0x%x-(\$-\$\$) resb 1\n%s:\n\n", $name, $address, $name;
        print SWOS_INC "extern $name\n";
    }
}

print SWOS_INC <<EOF;

; jumps and calls (opcodes 0xe8 and 0xe9) are relative, so we can't use them
; when calling SWOS code from SWOSPP, and vice versa - absolute calls must be made
;
%macro calla 1-2.nolist
%if %0 == 1
    mov  eax, %1
    call eax
%else
    %ifidn %2, push
        push eax
        mov  eax, %1
        call eax
        pop  eax
    %else
        mov  %2, %1
        call %2
    %endif
%endif
%endmacro

%macro jmpa 1.nolist
    push dword %1
    retn
%endmacro

; callCdecl
;
; %1   - function to call
; %... - parameters to function
;
; Macro for invoking cdecl call convention routines.
;
%macro callCdecl 1-*
    %rep %0 - 1
        %rotate -1              ; rotate right in order to push parameters in reverse order (minus function name)
        pushstr %1              ; string parameters will be detected and automatically declared
    %endrep
    %rotate -1                  ; restore parameter order so we can call the function
    call %1
    add  esp, (%0 - 1) * 4      ; cdecl must do stack cleanup
%endmacro

; AsmWriteToLogFunc
;
; %1... - printf-like parameters
;
; Macro for using log file support - use it just like in C
;
%macro AsmWriteToLogFunc 1+
    pushad                      ; we must not interfere with normal code execution
    pushf
    callCdecl WriteToLogFunc, %{1}      ; gotta escape param here... or else
    popf
    popad
%endmacro

%ifdef DEBUG
extern WriteToLogFunc
%macro WriteToLog 1+
    AsmWriteToLogFunc %1
%endmacro
%else
%macro WriteToLog 1+
    ; nothing
%endmacro
%endif

EOF

#
# Experimental: take special care with 68k registers. Declare them as unions for easier access.
# Could've probably been done by adding it to type list, but that would be too much pasting ;).
#
if ($make68kRegsUnions) {
    @regSet68k = ( 'D0', 'D1', 'D2', 'D3', 'D4', 'D5', 'D6', 'D7', 'A0', 'A1', 'A2', 'A3', 'A4', 'A5', 'A6' );
    foreach my $reg (@regSet68k) {
        if (exists($symbols{$reg})) {
            $symbols{$reg}->{'type'} = 'union { char *ptr; dword dw; word w; byte b; }';
        }
    }
}

#
# Output swossym.h
#

print SWOSSYM_H <<EOF;
/*
   swossym.h - SWOS symbols header file

   Automatically generated from $fileName - do not edit
*/

#pragma once
extern "C" {

extern dword swosCodeBase;
EOF

@keys = sort {$symbols{$a}->{'line'} <=> $symbols{$b}->{'line'}} keys %symbols;
foreach (@keys) {
    $data = $symbols{$_};
    next if !$data; # skip asm-only symbols
    print SWOSSYM_H "/*\n   $data->{'comment'}*/\n" if $data->{'comment'};
    $name = $data->{'prefix'} . $_;
    $suffix = " asm (\"$name\")";
    if ($data->{'function'}) {
        print SWOSSYM_H "extern void (*$name\[\])()$suffix;\n";
    } elsif ($data->{'array_type'}) {
        $space = substr($data->{'array_type'}, -1) eq '*' ? '' : ' ';
        print SWOSSYM_H "extern $data->{'array_type'}$space$name\[$data->{'array_size'}\]$suffix;\n";
    } elsif ($data->{'type'}) {
        $space = substr($data->{'type'}, -1) eq '*' ? '' : ' ';
        print SWOSSYM_H "extern $data->{'type'}$space$name$suffix;\n";
    }
}
print SWOSSYM_H "\n}\n";

# complain about symbols present in filter but not in map file
if ($numPresent != keys %symbols) {
    print 'Orphaned symbol' . ($numPresent > 1 ? 's' : '') . " found:\n";
    foreach my $name (sort {$symbols{$a}{'line'} cmp $symbols{$b}{'line'}} keys %symbols) {
        $data = $symbols{$name};
        if (!$data->{'present'}) {
            print "$name, at line $data->{'line'}\n";
        }
    }
}

close IN;
close SWOS_ASM;
close SWOS_INC;
close SWOSSYM_H;

# halt the build process if something ain't right
$numPresent == keys %symbols or exit(1);
