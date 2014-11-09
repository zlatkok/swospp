#define sprintf libc_sprintf
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#undef assert
#include "types.h"
#include "util.h"
#undef sprintf
#include "swos.h"

static bool silentLog = true;

void TurnLogOn()
{
    silentLog = false;
}

void TurnLogOff()
{
    silentLog = true;
}

void HexDumpToLog(const void *inAddr, int length, const char *title) {}

/* Just make it stop by default. */
void __cdecl WriteToLogFunc(const char *fmt, ...)
{
    if (!silentLog) {
        va_list va;
        va_start(va, fmt);
        vprintf(fmt, va);
        putchar('\n');
        va_end(va);
    }
}

void AssertFailed(const char *file, int line)
{
    _asm int 3
    _assert("qAlloc test", (char *)file, line);
}

void AssertFailedMsg(const char *file, int line, const char *msg)
{
    _asm int 3
    _assert((char *)msg, (char *)file, line);
}

int __cdecl sprintf(char *buf, const char *fmt, ...)
{
    int result;
    va_list arglist;
    va_start(arglist, fmt);
    result = vsprintf(buf, fmt, arglist);
    va_end(arglist);
    return result;
}