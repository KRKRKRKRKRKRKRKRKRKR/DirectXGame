#include "Window/WindowManager.h"
#include "Graphics/DirectXManager.h"
#include "Debug/CrashHandler.h"
#include "Utils/Logger.h"


int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	CrashHandler::Register();

	Logger::Initialize();

	WindowManager window;
	window.Create();

	DirectXManager dx;
	dx.Initialize();
	
	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	while (window.ProcessMessage()) {
		// ゲームの処理
	}

	return 0;
}