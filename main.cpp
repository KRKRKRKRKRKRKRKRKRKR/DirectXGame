#include "pch.h"
#include "Window/WindowManager.h"
#include "Graphics/DirectXManager.h"
#include "Debug/DebugManager.h"
#include "Utils/Logger.h"
#include "Renderer/PrimitiveRenderer.h"

std::wstring KwindowTitle = L"DirectXGame";

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	DebugManager::RegisterCrashHandler();

	Logger::Initialize();

	WindowManager window;
	window.Create(KwindowTitle, 1280, 720);

	DebugManager::EnableDebugLayer();

	DirectXManager directX;
	directX.Initialize(window.GetHWND(), window.GetClientWidth(), window.GetClientHeight());

	PrimitiveRenderer primitiveRenderer;
	primitiveRenderer.Initialize(&directX);
	

	while (window.ProcessMessage()) {
		directX.beginFrame();

		//描画処理はここから
		primitiveRenderer.DrawTriangleRender(&directX, window.GetClientWidth(), window.GetClientHeight(), directX.GetCommandList());

		directX.endFrame();
	}

	primitiveRenderer.Finalize();
	directX.Finalize();
	DebugManager::SetupInfoQueue(directX.GetDevice());
	DebugManager::ReportLiveObjects();

	return 0;
}