/** util.c

    Utility functions and implementations of C standard library functions missing from SWOS.
*/

#include <errno.h>
#include "qalloc.h"

#pragma GCC diagnostic ignored "-Wparentheses"

static unsigned rand_seed;

#define IS_DIGIT(c)   (c >= '0' && c <= '9')
#define IS_SPACE(c)   (c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r' || c == ' ')
#define IS_PUNCT(c)   ((c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~'))
#define IS_XDIGIT(c)  ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
#define IS_UPPER(c)   (c >= 'A' && c <= 'Z')
#define IS_LOWER(c)   (c >= 'a' && c <= 'z')
#define IS_CONTROL(c) ((c >= 0 && c <= 31) || c == 127)
#define IS_PRINT(c)   (c >= ' ' && c <= '~')

#define A(c)    B(c), B(c + 64), B(c + 128), B(c + 192)
#define B(c)    C(c), C(c + 16), C(c + 32), C(c + 48)
#define C(c)    D(c), D(c + 4), D(c + 8), D(c + 12)
#define D(c)    T(c), T(c + 1), T(c + 2), T(c + 3)
#define T(c)    ((-IS_DIGIT((c)) & C_DIGIT) | (-IS_SPACE((c)) & C_SPACE) | (-IS_PUNCT((c)) & C_PUNCT) | \
    (-IS_XDIGIT((c)) & C_XDIGIT) | (-IS_UPPER((c)) & C_UPPER) | (-IS_LOWER((c)) & C_LOWER) | \
    (-IS_CONTROL((c)) & C_CONTROL) | (-IS_PRINT((c)) & C_PRINT))

/* Common table for determining character classes. */
const byte IsWhat[256] = {
    A(0)
};

/* stack probe allocator... we don't need it of course, but no idea how to stop GCC from generating calls to it */
extern "C" void ___chkstk_ms(size_t) {}


/** int2str

    in:
         num - number to convert (signed 32-bit int)
    out:
         pointer to buffer containing string representation of a number;
         it's a static buffer so make a copy if you need to persist it

    Converts given number to string and returns pointer to buffer containing
    its string representation.
*/
char *int2str(int num)
{
    int oldNum = num;
    static char buf[12];    /* 10 digits is max 32-bit number + sign + zero terminator */
    char *p = buf + sizeof(buf) - 2;
    assert(sizeof(int) <= 4);

    /* store digits from the end of the buffer, so we don't need to invert it later */
    p[1] = '\0';
    if (num < 0) {
        do {
            num /= 10;
            *p-- = '0' + num * 10 - oldNum;    /* avoid having to do modulo */
            oldNum = num;
        } while (num);
        *p = '-';
    } else {
        do {
            num /= 10;
            *p-- = '0' + oldNum - num * 10;    /* avoid having to do modulo */
            oldNum = num;
        } while (num);
        p++;
    }

    assert(p >= buf && p < buf + sizeof(buf) - 1);
    return p;
}


/** strrchr

    in:
          src -> string to search in
          ch  -  character to look for

    out:
          pointer to last occurence of ch in src, or nullptr if not present
*/
const char *strrchr(const char *str, int ch)
{
    assert(str);
    const char *end = str + strlen(str);

    while (end != str && *end != ch)
        end--;

    if (*end != ch)
        return nullptr;

    return end;
}


/** strcpy

    in:
         src -> string to copy
         dst -> destination string
    out:
         pointer to terminating zero in dst
*/
char *strcpy(char *dst, const char *src)
{
    assert(src && dst);
    while (*dst++ = *src++);
    return dst - 1;
}


/** stricmp

    in:
         s1 -> first string
         s2 -> second string

    Lexicographically compares strings case insensitive. Returns an integer less
    than, equal, or greater than zero, indicating that first string is less
    than, equal, or greater than second string.
*/
int stricmp(const char *s1, const char *s2)
{
    char ch1, ch2;

    assert(s1 && s2);
    do {
        ch1 = toupper(*s1);
        ch2 = toupper(*s2);
        s1++;
        s2++;
    } while (ch1 && ch1 == ch2);

    return ch1 - ch2;
}


/*
    Appends a copy of the source string to the destination string.
*/
char *strcat(char *dst, const char *src)
{
    assert(src && dst);

    while (*dst)
        dst++;

    while (*dst++ = *src++);

    return dst;
}


