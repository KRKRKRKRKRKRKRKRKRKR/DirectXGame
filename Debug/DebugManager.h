#pragma once
#include <Windows.h>
#include <dbghelp.h>
#include <strsafe.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#pragma comment(lib, "dbghelp.lib")

class DebugManager {
	public:
	DebugManager() = delete;

	// 例外フィルタを登録する	
	static void RegisterCrashHandler();

	// デバッグレイヤーを有効にする
	static void EnableDebugLayer();

	static void ReportLiveObjects();

private:
	// クラッシュ時に呼ばれるコールバック
	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
};

