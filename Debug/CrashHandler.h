#pragma once
#include <Windows.h>
#include <dbghelp.h>
#include <strsafe.h>

#pragma comment(lib, "dbghelp.lib")

class CrashHandler {
public:

    CrashHandler() = delete;

    // 例外フィルタを登録する
    static void Register();

private:
    // クラッシュ時に呼ばれるコールバック
    static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
};