void srand(unsigned int seed)
{
    rand_seed = (seed ^ 0x08070605) - 3479;
}


unsigned int rand()
{
    unsigned int rnd = rand_seed * 347;
    rnd ^= 0x5555aaaa;
    return rand_seed = rnd - 78659;
}


char *strupr(char *s)
{
    assert(s);
    for (char *p = s; *p = toupper(*p); p++);
    return s;
}


/* Even if compiling with GCC built-in version we still need it for calls from ASM code. */
void *memcpy(void *in_dst, const void *in_src, size_t n)
{
    char *dst = (char *)in_dst;
    const char *src = (const char *)in_src;
    assert(!n || (in_src && in_dst && (int)n > 0));

    for (; n; --n)
        *dst++ = *src++;

    return in_dst;
}


int memcmp(const void *p, const void *q, size_t n)
{
    assert(!n || (p && q && (int)n > 0));
    auto s1 = (const uchar *)p;
    auto s2 = (const uchar *)q;

    for (; n; n--, s1++, s2++) {
        if (*s1 != *s2)
            return *s1 - *s2;
    }

    return 0;   /* both operands are equal */
}


/* stubs for connecting to library routines in SWOS */
/* keep them non-inline for access from asm */

int stackavail()
{
    int result;
    asm volatile (
        "mov  eax, offset swos_libc_stackavail_ \n"
        "call eax                               \n"
        : "=a" (result)
        :
        :
    );

    return result;
}


char *strncpy(char *dst, const char *src, size_t n)
{
    assert(!n || (dst && src && (int)n > 0));
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_strncpy_     \n"
        "call %[tmp]                                \n"
        : "+a" (dst), "+d" (src), "+b" (n), [tmp] "=r" (dummy)
        :
        : "cc", "memory"
    );

    return dst;
}


void *memset(void *ptr, int value, size_t num)
{
    assert(!num || ptr);
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_memset_  \n"
        "call %[tmp]                            \n"
        : "+a" (ptr), "+d" (value), [tmp] "=r" (dummy)
        : "b" (num)
        : "cc", "memory"
    );

    return ptr;
}


int strcmp(const char *str1, const char *str2)
{
    assert(str1 && str2);
    int result;
    int dummy, dummy2;
    asm volatile (
        "mov  %[tmp], offset swos_libc_strcmp_  \n"
        "call %[tmp]                            \n"
        : "=a" (result), [tmp] "=r" (dummy), "=d" (dummy2)
        : "a" (str1), "d" (str2)
        : "cc"
    );

    return result;
}


int strncmp(const char *str1, const char *str2, size_t n)
{
    assert(str1 && str2 && (int)n >= 0);
    int result;
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_strncmp_     \n"
        "call %[tmp]                                \n"
        : "=a" (result), "+d" (str2), "+b" (n), [tmp] "=r" (dummy)
        : "a" (str1)
        : "cc"
    );

    return result;
}


int strlen(const char *str)
{
    assert(str);
    int result;
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_strlen_      \n"
        "call %[tmp]                                \n"
        : "=a" (result), [tmp] "=r" (dummy)
        : "a" (str)
        : "cc"
    );

    return result;
}


void segread(struct SREGS *sregs)
{
    assert(sregs);
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_segread_     \n"
        "call %[tmp]                                \n"
        : "+a" (sregs), [tmp] "=r" (dummy)
        :
        : "memory"
    );
}


int int386x(int vec, union REGS *in, union REGS *out, struct SREGS *sregs)
{
    assert(in && out && sregs);
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_int386x_     \n"
        "call %[tmp]                                \n"
        : "+a" (vec), "+d" (in), "+b" (out), "+c" (sregs), [tmp] "=r" (dummy)
        :
        : "cc", "memory"
    );

    return vec;
}


void exit(int status)
{
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_exit_    \n"
        "jmp  %[tmp]                            \n"
        : "+a" (status), [tmp] "=r" (dummy)
        :
        :
    );

    __builtin_unreachable();
}

void *memmove(void *dst, const void *src, size_t n)
{
    assert(!n || (dst && src));
    int dummy;
    asm volatile (
        "mov  %[tmp], offset swos_libc_memmove_     \n"
        "call %[tmp]                                \n"
        : "+a" (dst), "+d" (src), "+b" (n), [tmp] "=r" (dummy)
        :
        : "cc", "memory"
    );

    return dst;
}

