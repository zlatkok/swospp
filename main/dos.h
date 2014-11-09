/**
    dos.h

    DOS interface constants and functions.

*/

#pragma once

/* Scan codes from keyboard controller. */
enum DOS_scanCodes {
    KEY_F4          = 0x3e,
    KEY_F7          = 0x41,
    KEY_F8          = 0x42,
    KEY_F9          = 0x43,
    KEY_F10         = 0x44,
    KEY_PAGE_UP     = 0x49,
    KEY_PAGE_DOWN   = 0x51
};

/* File open access mode. */
enum DOS_accessMode {
    F_READ_ONLY,
    F_WRITE_ONLY,
    F_READ_WRITE,
};

#define SEEK_START      0
#define SEEK_CUR        1
#define SEEK_END        2

/* File attributes when creating a file. */
enum DOS_fileAttributes {
    ATTR_NONE           = 0,
    ATTR_READ_ONLY      = 1,
    ATTR_HIDDEN         = 2,
    ATTR_SYSTEM         = 4,
    ATTR_VOLUME_LABEL   = 8,
    ATTR_SUBDIRECTORY   = 16,
    ATTR_ARCHIVE        = 32,
};

#define INVALID_HANDLE  ((dword)-1)

dword OpenFile(enum DOS_accessMode accessMode, const char *fileName);
dword CreateFile(enum DOS_fileAttributes fileAttribute, const char *fileName);
int WriteFile(dword handle, const void *pData, int size);
int ReadFile(dword handle, void *pData, int size);
int SeekFile(dword handle, uchar mode, int ofsHi, int ofsLo);
void CloseFile(dword handle);

#pragma pack(push, 1)
typedef struct DTA {
    byte  searchAttribute;
    byte  searchDrive;
    byte  searchName[11];
    word  dirEntryNum;
    word  startingCluster;
    word  reserved;
    word  startingCluster2;
    byte  attribute;
    word  fileTime;
    word  fileDate;
    dword fileSize;
    char  fileName[13];
} DTA;
#pragma pack(pop)

void SetDTA(DTA *newDTA);
int FindFirstFile(const char *fileSpec);
int FindNextFile();

#pragma aux OpenFile =  \
        "mov  ah, 3dh"  \
        "int  21h"      \
        "sbb  ebx, ebx" \
        "or   eax, ebx" \
        parm [al] [edx] \
        modify [ebx];

#pragma aux CreateFile = \
        "mov  ah, 3ch"   \
        "int  21h"       \
        "sbb  ebx, ebx"  \
        "or   eax, ebx"  \
        parm [cx] [edx]  \
        modify [ebx];

#pragma aux WriteFile = \
        "mov  ah, 40h"  \
        "int  21h"      \
        parm [ebx] [edx] [ecx];

#pragma aux CloseFile = \
        "mov  ah, 3eh"  \
        "int  21h"      \
        parm [ebx];

#pragma aux ReadFile =  \
        "mov  ah, 3fh"  \
        "int  21h"      \
        "sbb  ecx, ecx" \
        "or   eax, ecx" \
        parm [ebx] [edx] [ecx] \
        modify [ecx];

#pragma aux SeekFile =  \
        "mov  ah, 42h"  \
        "int  21h"      \
        "sbb  ecx, ecx" \
        "or   eax, ecx" \
        parm [ebx] [al] [ecx] [edx] \
        modify [ecx];

#pragma aux SetDTA =   \
        "mov  ah, 1ah" \
        "int  21h"     \
        parm [edx];

#pragma aux FindFirstFile = \
        "mov  ah, 4eh"  \
        "int  21h"      \
        "sbb  eax, eax" \
        "not  eax"      \
        parm [edx];

#pragma aux FindNextFile = \
        "mov  ah, 4fh"  \
        "int  21h"      \
        "sbb  eax, eax" \
        "not  eax";

void PrintToStderr(const char *str, int count);
void SetVideoMode(int mode);
char GetVideoMode();

#pragma aux PrintToStderr = \
        "push 2"        \
        "pop  ebx"      \
        "mov  ah, 0x40" \
        "int  0x21"     \
        parm [edx] [ecx];

#pragma aux SetVideoMode = \
        "xor  ah, ah"   \
        "int  0x10"     \
        parm [eax];

#pragma aux GetVideoMode = \
        "mov  ah, 0x0f" \
        "int  0x10"     \
        value [al];