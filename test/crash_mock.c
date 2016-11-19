/* Add here whatever's missing from SWOS/SWOS++ */
#include <assert.h>
#undef assert
#include "types.h"
#include "util.h"
#include "swos.h"

char pitchDatBuffer[10 * 1024];

void __cdecl WriteToLogFunc(const char *fmt, ...) {}

void AssertFailed(const char *file, int line)
{
    _assert("Crash logger test", (char *)file, line);
}

bool SaveXmlFile(void * tree, const char *fileName)
{
    return true;
}

void SwitchToPrevVideoMode() {}

void EndProgram()
{
    exit(1);
}