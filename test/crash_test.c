/**
    crash_test.c

    Module for testing crash reporter.
*/

#include <stdlib.h>
#include <stdio.h>
#define rand(a) swospp_rand
#define strtol(a, b, c, d) swospp_strtol
#include "types.h"
#include "util.h"
#undef strtol
#undef rand
#define sprintf(a, b, c) swospp_sprintf
#include "swos.h"
#undef sprintf
#include "dos.h"
#include "crashlog.h"


/* We will hijack this and simply end the program with 0 - successful run. */
void EndLogFile()
{
    exit(0);
}

#pragma aux GenerateCPUException aborts;
void GenerateCPUException(int exceptionNo)
{
    static int x, y;
    switch (exceptionNo) {
    case 0:
        x /= y;
    case 6:
        _asm db 0x0f, 0x0b  ; ud2
    case 13:
        _asm {
            mov  ax, cs
            mov  ss, ax
        }
    default:
        printf("I don't know how to generate exception %d.\n", exceptionNo);
        exit(1);
    }
}

int main(int argc, char **argv)
{
    int exceptionNo;
    if (!InstallCrashLogger()) {
        printf("Error installing crash logger!\n");
        return 1;
    }
    if (argc < 2) {
        printf("Need exception argument.\n");
        return 1;
    }
    exceptionNo = atoi(argv[1]);
    if (exceptionNo > 16 || exceptionNo == 15) {
        printf("%d is not a valid exception number.\n", exceptionNo);
        return 1;
    }
    GenerateCPUException(exceptionNo);
    return 0;
}
