#include "utils/CoreDump.h"
#include "utils/CExceptionInit.h"
#include "utils/Utils.h"

#include <Dbghelp.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <ctime>

#include <varargs.h>

void CoreDump::createReport(_EXCEPTION_POINTERS* ex)  // 0x004169E0
{
    HANDLE hFile = ::CreateFileA(CExceptionInit::s_logPath, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_FLAG_WRITE_THROUGH | FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        ::OutputDebugStringA("Error creating exception report");
        return;
    }

    ::SetFilePointer(hFile, 0, 0, FILE_END);

    SYSTEMTIME now;
    now.wYear = 0;
    now.wMonth = 0;
    now.wDayOfWeek = 0;
    now.wDay = 0;
    now.wHour = 0;
    now.wMinute = 0;
    now.wSecond = 0;
    now.wMilliseconds = 0;
    ::GetLocalTime(&now);

    CoreDump::writeLine(hFile, "[(%d) %04d/%02d/%02d %02d:%02d:%02d]: =======================\r\n", ::GetCurrentThreadId(), now.wMonth, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    EXCEPTION_RECORD* exceptionRecord = ex->ExceptionRecord;
    CONTEXT* context = ex->ContextRecord;
    const char* moduleFileName = "Unknown";
    MEMORY_BASIC_INFORMATION memoryInfo;
    char moduleFilePath[MAX_PATH];
    if (::VirtualQuery((void*)context->Eip, &memoryInfo, sizeof(memoryInfo)) && ::GetModuleFileNameA((HMODULE)memoryInfo.AllocationBase, moduleFilePath, sizeof(moduleFilePath)))
    {
        moduleFileName = Utils::getFileName(moduleFilePath);
    }

    const char* exceptionStr = CoreDump::exceptionToString(exceptionRecord->ExceptionCode);
    CoreDump::writeLine(hFile, "%s in module %s at %04x:%08x.\r\n", exceptionStr, moduleFileName, context->SegCs, context->Eip);
    CoreDump::writeLine(hFile, "Exception handler called in the L2AuthD Server.\r\n");

    tm* startTime = std::localtime(&CExceptionInit::s_startTime);
    CoreDump::writeLine(hFile, "start at %d/%d/%d %02d:%02d:%02d\r\n", startTime->tm_year + 1900, startTime->tm_mon + 1, startTime->tm_mday, startTime->tm_hour, startTime->tm_min, startTime->tm_sec);

    CoreDump::writeExtraInfo(hFile);
    if ((exceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) && (exceptionRecord->NumberParameters >= 2))
    {
        // ExceptionInformation[0] indicates if it was a read (0), write (1), or data execution
        // attempt (8).
        const bool readFrom = exceptionRecord->ExceptionInformation[0] == 0;
        char violationBuffer[1000];
        wsprintfA(violationBuffer, "%s location %08x caused an access violation.\r\n", readFrom ? "Read from" : "Write to", exceptionRecord->ExceptionInformation[1]);
        CoreDump::writeLine(hFile, "%s", violationBuffer);
    }

    CoreDump::writeLine(hFile, "\r\n");
    CoreDump::writeLine(hFile, "Registers:\r\n");
    CoreDump::writeLine(hFile, "EAX=%08x CS=%04x EIP=%08x EFLGS=%08x\r\n", context->Eax, context->SegCs, context->Eip, context->EFlags);
    CoreDump::writeLine(hFile, "EBX=%08x SS=%04x ESP=%08x EBP=%08x\r\n", context->Ebx, context->SegSs, context->Esp, context->Ebp);
    CoreDump::writeLine(hFile, "ECX=%08x DS=%04x ESI=%08x FS=%04x\r\n", context->Ecx, context->SegDs, context->Esi, context->SegFs);
    CoreDump::writeLine(hFile, "EDX=%08x ES=%04x EDI=%08x GS=%04x\r\n", context->Edx, context->SegEs, context->Edi, context->SegGs);
    CoreDump::writeLine(hFile, "Bytes at CS:EIP:\r\n");

    for (int i = 0; i < 16; ++i)
    {
        const char* eip = (const char*)context->Eip;
        CoreDump::writeLine(hFile, "%02x ", eip[i]);
    }

    CoreDump::writeLine(hFile, "\r\nStack dump:\r\n");

    int* stackPointer = (int*)context->Esp;
    int* stackBase = (int*)__readfsdword(4);
    if (stackBase > (stackPointer + 80))
    {
        stackBase = (stackPointer + 80);
    }

    int counter = 0;
    char stackBuffer[1000];
    memset(stackBuffer, 0, sizeof(stackBuffer));

    const char* limit = &stackBuffer[950];
    char* iterator = stackBuffer;
    while (stackPointer + 1 <= stackBase)
    {
        if ((counter % 8) == 0)
        {
            iterator += wsprintfA(iterator, "%08x: ", stackPointer);
        }

        const char* spaceOrEoL = " ";
        if (((++counter % 8) == 0) || (stackPointer + 2 > stackBase))
        {
            spaceOrEoL = "\r\n";
        }

        iterator += wsprintfA(iterator, "%08x: %S", *stackPointer, spaceOrEoL);
        ++stackPointer;
        if (iterator > limit)
        {
            CoreDump::writeLine(hFile, "%s", stackBuffer);
            stackBuffer[0] = 0;
            iterator = stackBuffer;
        }
    }

    CoreDump::writeLine(hFile, "%s", stackBuffer);

    HANDLE currProc = ::GetCurrentProcess();
    if (::SymInitialize(currProc, 0, true))  // debug symbols are found
    {
        CoreDump::ImageHelpStackWalk(hFile, context);
        ::SymCleanup(currProc);
    }
    else
    {
        CoreDump::IntelStackWalk(hFile, context);
    }
    CoreDump::writeModules(hFile);

    ::CloseHandle(hFile);
}

void CoreDump::ImageHelpStackWalk(HANDLE hFile, CONTEXT* contextRecord)  // 0x00415DD8
{
    CoreDump::writeLine(hFile, "\r\nCall Stack Information\r\n");
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS);

    STACKFRAME stackFrame;
    memset(&stackFrame, 0, sizeof(stackFrame));
    stackFrame.AddrPC.Offset = contextRecord->Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = contextRecord->Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = contextRecord->Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;

    while (true)
    {
        HANDLE currThread = ::GetCurrentThread();
        HANDLE currProc = ::GetCurrentProcess();

        // The GetModuleBase function retrieves the base address of the module that contains the
        // specified address.
        bool result = ::StackWalk(IMAGE_FILE_MACHINE_I386, currProc, currThread, &stackFrame, contextRecord, 0, &::SymFunctionTableAccess, &::SymGetModuleBase, NULL) != FALSE;

        if (!result || stackFrame.AddrFrame.Offset == 0)
        {
            break;
        }

        CoreDump::writeLine(hFile, "%08x: 08x", stackFrame.AddrPC.Offset, stackFrame.AddrFrame.Offset);

        char symbolBuffer[sizeof(IMAGEHLP_SYMBOL) + 512];
        IMAGEHLP_SYMBOL* imageSymbol = imageSymbol = (IMAGEHLP_SYMBOL*)symbolBuffer;
        // imageSymbol->SizeOfStruct = sizeof(symbolBuffer);
        imageSymbol->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);  // FIXED
        imageSymbol->MaxNameLength = 512;

        DWORD dwDisp = 0;
        int pc = stackFrame.AddrPC.Offset;
        if (::SymGetSymFromAddr(currProc, pc, &dwDisp, imageSymbol))
        {
            CoreDump::writeLine(hFile, "%s+%x ", imageSymbol->Name, dwDisp);
        }

        const char* filename = "Unknown";
        char exeFilename[MAX_PATH];
        MEMORY_BASIC_INFORMATION memoryInfo;
        if (::VirtualQuery((void*)stackFrame.AddrPC.Offset, &memoryInfo, sizeof(memoryInfo)) && ::GetModuleFileNameA((HMODULE)memoryInfo.AllocationBase, exeFilename, MAX_PATH))
        {
            filename = Utils::getFileName(exeFilename);
        }

        char moduleFilename[MAX_PATH];
        memset(moduleFilename, 0, MAX_PATH);

        int offset = 0;
        int section = 0;
        CoreDump::GetLogicalAddress((const void*)stackFrame.AddrPC.Offset, moduleFilename, MAX_PATH, &section, &offset);
        CoreDump::writeLine(hFile, "%04x:%08x %s\r\n", section, offset, moduleFilename);
        CoreDump::writeLine(hFile, "Params: %08x %08x %08x %08x\r\n", stackFrame.Params[0], stackFrame.Params[1], stackFrame.Params[2], stackFrame.Params[3]);

        CoreDump::writeLine(hFile, "[%s] Bytes at CS:EIP: ", filename);
        char* pcOffset = (char*)stackFrame.AddrPC.Offset;
        for (int i = 0; i < 16; ++i)
        {
            CoreDump::writeLine(hFile, "%02x ", pcOffset[i]);
        }

        CoreDump::writeLine(hFile, "\r\n\r\n");
    }
}

