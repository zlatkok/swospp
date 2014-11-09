#pragma once

/* assert support */
#ifdef DEBUG
void AssertFailed(const char *, int);
void AssertFailedMsg(const char *, int, const char *);
/* it's actually better if we leave out the aborts attribute, since call stack will be available then
   (with aborts compiler generates direct jmp and leaves us without access to caller context) */
#pragma aux AssertFailed /*aborts*/;
#pragma aux AssertFailedMsg /*aborts*/;
#define assert(x) ((void)((x) ? (void)0 : AssertFailed(__FILE__, __LINE__)))
#define assert_msg(x, msg) ((void)((x) ? (void)0 : AssertFailedMsg(__FILE__, __LINE__, msg)))
#else
#define assert(x)           ((void)0)
#define assert_msg(x, msg)  ((void)0)
#endif

#undef sizeofarray
#undef member_size
#undef offsetof
#define sizeofarray(x) (sizeof(x) / sizeof((x)[0]))
#define member_size(type, member) (sizeof(((type *)0)->member))
#define offsetof(st, m) ((unsigned int)(&((st *)0)->m))

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

#define static_assert(cond) typedef char static_assertion_##__FILE__##__LINE__[(cond) ? 1 : -1]

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

void exit(int status);
#pragma aux exit aborts;

typedef unsigned long time_t;
time_t time(time_t *timer);
char *ctime(const time_t *timer);

const char *strrchr(const char *str, int character);
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t n);
int strlen(const char *str);
typedef int (*strlen_f)(char *str);
char *strncpy(char *dst, const char *src, size_t n);
typedef char *(*strncpy_f)(char *dst, const char *src, size_t n);
void *memset(void *ptr, int value, size_t num);
void *memmove(void *dst, const void *src, size_t n);
int swos_libc_stackavail();
int stackavail();
#pragma aux stackavail =                    \
    "mov  eax, offset swos_libc_stackavail" \
    "call eax"                              \
    value [eax];

/* these are missing from SWOS, we'll have to implement them */
char *strcpy(char *dst, const char *src);
char *strcat(char *dst, const char *src);
int stricmp(const char *s1, const char *s2);
void *memcpy(void *dst, const void *src, size_t n);
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

BFile *OpenBFile(byte accessMode, const char *fileName);
bool OpenBFileUnmanaged(BFile *file, void *buffer, int bufferSize, byte accessMode, const char *fileName);
BFile *CreateBFile(word fileAttribute, const char *fileName);
bool CreateBFileUnmanaged(BFile *file, void *buffer, int bufferSize, word fileAttribute, const char *fileName);
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