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

#pragma pack(push, 1)
struct DTA {
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
};
#pragma pack(pop)


static inline __attribute__((always_inline)) dword OpenFile(enum DOS_accessMode accessMode, const char *fileName)
{
    dword result;
    int dummy;
    asm volatile (
        "mov  ah, 0x3d          \n"
        "int  0x21              \n"
        "sbb  %[tmp], %[tmp]    \n"
        "or   eax, %[tmp]       \n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "a" (accessMode), "d" (fileName)
        : "cc"
    );
    return result;
}

static inline __attribute__((always_inline)) dword CreateFile(enum DOS_fileAttributes fileAttribute, const char *fileName)
{
    dword result;
    int dummy;
    asm volatile (
        "mov  ah, 0x3c          \n"
        "int  0x21              \n"
        "sbb  %[tmp], %[tmp]    \n"
        "or   eax, %[tmp]       \n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "c" (fileAttribute), "d" (fileName)
        : "cc"
    );
    return result;
}

static inline __attribute__((always_inline)) bool CreateDirectory(const char *path)
{
    bool result;
    asm volatile (
        "mov  ah, 0x39          \n"
        "int  0x21              \n"
        "jnc  set_result        \n"
        // check if it already exists
        "cmp  ax, 5             \n"
        "jnz  set_result        \n"
        "xor  eax, eax          \n"
"set_result:                    \n"
        "sbb  eax, eax          \n"
        "not  eax               \n"
        : "=a" (result)
        : "d" (path)
        : "cc"
    );
    return result;
}

static inline __attribute__((always_inline)) int WriteFile(dword handle, const void *pData, int size)
{
    int result;
    asm volatile (
        "mov  ah, 0x40  \n"
        "int  0x21      \n"
        : "=a" (result)
        : "b" (handle), "d" (pData), "c" (size)
        : "cc"
    );
    return result;
}

static inline __attribute__((always_inline)) void CloseFile(dword handle)
{
    asm volatile (
        "mov  ah, 0x3e  \n"
        "int  0x21      \n"
        :
        : "b" (handle)
        : "cc", "eax"
    );
}

static inline __attribute__((always_inline)) int ReadFile(dword handle, void *pData, int size)
{
    int result;
    int dummy;
    asm volatile (
        "mov  ah, 0x3f          \n"
        "int  0x21              \n"
        "sbb  %[tmp], %[tmp]    \n"
        "or   eax, %[tmp]       \n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "b" (handle), "d" (pData), "c" (size)
        : "cc", "memory"
    );
    return result;
}

static inline __attribute__((always_inline)) int SeekFile(dword handle, uchar mode, int ofsHi, int ofsLo)
{
    int result;
    int dummy;
    asm volatile (
        "mov  ah, 0x42          \n"
        "int  0x21              \n"
        "sbb  %[tmp], %[tmp]    \n"
        "or   eax, %[tmp]       \n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "b" (handle), "a" (mode), "c" (ofsHi), "d" (ofsLo)
        : "cc"
    );
    return result;
}

static inline __attribute__((always_inline)) int GetFilePos(dword handle)
{
    int result;
    asm volatile (
        "mov  ax, 0x4201        \n"
        "xor  ecx, ecx          \n"
        "xor  edx, edx          \n"
        "int  0x21              \n"
        "sbb  ecx, ecx          \n"
        "shl  edx, 16           \n"
        "or   eax, edx          \n"
        "or   eax, ecx          \n"
        : "=a" (result)
        : "b" (handle)
        : "cc", "ecx", "edx"
    );
    return result;
}

static inline __attribute__((always_inline)) void SetDTA(DTA *newDTA)
{
    asm volatile (
        "mov  ah, 0x1a  \n"
        "int  0x21      \n"
        :
        : "d" (newDTA)
        : "eax"
    );
}

static __attribute__((always_inline)) inline int FindFirstFile(const char *fileSpec)
{
    int result;
    asm volatile (
        "mov  ah, 0x4e  \n"
        "int  0x21      \n"
        "sbb  eax, eax  \n"
        "not  eax       \n"
        : "=a" (result)
        : "d" (fileSpec)
        : "cc", "memory"
    );
    return result;
}