void CoreDump::IntelStackWalk(HANDLE hFile, CONTEXT* contextRecord)  // 0x004161BE
{
    CoreDump::writeLine(hFile, "\r\nIntel Call Stack Information\r\n");
    const int* pcOffset = (const int*)contextRecord->Eip;
    const int* addrFrameOffset = (const int*)contextRecord->Ebp;
    do
    {
        char moduleFilename[MAX_PATH];
        moduleFilename[0] = 0;
        memset(&moduleFilename[1], 0, 256u);
        moduleFilename[257] = 0;
        moduleFilename[258] = 0;
        moduleFilename[259] = 0;
        int section = 0;
        int offset = 0;
        CoreDump::GetLogicalAddress(pcOffset, moduleFilename, MAX_PATH, &section, &offset);
        CoreDump::writeLine(hFile, "%08x: 08x04x08xS", pcOffset, addrFrameOffset, section, offset, moduleFilename);

        CoreDump::writeLine(hFile, "Bytes at CS:EIP: ");
        const char* stackIt = (const char*)pcOffset;
        for (int i = 0; i < 16; ++i)
        {
            CoreDump::writeLine(hFile, "%02x ", stackIt[i]);
        }
        CoreDump::writeLine(hFile, "\r\n\r\n");

        pcOffset = (int*)addrFrameOffset[1];
        const int* addrFrameOffsetPrev = addrFrameOffset;
        addrFrameOffset = (int*)*addrFrameOffset;  // TODO: increment??

        if ((int)addrFrameOffset & 3)
        {
            break;
        }

        if (addrFrameOffset <= addrFrameOffsetPrev)
        {
            break;
        }
    } while (::IsBadWritePtr((void*)addrFrameOffset, 8) != FALSE);
}

