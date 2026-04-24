#include "DebugManager.h"

DebugManager::~DebugManager() { 

}

void DebugManager::RegisterCrashHandler() {
	SetUnhandledExceptionFilter(ExportDump);
}

LONG WINAPI DebugManager::ExportDump(EXCEPTION_POINTERS* exception) {
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

void DebugManager::EnableDebugLayer() {
#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {

		// デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();

		//GPUチェックを有効にする
		debugController->SetEnableGPUBasedValidation(TRUE);

		debugController->Release();
	}

	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		// D3D12の警告をすべて表示する
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};

		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
		infoQueue->Release();
	}
#endif
}