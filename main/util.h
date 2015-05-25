#pragma once

#include "dos.h"

extern "C" {

/* assert support */
#ifdef DEBUG
void AssertFailed(const char *, int) __attribute__((noreturn));
/* it's actually better if we leave out the noreturn attribute, since call stack will be available then
   (with aborts compiler generates direct jmp and leaves us without access to caller context) */
// turning it on again for tests with GCC
void AssertFailedMsg(const char *, int, const char *) __attribute__((noreturn));

#define assert(x) ((void)((x) ? (void)0 : AssertFailed(__FILE__, __LINE__)))
#define assert_msg(x, msg) ((void)((x) ? (void)0 : AssertFailedMsg(__FILE__, __LINE__, msg)))
#else
#define assert(x)           ((void)0)
#define assert_msg(x, msg)  ((void)0)
#endif

#undef sizeofarray
#define sizeofarray(x) (sizeof(x) / sizeof((x)[0]))
#ifndef offsetof
#define offsetof(st, m) ((unsigned int)(&((st *)0)->m))
#endif

#undef STRINGIFY
#undef QUOTE
#define STRINGIFY(s) QUOTE(s)
#define QUOTE(s) #s

#define abs(a) ((a) < 0 ? -(a) : (a))
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef EZERO
#define EZERO 0
#endif

enum CharTypeFlags {
    C_DIGIT    = 1,
    C_SPACE    = 2,
    C_PUNCT    = 4,
    C_XDIGIT   = 8,
    C_UPPER    = 16,
    C_LOWER    = 32,
    C_CONTROL  = 64,
    C_PRINT    = 128
};

/* from ctype.h */
extern const unsigned char IsWhat[256];

#define isalnum(c)  ((IsWhat[(c) & 0xff] & (C_LOWER | C_UPPER | C_DIGIT)) != 0)
#define isalpha(c)  ((IsWhat[(c) & 0xff] & (C_LOWER | C_UPPER)) != 0)
#define iscntrl(c)  ((IsWhat[(c) & 0xff] & C_CONTROL) != 0)
#define iscsym(c)   ((IsWhat[(c) & 0xff] & (C_DIGIT | C_UPPER | C_LOWER)) || (c) == '_')
#define iscsymf(c)  ((IsWhat[(c) & 0xff] & (C_UPPER | C_LOWER)) || (c) == '_')
#define isdigit(c)  ((IsWhat[(c) & 0xff] & C_DIGIT) != 0)
#define isgraph(c)  ((IsWhat[(c) & 0xff] & (C_PRINT | C_SPACE)) != 0)
#define islower(c)  ((IsWhat[(c) & 0xff] & C_LOWER) != 0)
#define isprint(c)  ((IsWhat[(c) & 0xff] & C_PRINT) != 0)
#define ispunct(c)  ((IsWhat[(c) & 0xff] & C_PUNCT) != 0)
#define isspace(c)  ((IsWhat[(c) & 0xff] & C_SPACE) != 0)
#define isupper(c)  ((IsWhat[(c) & 0xff] & C_UPPER) != 0)
#define isxdigit(c) ((IsWhat[(c) & 0xff] & C_XDIGIT) != 0)

#define tolower(a) (isupper(a) ? (a) - 'A' + 'a' : (a))
#define toupper(a) (islower(a) ? (a) - 'a' + 'A' : (a))

#define skipWhitespace(p) {while(isspace(*p)) p++;}

#define swap(a, b) { dword tmp = (dword)a; *(dword *)&a = *(dword *)&b; *(dword *)&b = tmp; }

#ifndef LONG_MAX
#define LONG_MAX    2147483647L         /* maximum value of a long int       */
#endif
#ifndef LONG_MIN
#define LONG_MIN    (-2147483647L-1)    /* minimum value of a long int       */
#endif
#ifndef ULONG_MAX
#define ULONG_MAX   4294967295UL        /* maximum value of an unsigned long */
#endif

/* functions included in SWOS - have to be called through a pointer */

void exit(int status) __attribute__((noreturn));
int stackavail();

const char *strrchr(const char *str, int character);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t n);
int strlen(const char *str);
char *strncpy(char *dst, const char *src, size_t n);
void *memset(void *ptr, int value, size_t num);
void *memmove(void *dst, const void *src, size_t n);
void segread(struct SREGS *sregs);
int int386x(int vec, union REGS *in, union REGS *out, struct SREGS *sregs);

/* recreate small part of time.h, just enough so we can get timestamps nicely printed */
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef unsigned long time_t;   /* unfortunately, GCC seems to define it if any system header is included */
#endif

static inline __attribute__((always_inline)) time_t time(time_t *seconds)
{
    time_t result;
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_time_    \t\n"
        "call %[tmp]                            \t\n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "a" (seconds)
        : "cc", "memory"
    );
    return result;
}

static inline __attribute__((always_inline)) char *ctime (const time_t *timer)
{
    char *result;
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_ctime_   \t\n"
        "call %[tmp]                            \t\n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "a" (timer)
        : "cc", "memory"
    );
    return result;
}

/* these are missing from SWOS, we'll have to implement them */
char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
int stricmp(const char *s1, const char *s2);
#ifdef __GNUC__
/* use GCC kickass built in memcpy instead of our crappy one */
#define memcpy(dst, src, n) __builtin_memcpy(dst, src, n)
#else
void *memcpy(void *dst, const void *src, size_t n);
#endif
int memcmp(const void *p, const void *q, size_t n);
char *int2str(int num);
char *strupr(char *s);
void srand(unsigned int seed);
unsigned int rand();
unsigned int simpleHash(const void *data, int size);
long int strtol(const char *ptr, const char **endptr, int base, int *result);

#ifndef RAND_MAX
#define RAND_MAX 0xffffffff
#endif

/* buffered file routines */
typedef struct BFile {
    char *buffer;
    int bufferSize;
    int bytesInBuffer;
    int readPtr;
    dword handle;
    bool managed;
} BFile;

#define FILE_BUFFER_SIZE    (4 * 1024)

BFile *OpenBFile(DOS_accessMode accessMode, const char *fileName);
bool OpenBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_accessMode accessMode, const char *fileName);
BFile *CreateBFile(DOS_fileAttributes fileAttribute, const char *fileName);
bool CreateBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_fileAttributes fileAttribute, const char *fileName);
int FlushBFile(BFile *file);
int WriteBFile(BFile *file, const void *pData, int size);
bool PutCharBFile(BFile *file, char c);
int ReadBFile(BFile *file, void *pData, int size);
int GetCharBFile(BFile *file);
int PeekCharBFile(BFile *file);
bool UngetCharBFile(BFile *file, int c);
int SeekBFile(BFile *file, uchar mode, int ofsHi, int ofsLo);
void CloseBFile(BFile *file);
void CloseBFileUnmanaged(BFile *file);

}