bool CoreDump::GetLogicalAddress(LPCVOID addr, LPSTR moduleName, DWORD nSize, int* section, int* offset)  // 0x00415CD1
{
    MEMORY_BASIC_INFORMATION memoryInfo;
    if (::VirtualQuery(addr, &memoryInfo, sizeof(memoryInfo)) == 0)
    {
        return false;
    }

    DWORD hMod = (DWORD)memoryInfo.AllocationBase;
    if (::GetModuleFileNameA((HMODULE)hMod, moduleName, nSize) == 0)
    {
        return false;
    }

    // Point to the DOS header in memory
    IMAGE_DOS_HEADER* pDosHdr = (IMAGE_DOS_HEADER*)hMod;

    // From the DOS header, find the NT (PE) header
    IMAGE_NT_HEADERS* pNtHdr = (IMAGE_NT_HEADERS*)(hMod + pDosHdr->e_lfanew);
    IMAGE_SECTION_HEADER* pSection = IMAGE_FIRST_SECTION(pNtHdr);

    DWORD rva = (DWORD)addr - hMod;  // RVA is offset from module loadaddress
    int sectionIndex = 0;

    // Iterate through the section table, looking for the one that encompasses
    // the linear address.
    for (int i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++, pSection++)
    {
        DWORD sectionStart = pSection->VirtualAddress;
        DWORD sectionEnd = sectionStart + max(pSection->SizeOfRawData, pSection->Misc.PhysicalAddress);

        // Is the address in this section???
        if ((rva >= sectionStart) && (rva <= sectionEnd))
        {
            // Yes, address is in the section.  Calculate section and offset,
            // and store in the "section" & "offset" params, which were
            // passed by reference.

            *section = sectionIndex + 1;
            *offset = rva - sectionStart;
            return true;
        }
        ++sectionIndex;
        ++pSection;
    }

    return false;  // Should never get here!
}

void CoreDump::writeLine(HANDLE hFile, const char* format, ...)  // 0x0041612B
{
    va_list va;
    va_start(va, format);

    char buffer[2016];
    ::wvsprintfA(buffer, format, va);
    int len = ::lstrlenA(buffer);
    DWORD written;
    ::WriteFile(hFile, buffer, len, &written, 0);
}

