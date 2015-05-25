#include "swos.h"
#include "util.h"
#include "crashlog.h"
#include "dos.h"
#include "qalloc.h"
#include "dosipx.h"
#include "options.h"

/* exception context */
static byte exceptionNo asm("exceptionNo") __attribute__((used));
static dword rEax asm("rEax") __attribute__((used));
static dword rEbx asm("rEbx") __attribute__((used));
static dword rEcx asm("rEcx") __attribute__((used));
static dword rEdx asm("rEdx") __attribute__((used));
static dword rEsi asm("rEsi") __attribute__((used));
static dword rEdi asm("rEdi") __attribute__((used));
static dword rEbp asm("rEbp") __attribute__((used));
static dword rEip asm("rEip") __attribute__((used));
static dword rEsp asm("rEsp") __attribute__((used));
static word rCs asm("rCs") __attribute__((used));
static word rSs asm("rSs") __attribute__((used));

void ExceptionHandler() asm("ExceptionHandler");
void UserExceptionHandler() asm ("UserExceptionHandler");

#define DeclareExceptionHandler(aExceptionNo)               \
void Exception ## aExceptionNo ## Handler() asm("Exception" #aExceptionNo "Handler");    \
asm(                                                        \
    "Exception" #aExceptionNo "Handler:                 \n" \
    "mov  dword ptr [exceptionNo], " # aExceptionNo "   \n" \
    "jmp  ExceptionHandler                              \n" \
);

DeclareExceptionHandler(0);
DeclareExceptionHandler(6);
DeclareExceptionHandler(10);
DeclareExceptionHandler(11);
DeclareExceptionHandler(12);
DeclareExceptionHandler(13);
extern "C" void Exception14Handler();           // inline this one below

/** ExceptionHandler

    Will save registers and point return address for our user-mode handler to use.
    Thanks Ralph Brown's Interrupt List for the offsets!
    And why oh why does GCC not support naked attribute on x86?!
*/
asm(
    "Exception14Handler:        \n"             // squeeze in last handler here to eliminate jmp $+2 instruction ;)
    "mov   dword ptr [exceptionNo], 14  \n"

    "ExceptionHandler:          \n"

    "mov   rEax, eax            \n"
    "mov   rEbx, ebx            \n"
    "mov   rEcx, ecx            \n"
    "mov   rEdx, edx            \n"
    "mov   rEbp, ebp            \n"
    "mov   rEsi, esi            \n"
    "mov   rEdi, edi            \n"
    "mov   eax, [esp + 24]      \n"
    "mov   rEsp, eax            \n"
    "mov   eax, [esp + 12]      \n"
    "mov   rEip, eax            \n"
    "mov   ax, [esp + 16]       \n"
    "mov   rCs, ax              \n"
    "mov   ax, [esp + 28]       \n"
    "mov   rSs, ax              \n"
    "mov   ax, cs               \n"
    "mov   [esp + 16], ax       \n"
    "mov   eax, offset UserExceptionHandler\n"
    "mov   [esp + 12], eax      \n"
    "mov   eax, rEax            \n" // keep registers intact
    "retf                       \n"
);

/* Seems this is the only DPMI service DOS/4GW is supporting for this purpose. */
static bool InstallExceptionHandler(int exceptionNo, void (*handler)())
{
    bool result;
    asm volatile (
        "mov  ax, 0x203     \n"
        "mov  cx, cs        \n"
        "int  0x31          \n"
        "sbb  eax, eax      \n"
        "not  eax           \n"
        : "=a" (result)
        : "b" (exceptionNo), "d" (handler)
        : "cc", "cx"
    );
    return result;
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
    if (exceptionNo > (int)sizeofarray(exceptionNames))
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