/* djb2 */
unsigned int simpleHash(const void *data, int size)
{
    unsigned int hash = 5381;
    const char *p = (const char *)data, *end = p + size;
    assert(!size || (size > 0 && data));

    while (p != end)
        hash = (hash << 5) + hash + *p++;

    return hash;
}

/* The following was shamelessly ripped from Open Watcom :P */

/*
 * this table is the largest value that can safely be multiplied
 * by the associated base without overflowing
 */
static unsigned long nearlyOverflowing[] = {
    ULONG_MAX / 2,  ULONG_MAX / 3,  ULONG_MAX / 4,  ULONG_MAX / 5,
    ULONG_MAX / 6,  ULONG_MAX / 7,  ULONG_MAX / 8,  ULONG_MAX / 9,
    ULONG_MAX / 10, ULONG_MAX / 11, ULONG_MAX / 12, ULONG_MAX / 13,
    ULONG_MAX / 14, ULONG_MAX / 15, ULONG_MAX / 16, ULONG_MAX / 17,
    ULONG_MAX / 18, ULONG_MAX / 19, ULONG_MAX / 20, ULONG_MAX / 21,
    ULONG_MAX / 22, ULONG_MAX / 23, ULONG_MAX / 24, ULONG_MAX / 25,
    ULONG_MAX / 26, ULONG_MAX / 27, ULONG_MAX / 28, ULONG_MAX / 29,
    ULONG_MAX / 30, ULONG_MAX / 31, ULONG_MAX / 32, ULONG_MAX / 33,
    ULONG_MAX / 34, ULONG_MAX / 35, ULONG_MAX / 36
};

#define hexstr(p) (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))

static int radixValue(char c)
{
    if (c >= '0'  &&  c <= '9')
        return c - '0';

    c = tolower(c);

    if (c >= 'a'  &&  c <= 'i')
        return c - 'a' + 10;
    if (c >= 'j'  &&  c <= 'r')
        return c - 'j' + 19;
    if (c >= 's'  &&  c <= 'z')
        return c - 's' + 28;

    return 37;
}


/** strol

    The strtol function converts the string pointed to by ptr to an object
    of type long int. The function recognizes a string containing optional white
    space, an optional sign (+ or -), followed by a sequence of digits and letters.
    The conversion ends at the first unrecognized character.  A pointer to that
    character will be stored in the object endptr points to if endptr is not NULL.
    ` character is ignored and can be used for digit grouping, such as:
    10`000`000`000

    ptr    -> points to (hopefully) string representation of long int to convert
    endptr -> will receive value of a pointer to first unrecognized char if non null
    base   -  conversion base, if 0 it will be determined by first characters
    result -  pointer to integer that will contain result of the operation at the end, if non null
              codes from errno.h are used
*/
long int strtol(const char *ptr, const char **endptr, int base, int *result)
{
    const char *p;
    const char *startp;
    int digit;
    unsigned long int value;
    unsigned long int prevValue;
    char sign;
    char overflow;

    if (result)
        *result = EZERO;

    if (endptr)
        *endptr = (char *)ptr;

    p = ptr;
    skipWhitespace(p);
    sign = *p;

    if (sign == '+' || sign == '-')
        p++;

    if (!base) {
        if (hexstr(p))
            base = 16;
        else if (*p == '0')
            base = 8;
        else
            base = 10;
    }

    if (base < 2  ||  base > 36) {
        if (result)
            *result = EDOM;
        return 0;
    }

    if (base == 16) {
        if (hexstr(p))
            p += 2;    /* skip over '0x' */
    }

    startp = p;
    overflow = 0;
    value = 0;

    for (;;) {
        if (*p == '`') {
            p++;
            continue;
        }

        digit = radixValue(*p);
        if (digit >= base)
            break;

        if (value > nearlyOverflowing[base - 2])
            overflow = 1;

        prevValue = value;
        value = value * base + digit;

        if (value < prevValue)
            overflow = 1;

        p++;
    }
    if (p == startp)
        p = ptr;

    if (endptr != nullptr)
        *endptr = (char *)p;

    if (value >= 0x80000000 && (value != 0x80000000 || sign != '-'))
        overflow = 1;

    if (overflow) {
        if (result)
            *result = ERANGE;

        return sign == '-' ? LONG_MIN : LONG_MAX;
    }

    if (sign == '-')
        value = -value;

    return value;
}


