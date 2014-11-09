#include "swos.h"
#include "util.h"
#include "crashlog.h"
#include "dos.h"
#include "qalloc.h"
#include "dosipx.h"
#include "options.h"

/* exception context */
static byte exceptionNo;
static dword rEax;
static dword rEbx;
static dword rEcx;
static dword rEdx;
static dword rEsi;
static dword rEdi;
static dword rEbp;
static dword rEip;
static dword rEsp;
static word rCs;
static word rSs;

static void UserExceptionHandler();
static __declspec(naked) void ExceptionHandler();
#pragma aux InstallExceptionHandler parm [ebx] [edx];
static bool __declspec(naked) InstallExceptionHandler(int exceptionNo, void (*handler)());

#define DeclareExceptionHandler(aExceptionNo)                           \
static __declspec(naked) void Exception ## aExceptionNo ## Handler();   \
static void Exception ## aExceptionNo ## Handler()                      \
{                                                                       \
    _asm { mov   exceptionNo, aExceptionNo }                            \
    _asm { jmp   ExceptionHandler }                                     \
}

DeclareExceptionHandler(0);
DeclareExceptionHandler(6);
DeclareExceptionHandler(10);
DeclareExceptionHandler(11);
DeclareExceptionHandler(12);
DeclareExceptionHandler(13);
DeclareExceptionHandler(14);

/** ExceptionHandler

    Will save registers and point return address for our user-mode handler to use.
    Thanks Ralph Brown's Interrupt List for the offsets!
*/
static void ExceptionHandler()
{
    _asm {
        mov   rEax, eax
        mov   rEbx, ebx
        mov   rEcx, ecx
        mov   rEdx, edx
        mov   rEbp, ebp
        mov   rEsi, esi
        mov   rEdi, edi
        mov   eax, [esp + 0x18]
        mov   rEsp, eax
        mov   eax, [esp + 0x0c]
        mov   rEip, eax
        mov   ax, [esp + 0x10]
        mov   rCs, ax
        mov   ax, [esp + 0x1c]
        mov   rSs, ax
        mov   ax, cs
        mov   [esp + 0x10], ax
        mov   eax, offset UserExceptionHandler
        mov   [esp + 0x0c], eax
        mov   eax, rEax     ; keep registers intact
        retf
    }
}

/* Seems this is the only DPMI service DOS/4GW is supporting for this purpose. */
bool InstallExceptionHandler(int exceptionNo, void (*handler)())
{
    _asm {
        mov   ax, 0x203
        mov   cx, cs
        int   0x31
        sbb   eax, eax
        not   eax
        retn
    }
}

static const char *getExceptionName(int exceptionNo)
{
    static const char *exceptionNames[] = {
        "divide by zero",
        "single step/debug exception",
        "debug exception",
        "breakpoint",
        "overflow",
        "bounds",
        "invalid opcode",
        "device not available",
        "double fault",
        "overrun",
        "invalid TSS",
        "segment not present",
        "stack fault",
        "general protection fault",
        "page fault",
        "unknown exception",
        "coprocessor error",
    };
    if (exceptionNo > sizeofarray(exceptionNames))
        return "unknown exception";
    return exceptionNames[exceptionNo];
}

/** UserExceptionHandler

    Something bad has just happened. Print out the info (both on screen and to log), and exit.
*/
void UserExceptionHandler()
{
    char *buf = (char *)pitch_dat_buffer;   /* he won't need it anymore... */
    int charCount = sprintf(buf,
        "End of the road\r\n"
        "===============\r\n"
        "How misfortunate. Seems we have crash landed.\r\n"
        "If you know someone who can do something about it, you might\r\n"
        "as well find the log file (SWOSPP.LOG) and send it to them.\r\n\n"
        "Detailed information:\r\n"
        "Exception: %d - %s\r\n"
        "Address: %04x:%08x\r\n"
        "Known registers:\r\n"
        "EAX %08x EDX %08x EBP %08x\r\n"
        "EBX %08x ESI %08x ESP %08x\r\n"
        "ECX %08x EDI %08x SS      %04x\r\n",
        exceptionNo, getExceptionName(exceptionNo), rCs, rEip,
        rEax, rEdx, rEbp, rEbx, rEsi, rEsp, rEcx, rEdi, rSs
     );
     WriteToLog(("\n%s", buf));
     assert(charCount < 10032 - 1);
     buf[charCount] = '\n';
     SwitchToPrevVideoMode();
     PrintToStderr(buf, charCount + 1);
     EndProgram(true);
}

/** InstallCrashLogger

    Call at initialization - will set up handlers to dump crash data into the log.
*/
bool InstallCrashLogger()
{
    if (!InstallExceptionHandler(13, Exception13Handler) || !InstallExceptionHandler(14, Exception14Handler) ||
        !InstallExceptionHandler(0, Exception0Handler) || !InstallExceptionHandler(6, Exception6Handler) ||
        !InstallExceptionHandler(10, Exception10Handler) || !InstallExceptionHandler(12, Exception12Handler) ||
        !InstallExceptionHandler(11, Exception11Handler)) {
        WriteToLog(("Crash logger failed to install one or more exception handlers."));
        return false;
    } else {
        WriteToLog(("Crash logger installed successfully."));
        return true;
    }
}