#pragma once
#include <windows.h>
#include <dxgidebug.h>
#include <dbghelp.h>
#include <dxgi1_6.h>
#include <strsafe.h>
#include <d3d12sdklayers.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dbghelp.lib")
class DebugManager {
	public:
	DebugManager() = delete;
	~DebugManager();

	// 例外フィルタを登録する	
	static void RegisterCrashHandler();

	// デバッグレイヤーを有効にする
	static void EnableDebugLayer();

	static void ReportLiveObjects();

private:
	// クラッシュ時に呼ばれるコールバック
	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
};