/**
    Buffered File support routines, built on top of DOS direct file access functions.
**/

static BFile *initBFile(BFile *file, void *buffer, int bufferSize, bool readOnly, bool managed)
{
    assert(file);
    file->bytesInBuffer = 0;
    file->readPtr = readOnly ? 0 : -1;
    file->buffer = (char *)buffer;
    file->bufferSize = bufferSize;
    file->managed = managed;

    if (file->handle == INVALID_HANDLE) {
        if (managed)
            qFree(file);
        return nullptr;
    }

    if (!managed)
        file->bufferSize = bufferSize;
    else if (!(file->buffer = (char *)qAlloc(FILE_BUFFER_SIZE))) {
        CloseFile(file->handle);
        qFree(file);
        return nullptr;
    }

    return file;
}


BFile *OpenBFile(DOS_accessMode accessMode, const char *fileName)
{
    assert_msg(accessMode != F_READ_WRITE, "BFile can't be open for both read and write.");
    auto file = (BFile *)qAlloc(sizeof(BFile));

    if (!file)
        return nullptr;

    assert(fileName);
    file->handle = (dword)OpenFile(accessMode, fileName);
    return initBFile(file, nullptr, FILE_BUFFER_SIZE, accessMode == F_READ_ONLY, true);
}


bool OpenBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_accessMode accessMode, const char *fileName)
{
    assert(file);
    assert_msg(accessMode != F_READ_WRITE, "BFile can't be open for both read and write.");
    file->handle = OpenFile(accessMode, fileName);
    return initBFile(file, buffer, bufferSize, accessMode == F_READ_ONLY, false) != nullptr;
}


BFile *CreateBFile(DOS_fileAttributes fileAttribute, const char *fileName)
{
    auto file = (BFile *)qAlloc(sizeof(BFile));

    if (!file)
        return nullptr;

    assert(fileName);
    file->handle = CreateFile(fileAttribute, fileName);
    return initBFile(file, nullptr, FILE_BUFFER_SIZE, false, true);
}


bool CreateBFileUnmanaged(BFile *file, void *buffer, int bufferSize, DOS_fileAttributes fileAttribute, const char *fileName)
{
    assert(file);
    file->handle = CreateFile(fileAttribute, fileName);
    return initBFile(file, buffer, bufferSize, false, false) != nullptr;
}


int FlushBFile(BFile *file)
{
    assert(file);
    /* a NOP for read-only files */
    if (file->readPtr >= 0)
        return 0;

    assert(file->bytesInBuffer >= 0 && file->bytesInBuffer <= file->bufferSize);
    if (file->bytesInBuffer) {
        int bytesWritten = WriteFile(file->handle, file->buffer, file->bytesInBuffer);
        if (bytesWritten < 0)
            return bytesWritten;

        assert(bytesWritten <= file->bytesInBuffer);

        /* partial write */
        if (bytesWritten != file->bytesInBuffer)
            memmove(file->buffer, file->buffer + bytesWritten, file->bytesInBuffer - bytesWritten);

        file->bytesInBuffer -= bytesWritten;
        return bytesWritten;
    }

    return 0;
}


int WriteBFile(BFile *file, const void *pData, int size)
{
    assert(size >= 0 && pData && file);
    assert(file->readPtr < 0);

    if (!size)
        return 0;

    if (size > file->bufferSize) {
        /* for big writes just do it directly, but first flush whatever we got in the buffer */
        int bytesInBuffer = file->bytesInBuffer;
        int bytesWritten = FlushBFile(file);

        if (bytesWritten < 0)
            return bytesWritten;

        if (bytesWritten != bytesInBuffer)
            return 0;

        return WriteFile(file->handle, pData, size);
    } else {
        int remainingBufSize = file->bufferSize - file->bytesInBuffer;
        if (size <= remainingBufSize) {
            /* entirely buffered */
            memcpy(file->buffer + file->bytesInBuffer, pData, size);
            file->bytesInBuffer += size;
            assert(file->bytesInBuffer <= file->bufferSize);
            return size;
        } else {
            /* buffer flush, start over */
            int bytesWritten;
            memcpy(file->buffer + file->bytesInBuffer, pData, remainingBufSize);
            assert(file->bytesInBuffer + remainingBufSize == file->bufferSize);
            file->bytesInBuffer = file->bufferSize;
            bytesWritten = FlushBFile(file);

            if (bytesWritten < 0)
                return bytesWritten;

            if (file->bytesInBuffer)
                return 0;

            memcpy(file->buffer, (char *)pData + remainingBufSize, size - remainingBufSize);
            file->bytesInBuffer = size - remainingBufSize;
            assert(file->bytesInBuffer <= file->bufferSize);

            return size;
        }
    }
}


