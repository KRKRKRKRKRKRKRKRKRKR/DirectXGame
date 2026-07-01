#pragma once
#include <Windows.h>
#include <dbghelp.h>
#include <strsafe.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <wrl/client.h>
#pragma comment(lib, "dbghelp.lib")

class Debug {
	public:
	Debug() = delete;

	// 例外フィルタを登録する	
	static void RegisterCrashHandler();

	// デバッグレイヤーを有効にする
	static void EnableDebugLayer();

	//InfoQueueを取得する
	static void SetupInfoQueue(ID3D12Device* dx);

	// ライブオブジェクトをレポートする
	static void ReportLiveObjects();

private:
	// クラッシュ時に呼ばれるコールバック
	static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception);
};

