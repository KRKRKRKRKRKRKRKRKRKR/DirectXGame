#include "Debug.h"
void Debug::RegisterCrashHandler() {
	SetUnhandledExceptionFilter(ExportDump);
}

LONG WINAPI Debug::ExportDump(EXCEPTION_POINTERS* exception) {
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

void Debug::EnableDebugLayer() {
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {

		// デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();

		//GPUチェックを有効にする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif
}

void Debug::SetupInfoQueue(ID3D12Device* dx) {
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(dx->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// D3D12の警告をすべて表示する
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
			// プロセス終了時にランタイムが残存オブジェクトを報告するSTATE_CREATIONカテゴリの警告。
			// アプリのバグではなく多くのD3D12アプリで見られる終了時特有の情報だが、
			// SetBreakOnSeverity(WARNING, true)によりDebugBreak()を誘発し、デバッガ未アタッチ時に
			// 未処理例外として扱われてしまうため、このIDだけ除外する
			D3D12_MESSAGE_ID_LIVE_OBJECT_SUMMARY,
			D3D12_MESSAGE_ID_LIVE_DEVICE,
		};

		// NumSeverities/pSeverityListとNumIDs/pIDListはOR条件（いずれかにマッチすれば破棄）。
		// severitiesにWARNINGを加えるとID関係なく全WARNINGが握りつぶされてしまうため、
		// ここはINFOのみのままにし、上のdenyIdsで個別のID指定だけを効かせる
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
	}
#endif
}

void Debug::ReportLiveObjects() {
	Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_DETAIL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_DETAIL);
	}
}