bool PutCharBFile(BFile *file, char c)
{
    return WriteBFile(file, &c, 1) == 1;
}


int ReadBFile(BFile *file, void *pData, int size)
{
    int bytesRead, originalSize = size;
    auto data = (char *)pData;
    assert(file && pData && size >= 0);
    assert(file->readPtr >= 0);

    if (!size)
        return 0;

    /* get anything we have in buffer first */
    if (file->readPtr < file->bytesInBuffer) {
        int bytesInBuffer = file->bytesInBuffer - file->readPtr;
        if (size > bytesInBuffer) {
            memcpy(data, file->buffer + file->readPtr, bytesInBuffer);
            size -= bytesInBuffer;
            data += bytesInBuffer;

            if (size > file->bufferSize) {
                file->bytesInBuffer = 0;
                return ReadFile(file->handle, data, size) + bytesInBuffer;
            }
        } else {
            /* entirely in buffer */
            memcpy(data, file->buffer + file->readPtr, size);
            file->readPtr += size;
            return size;
        }
    } else {
        /* buffer empty, reset it */
        file->readPtr = 0;
        file->bytesInBuffer = 0;

        if (size > file->bufferSize)
            return ReadFile(file->handle, data, size);
    }
    /* fill buffer and return data */
    bytesRead = ReadFile(file->handle, file->buffer, file->bufferSize);
    if (bytesRead < 0)
        return bytesRead;

    file->readPtr = min(size, bytesRead);
    file->bytesInBuffer = bytesRead;
    memcpy(data, file->buffer, file->readPtr);
    return file->readPtr + originalSize - size;
}


int GetCharBFile(BFile *file)
{
    int c = 0, bytesRead = ReadBFile(file, &c, 1);
    if (bytesRead != 1)
        return -1;

    return c;
}


int PeekCharBFile(BFile *file)
{
    assert(file && file->readPtr >= 0);
    if (file->readPtr < file->bytesInBuffer)
        return file->buffer[file->readPtr] & 0xff;
    else {
        /* refill the buffer */
        int bytesRead = ReadFile(file->handle, file->buffer, file->bufferSize);
        if (bytesRead < 0)
            return -1;

        file->bytesInBuffer = bytesRead;
        file->readPtr = 0;

        if (!bytesRead)
            return -1;

        return file->buffer[0] & 0xff;
    }
}

bool UngetCharBFile(BFile *file, int c)
{
    assert(file && file->readPtr >= 0);
    assert(file->readPtr <= file->bufferSize);
    assert(file->bufferSize >= 0);
    if (c < 0)
        return true;

    if (file->bytesInBuffer > 0) {
        assert(file->bufferSize >= file->readPtr);
        if (file->readPtr > 0)
            file->buffer[--file->readPtr] = c;
        else {
            bool seekBack = file->bytesInBuffer == file->bufferSize;
            memmove(file->buffer + 1, file->buffer, min(file->bytesInBuffer, file->bufferSize - 1));
            file->bytesInBuffer = min(file->bytesInBuffer + 1, file->bufferSize);
            file->buffer[0] = c;

            /* seek back 1 byte in file, otherwise in the next read we will miss that byte that was kicked out */
            if (seekBack)
                return SeekFile(file->handle, SEEK_CUR, -1, -1) != -1;
        }
    } else {
        file->buffer[0] = c;
        file->bytesInBuffer = 1;
        file->readPtr = 0;
    }
    return true;
}


int SeekBFile(BFile *file, uchar mode, int ofsHi, int ofsLo)
{
    /* discard the buffer and seek, unless it's just get current offset request */
    if (ofsHi || ofsLo)
        FlushBFile(file);

    return SeekFile(file->handle, mode, ofsHi, ofsLo);
}


void CloseBFile(BFile *file)
{
    assert(file && file->buffer);
    FlushBFile(file);
    CloseFile(file->handle);
    qFree(file->buffer);
    qFree(file);
}


void CloseBFileUnmanaged(BFile *file)
{
    assert(file);
    FlushBFile(file);
    CloseFile(file->handle);
}