void CoreDump::writeModules(HANDLE hFile)  // 0x0041703B
{
    CoreDump::writeLine(hFile, "\r\n\tModule list: names, addresses, sizes, time stamps and file times:\r\n");

    SYSTEM_INFO systemInfo;
    ::GetSystemInfo(&systemInfo);

    int pageSize = systemInfo.dwPageSize;
    int pagesPerGb = 1024u * 1024u * 1024u / systemInfo.dwPageSize;
    int pageCount = 4u * pagesPerGb;  // 4 GB / pageSize => TODO: x64
    int pageIndex = 0;
    void* baseAddress = NULL;
    while (true)
    {
        if (pageIndex >= pageCount)
        {
            break;
        }

        MEMORY_BASIC_INFORMATION memoryInfo;
        if (::VirtualQuery((LPCVOID)(pageSize * pageIndex), &memoryInfo, sizeof(memoryInfo)) == 0)
        {
            pageIndex += 0x10000 / pageSize;
            continue;
        }

        if (memoryInfo.RegionSize == 0)
        {
            pageIndex += 0x10000 / pageSize;
            continue;
        }

        pageIndex += memoryInfo.RegionSize / pageSize;
        if ((memoryInfo.State == MEM_COMMIT) && (baseAddress < memoryInfo.AllocationBase))
        {
            baseAddress = memoryInfo.AllocationBase;
            CoreDump::writeModuleInfo(hFile, (HMODULE)memoryInfo.AllocationBase);
        }
    }
}