static inline __attribute__((always_inline)) bool32 FindNextFile()
{
    int result;
    asm volatile (
        "mov  ah, 0x4f  \n"
        "int  0x21      \n"
        "sbb  eax, eax  \n"
        "not  eax       \n"
        : "=a" (result)
        :
        : "cc", "memory"
    );
    return result;
}

static inline __attribute__((always_inline)) void PrintToStderr(const char *str, int count)
{
    asm volatile (
        "push 2         \n"
        "mov  ah, 0x40  \n"
        "pop  ebx       \n"
        "int  0x21      \n"
        :
        : "d" (str), "c" (count)
        : "cc", "eax", "ebx"
    );
}

static __attribute__((always_inline)) inline void SetVideoMode(int mode)
{
    int dummy;
    asm volatile (
        "mov  ah, 0     \n"     // do not disturb flags
        "int  0x10      \n"
        : "=a" (dummy)
        : "a" (mode)
        :
    );
}

static __attribute__((always_inline)) inline dword GetVideoMode()
{
    dword result;
    asm volatile (
        "mov  eax, 0x0f00       \n"
        "int  0x10              \n"
        "movzx eax, al          \n"
        : "=a" (result)
        :
        : "eax", "ebx"
    );
    return result;
}

static inline __attribute__((always_inline)) void GetDosTime(uchar *aHour, uchar *aMinute, uchar *aSecond, uchar *aHundred)
{
    uchar hour, minute, second, hundred;
    asm volatile (
        "mov  ah, 0x2c                      \n"
        "int  0x21                          \n"
        "mov  al, ch                        \n"
        "mov  %b[second], dh                \n"
        : "=a" (hour), "=c" (minute), [second] "=q" (second), "=d" (hundred)
        :
        :
    );
    *aHour = hour % 24;     /* DOSBox seems to return values >24 when running for long time */
    *aMinute = minute;
    *aSecond = second;
    *aHundred = hundred;
}

static inline __attribute__((always_inline)) void GetDosDate(ushort *aYear, uchar *aMonth, uchar *aDay)
{
    ushort year;
    uchar month, day;
    asm volatile (
        "mov  ah, 0x2a                  \n"
        "int  0x21                      \n"     // DOS Get System Date
        "mov  al, dh                    \n"
        : "=c" (year), "=d" (day), "=a" (month)
        :
        :
    );
    *aYear = year;
    *aMonth = month;
    *aDay = day;
}

static inline __attribute__((always_inline)) unsigned short GetTickCount()
{
    return *(unsigned short *)0x46c;
}

/* Copy part of Watcom's i86.h, luck is that relevant functions have been left in SWOS ;) */

#pragma pack(push, 1)

#undef __FILLER
#define __FILLER(a) unsigned short a;

/* dword registers */
struct DWORDREGS {
    unsigned int eax;
    unsigned int ebx;
    unsigned int ecx;
    unsigned int edx;
    unsigned int esi;
    unsigned int edi;
    unsigned int cflag;
};


/* word registers */
struct WORDREGS {
    unsigned short ax;  __FILLER(_1)
    unsigned short bx;  __FILLER(_2)
    unsigned short cx;  __FILLER(_3)
    unsigned short dx;  __FILLER(_4)
    unsigned short si;  __FILLER(_5)
    unsigned short di;  __FILLER(_6)
    unsigned int cflag;
};

/* byte registers */
struct BYTEREGS {
    unsigned char al, ah;  __FILLER(_1)
    unsigned char bl, bh;  __FILLER(_2)
    unsigned char cl, ch;  __FILLER(_3)
    unsigned char dl, dh;  __FILLER(_4)
};

/* general purpose registers union - overlays the corresponding dword,
 * word, and byte registers.
 */

union REGS {
    struct DWORDREGS x;
    struct WORDREGS  w;
    struct BYTEREGS  h;
};
#define _REGS REGS

/* segment registers */
struct SREGS {
    unsigned short es, cs, ss, ds;
    unsigned short fs, gs;
};
#define _SREGS SREGS

#define  FP_OFF(__p) ((unsigned)(__p))
#define _FP_OFF(__p) ((unsigned)(__p))

static inline __attribute__((always_inline)) unsigned short FP_SEG(const volatile void *)
{
    unsigned short seg;
    asm (
        "mov  %w[seg], ds"
        : [seg] "=q" (seg)
        :
        :
    );
    return seg;
}
#define _FP_SEG FP_SEG

#pragma pack(pop)
