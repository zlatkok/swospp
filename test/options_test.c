#include <stdlib.h>
#include <stdio.h>
#define sprintf __sprintf
#include "swos.h"
#include "options.h"
#include "qalloc.h"
#include "bfile_mock.h"
#define rand __rand
#define strtol __strtol
#include "util.h"
#undef rand
#undef strtol

/* can't include Windows.h or all hell will break loose */
extern unsigned long __cdecl _exception_code(void);
extern void * __cdecl _exception_info(void);

int main()
{
    extern void TurnLogOn();
    TurnLogOn();
    qAllocInit();
    atexit((void (*)())getchar);
    atexit(qAllocFinish);
    srand(time(nullptr));
    SetupWinFileEmulationLayer((uint8_t *)OpenBFile - 50, (uint8_t *)OpenBFile + 3 * 1024);
    SetupWinFileEmulationLayer((uint8_t *)SaveOptionsIfNeeded, (uint8_t *)SaveOptionsIfNeeded + 100);
    __try {
        InitializeOptions();
        SaveOptionsIfNeeded();
    } __except (HandleException(_exception_code(), _exception_info())) {}
    return 0;
}