void CoreDump::writeModuleInfo(HANDLE hFile, HMODULE hModule)  // 0x00417112
{
    char moduleFilename[MAX_PATH];
    if (::GetModuleFileNameA(hModule, moduleFilename, sizeof(moduleFilename)) == 0)
    {
        return;
    }

    IMAGE_DOS_HEADER* imgHeader = (IMAGE_DOS_HEADER*)hModule;
    if (imgHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return;
    }

    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((char*)imgHeader + imgHeader->e_lfanew);
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
    {
        return;
    }

    HANDLE moduleFile = ::CreateFileA(moduleFilename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    char buffer[100];
    memset(buffer, 0, sizeof(buffer));

    int fileSize = 0;
    if (moduleFile != INVALID_HANDLE_VALUE)
    {
        fileSize = ::GetFileSize(moduleFile, 0);
        FILETIME lastWriteTime;
        if (::GetFileTime(moduleFile, 0, 0, &lastWriteTime))
        {
            wsprintfA(buffer, " - file date is ");
            int buffEnd = lstrlenA(buffer);
            CoreDump::getFileTime(&buffer[buffEnd], lastWriteTime);
        }
        ::CloseHandle(moduleFile);
    }

    CoreDump::writeLine(hFile, "%s, loaded at 0x%08x - %d bytes - %08x%s\r\n", moduleFilename, hModule, fileSize, ntHeader->FileHeader.TimeDateStamp, buffer);
}

void CoreDump::writeExtraInfo(HANDLE hFile)  // 0x004173B0
{
    FILETIME systemTimeAsFileTime;
    ::GetSystemTimeAsFileTime(&systemTimeAsFileTime);

    char logCreationTime[100];
    CoreDump::getFileTime(logCreationTime, systemTimeAsFileTime);
    CoreDump::writeLine(hFile, "Error occurred at %s.\r\n", logCreationTime);

    char moduleFilePathName[MAX_PATH];
    if (!::GetModuleFileNameA(0, moduleFilePathName, sizeof(moduleFilePathName)))
    {
        lstrcpyA(moduleFilePathName, "Unknown");
    }

    char buffer[200];
    DWORD pcbBuffer = sizeof(buffer);
    if (!::GetUserNameA(buffer, &pcbBuffer))
    {
        lstrcpyA(buffer, "Unknown");
    }

    CoreDump::writeLine(hFile, "%s, run by %s.\r\n", moduleFilePathName, buffer);

    SYSTEM_INFO systemInfo;
    ::GetSystemInfo(&systemInfo);
    CoreDump::writeLine(hFile, "%d processor(s), type %d.\r\n", systemInfo.dwNumberOfProcessors, systemInfo.dwProcessorType);

    MEMORYSTATUS memoryStatus;
    memoryStatus.dwLength = sizeof(MEMORYSTATUS);
    ::GlobalMemoryStatus(&memoryStatus);

    // CoreDump::writeLine(hFile, "%d MBytes physical memory.\r\n", (memoryStatus.dwTotalPhys +
    // 0xFFFFF) >> 20);
    // FIXED:
    size_t physMemory = memoryStatus.dwTotalPhys;
    size_t mbytes = physMemory >> 20;
    CoreDump::writeLine(hFile, "%d MBytes physical memory.\r\n", mbytes);
}

void CoreDump::getFileTime(char* out, FILETIME fileTime)  // 0x004172E8
{
    bool success = ::FileTimeToLocalFileTime(&fileTime, &fileTime) == TRUE;
    if (!success)
    {
        *out = '\0';
        return;
    }

    WORD fatTime;
    WORD fatDate;
    success = ::FileTimeToDosDateTime(&fileTime, &fatDate, &fatTime) == TRUE;
    if (!success)
    {
        *out = '\0';
        return;
    }

    wsprintfA(out, "%d/%d/%d %02d:%02d:%02d", (fatDate >> 5) & 15, fatDate & 31, (fatDate >> 9) + 1980, fatTime >> 11, (fatTime >> 5) & 63, 2 * (fatTime & 31));
}

const char* CoreDump::exceptionToString(int exceptionCode)  // 0x004174C8
{
    struct ExceptionName
    {
        int code;
        const char* name;
    };

    ExceptionName exceptions[24];
    exceptions[0].code = DBG_CONTROL_C;
    exceptions[0].name = "a Control-C";
    exceptions[1].code = DBG_CONTROL_BREAK;
    exceptions[1].name = "a Control-Break";
    exceptions[2].code = EXCEPTION_DATATYPE_MISALIGNMENT;
    exceptions[2].name = "a Datatype Misalignment";
    exceptions[3].code = EXCEPTION_BREAKPOINT;
    exceptions[3].name = "a Breakpoint";
    exceptions[4].code = EXCEPTION_ACCESS_VIOLATION;
    exceptions[4].name = "an Access Violation";
    exceptions[5].code = EXCEPTION_IN_PAGE_ERROR;
    exceptions[5].name = "an In Page Error";
    exceptions[6].code = STATUS_NO_MEMORY;
    exceptions[6].name = "a No Memory";
    exceptions[7].code = EXCEPTION_ILLEGAL_INSTRUCTION;
    exceptions[7].name = "an Illegal Instruction";
    exceptions[8].code = EXCEPTION_NONCONTINUABLE_EXCEPTION;
    exceptions[8].name = "a Noncontinuable Exception";
    exceptions[9].code = EXCEPTION_INVALID_DISPOSITION;
    exceptions[9].name = "an Invalid Disposition";
    exceptions[10].code = EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
    exceptions[10].name = "a Array Bounds Exceeded";
    exceptions[11].code = EXCEPTION_FLT_DENORMAL_OPERAND;
    exceptions[11].name = "a Float Denormal Operand";
    exceptions[12].code = EXCEPTION_FLT_DIVIDE_BY_ZERO;
    exceptions[12].name = "a Float Divide by Zero";
    exceptions[13].code = EXCEPTION_FLT_INEXACT_RESULT;
    exceptions[13].name = "a Float Inexact Result";
    exceptions[14].code = EXCEPTION_FLT_INVALID_OPERATION;
    exceptions[14].name = "a Float Invalid Operation";
    exceptions[15].code = EXCEPTION_FLT_OVERFLOW;
    exceptions[15].name = "a Float Overflow";
    exceptions[16].code = EXCEPTION_FLT_STACK_CHECK;
    exceptions[16].name = "a Float Stack Check";
    exceptions[17].code = EXCEPTION_FLT_UNDERFLOW;
    exceptions[17].name = "a Float Underflow";
    exceptions[18].code = EXCEPTION_INT_DIVIDE_BY_ZERO;
    exceptions[18].name = "an Integer Divide by Zero";
    exceptions[19].code = EXCEPTION_INT_OVERFLOW;
    exceptions[19].name = "an Integer Overflow";
    exceptions[20].code = EXCEPTION_PRIV_INSTRUCTION;
    exceptions[20].name = "a Privileged Instruction";
    exceptions[21].code = EXCEPTION_STACK_OVERFLOW;
    exceptions[21].name = "a Stack Overflow";
    exceptions[22].code = STATUS_DLL_INIT_FAILED;
    exceptions[22].name = "a DLL Initialization Failed";
    exceptions[23].code = 0xE06D7363;
    exceptions[23].name = "a Microsoft C++ Exception";

    for (int i = 0; i < 24; ++i)
    {
        if (exceptionCode == exceptions[i].code)
        {
            return exceptions[i].name;
        }
    }

    return "Unknown exception type";
}
