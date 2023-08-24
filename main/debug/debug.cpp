#include <stdarg.h>
#include "bfile.h"

static BFile logFile;
static char logFileBuffer[4 * 1024];
static const char logFileName[] = "swospp.log";

//#define FLUSH_LOG_FILE  /* define this to flush log file after each line */
                        /* in case you wanna debug some juicy infinite loop for example ;) */

/* bit mask of allowed message classes to be written to the log */
static dword messageMask =  //LM_GAME_LOOP +
                            //LM_GAME_LOOP_STATE +
                            LM_SYNC +
                            //LM_WATCHER +
                            //LM_SUBS_MENU +
                            LM_PACKETS +
                            0;
static char logBuf[3 * 1024];

/* provided in vsprinf.c */
int __cdecl vsprintf(char *buf, const char *fmt, va_list arg);
int __cdecl sprintf(char *buf, const char *fmt, ...);

static void WriteToLogFuncV(const char *fmt, va_list args);

extern "C" __attribute((noreturn)) void AssertFailedMsg(const char *file, int line, const char *msg)
{
    asm("int 1");
    char msgBuf[128];
    int msgLen;

    if (msg) {
        msgLen = strncpy(msgBuf, msg, sizeof(msgBuf) - 1) - msgBuf;
        msgBuf[min(msgLen, (int)sizeof(msgBuf) - 1)] = '\0';
    } else {
        msgLen = sprintf(msgBuf, "Assertion failed at %s(%d).", file, line);
    }

    SwitchToPrevVideoMode();
    WriteToLog("Assertion failed! At %s:%d", file, line);

    if (msg)
        WriteToLog("%s", msg);

    if (msgLen > 0)
        PrintToStderr(msgBuf, msgLen);

    EndProgram(true);
}

extern "C" __attribute((noreturn)) void AssertFailed(const char *file, int line)
{
    asm("int 1");
    AssertFailedMsg(file, line, nullptr);
}

/** StartLogFile

    Makes sure the log file is open and writes introductory lines.
*/
extern "C" void StartLogFile()
{

    /* little hackie - plant 1 to file handle so any asserts during file creation will write to stdout */
    logFile.handle = 1;
    assert_msg(CreateBFileUnmanaged(&logFile, logFileBuffer, sizeof(logFileBuffer), ATTR_NONE, logFileName),
        "Failed to create debug log file.");
    WriteToLog("*** SWOS++ DEBUG VERSION ***");
    WriteToLog("Starting log file...");

    time_t rawTime;
    time(&rawTime);
    char *timeStr = ctime(&rawTime);
    timeStr[strlen(timeStr) - 1] = '\0';
    WriteToLog("The current local time is: %s", timeStr);
}

/** EndLogFile

    Writes ending message and closes log file.
*/
extern "C" void EndLogFile()
{
    WriteToLog("Log file ended succesfuly.");
    if (logFile.handle > 4)
        CloseBFileUnmanaged(&logFile);
}

/** FlushLogFile

    Reopen log file, forcing any changes to be commited to disk.
*/
extern "C" void FlushLogFile()
{
    CloseBFileUnmanaged(&logFile);
    assert_msg(OpenBFileUnmanaged(&logFile, logFileBuffer, sizeof(logFileBuffer), F_WRITE_ONLY, logFileName),
        "Failed to open debug log file.");
    SeekBFile(&logFile, SEEK_END, 0);
}

/** WriteToLogFunc

    Same as WriteToLogFunc, except it has additional message class parameter
    which will in conjunction with current message mask decide whether the
    message would be ignored or written to log.
*/
void WriteToLogFunc(dword messageClass, const char *fmt, ...)
{
    if (messageClass & messageMask) {
        va_list va;
        va_start(va, fmt);
        WriteToLogFuncV(fmt, va);
        va_end(va);
    }
}

/** WriteToLogFunc

    fmt - printf format string
    ... - additional parameters for format string

    Prints printf-format style string to log file, prepends time-stamp.
*/
extern "C" void __cdecl WriteToLogFunc(const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    WriteToLogFuncV(fmt, va);
    va_end(va);
}

/** WriteToLogFuncV

    fmt  - printf format string
    args - additional parameters for format string

    Prints printf-format style string to log file, prepends time-stamp.
*/
static void WriteToLogFuncV(const char *fmt, va_list args)
{
    unsigned char hour, minute, second, hundred;
    GetDosTime(&hour, &minute, &second, &hundred);

    int length = sprintf(logBuf, "[%02hu:%02hu:%02hu:%02hu] ", hour, minute, second, hundred);
    length += vsprintf(logBuf + length, fmt, args);

    assert_msg(logFile.handle, "Trying to log without initializing log file.");
    WriteBFile(&logFile, logBuf, length);
    PutCharBFile(&logFile, '\n');
#ifdef FLUSH_LOG_FILE
    FlushLogFile();
#endif
}

/** WriteToLogFuncNoStamp

    str - printf format string
    ... - additional parameters for format string

    Prints printf-format style string to log file, without the leading time-stamp.
*/
void __cdecl WriteToLogFuncNoStamp(const char *str, ...)
{
    int length;
    va_list va;

    va_start(va, str);
    length = vsprintf(logBuf, str, va);
    va_end(va);

    WriteBFile(&logFile, logBuf, length);
    PutCharBFile(&logFile, '\n');
}

/** HexDumpToLog

    addr   - beginning address of dump
    length - number of bytes to dump
    title  - title for hex dump, for easier output interpretation

    Prints memory hex dump to log file. It prints it line by line, so it's able
    to dump memory blocks of arbitrary length. However it takes a while to dump
    big blocks, so don't go wild. :P
*/
void HexDumpToLog(const void *inAddr, int length, const char *title)
{
    char buf[2048];
    unsigned char *q, *addr = (unsigned char *)inAddr;

    if (!length || !addr)
        return;

    if (title)
        WriteToLogFunc("Hex dump of %s, size = %d\n", title, length);

    WriteToLogFuncNoStamp("   Address                    Hexadecimal values                    Printable");
    WriteToLogFuncNoStamp("--------------  -----------------------------------------------  ----------------\n");

    while (length > 0) {
        int i, lineOutput;

        length -= 16;
        int lineCount = length < 0 ? 16 + length : 16;
        char *p = buf + sprintf(buf, "   %08lx     ", (int)addr);

        for (i = lineOutput = 0, q = addr; i < 16; i++, q++) {
            lineOutput++ < lineCount ? (p += sprintf(p, "%02x", *q)) : (*p++ = ' ', *p++ = ' ', p);
            *p++ = ' ';
        }

        *p++ = ' ';

        for (i = 0, q = addr; i < lineCount; i++, q++)
            *p++ = *q < 32 || *q > 126 ? '.' : *q;

        *p = '\0';
        WriteToLogFuncNoStamp("%s", buf);
        addr += 16;
    }
    WriteToLogFuncNoStamp("");
}

/** HexDumpToLogM

    Dump memory to the log if message class bit is set in the current mask.
*/
void HexDumpToLog(dword messageClass, const void *addr, int length, const char *title)
{
    if (messageClass & messageMask)
        HexDumpToLog(addr, length, title);
}
