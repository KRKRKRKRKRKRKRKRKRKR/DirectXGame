#include "CrashHandler.h"

void CrashHandler::Register() {
    SetUnhandledExceptionFilter(ExportDump);
}

LONG WINAPI CrashHandler::ExportDump(EXCEPTION_POINTERS* exception) {
    SYSTEMTIME time;
    GetLocalTime(&time);

    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH,
        L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay,
        time.wHour, time.wMinute);

    HANDLE dumpFileHandle = CreateFile(
        filePath,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ,
        0, CREATE_ALWAYS, 0, 0);

    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    MINIDUMP_EXCEPTION_INFORMATION minidumpInfo{ 0 };
    minidumpInfo.ThreadId = threadId;
    minidumpInfo.ExceptionPointers = exception;
    minidumpInfo.ClientPointers = TRUE;

    MiniDumpWriteDump(
        GetCurrentProcess(), processId, dumpFileHandle,
        MiniDumpWithFullMemory, &minidumpInfo, nullptr, nullptr);

    return EXCEPTION_EXECUTE_HANDLER;
}