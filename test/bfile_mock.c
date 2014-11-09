/** What we do here? Overwrite int 0x21 opcodes in code stream with invalid opcode, and
    install exception handler that specifically looks for invalid opcode exceptions.
    Then after one is found take over parameters in registers to int 0x21 and substitute
    functionality using Win32 API - the caller won't have a clue ;)
*/
#include <windows.h>

/** SetupWinFileEmulationLayer

    start - beginning of code to process
    end   - end of code (don't touch this byte)
*/
void SetupWinFileEmulationLayer(unsigned char *start, const unsigned char *end)
{
    DWORD oldProtect;
    unsigned char *base = start;
    /* allow write to memory block */
    VirtualProtect(base, end - base, PAGE_READWRITE, &oldProtect);
    /* look for pattern:
       mov ah, xx   ; b4 xx
       int 0x21     ; cd 21

       and replace int 0x21 with ud2 instruction to cause invalid opcode exception */
    while (start < end - 4) {
        if (*start == 0xb4 && start[2] == 0xcd && start[3] == 0x21) {
            start[2] = 0x0f;
            start[3] = 0x0b;
            start += 4;
        } else
            start++;
    }
    /* restore access mode to memory block */
    VirtualProtect(base, end - base, oldProtect, &oldProtect);
}

static void clearCarry(EXCEPTION_POINTERS *ei)
{
    ei->ContextRecord->EFlags &= ~1;
}

static void setCarry(EXCEPTION_POINTERS *ei, int val)
{
    ei->ContextRecord->EFlags |= val == 0;
}

/** HandleException

    exceptionCode -  code of exception that occured
    ei            -> EXCEPTION_POINTERS struct, description of exception, and CPU registers and state

    Filter exceptons, only handle invalid opcode which is deliberately generated
    in place of int 21h calls, and replace its functionality using win32 API.
    If handled successfully skip invalid bytes and continue execution.
    If exception wasn't recognized signal other exception handlers to take over.
*/
int HandleException(DWORD exceptionCode, EXCEPTION_POINTERS *ei)
{
    DWORD bytesProcessed;
    if (exceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
        if (*(unsigned char *)(ei->ContextRecord->Eip - 2) != 0xb4)
            return EXCEPTION_CONTINUE_SEARCH;
        switch ((ei->ContextRecord->Eax >> 8) & 0xff) {
        case 0x3c:  /* create file, ignore file attribute */
            clearCarry(ei);
            ei->ContextRecord->Eax = (DWORD)CreateFile((const char *)ei->ContextRecord->Edx,
                GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            setCarry(ei, ei->ContextRecord->Eax);
            break;
        case 0x3d:  /* open file, ignore access and sharing modes */
            clearCarry(ei);
            ei->ContextRecord->Eax = (DWORD)CreateFile((const char *)ei->ContextRecord->Edx,
                GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            setCarry(ei, ei->ContextRecord->Eax);
            break;
        case 0x40:  /* write file */
            clearCarry(ei);
            setCarry(ei, WriteFile((HANDLE)ei->ContextRecord->Ebx, (LPCVOID)ei->ContextRecord->Edx,
                ei->ContextRecord->Ecx, &bytesProcessed, NULL));
            ei->ContextRecord->Eax = bytesProcessed;
            break;
        case 0x3e:  /* close file */
            clearCarry(ei);
            setCarry(ei, CloseHandle((HANDLE)ei->ContextRecord->Ebx));
            break;
        case 0x3f:  /* read file */
            clearCarry(ei);
            setCarry(ei, ReadFile((HANDLE)ei->ContextRecord->Ebx, (LPVOID)ei->ContextRecord->Edx,
                ei->ContextRecord->Ecx, &bytesProcessed, NULL));
            ei->ContextRecord->Eax = bytesProcessed;
            break;
        case 0x42:  /* seek file */
            clearCarry(ei);
            ei->ContextRecord->Eax = SetFilePointer((HANDLE)ei->ContextRecord->Ebx,
                ei->ContextRecord->Edx, (LONG *)&ei->ContextRecord->Ecx, ei->ContextRecord->Eax & 3);
            break;
        default:
            return EXCEPTION_CONTINUE_SEARCH;
        }
        ei->ContextRecord->ContextFlags |= CONTEXT_CONTROL | CONTEXT_INTEGER;
        ei->ContextRecord->Eip += 2;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}