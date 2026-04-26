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

	DirectXManager dx;
	dx.Initialize(window.GetHWND(), window.GetClientWidth(), window.GetClientHeight());

	PrimitiveRenderer primitiveRenderer;
	primitiveRenderer.Initialize(&dx);

	while (window.ProcessMessage()) {
		dx.beginFrame();

		//描画処理はここから
		primitiveRenderer.DrawTriangleRender(&dx, window.GetClientWidth(), window.GetClientHeight(), dx.GetCommandList());

		dx.endFrame();
	}

	DebugManager::ReportLiveObjects();

	return 0;
}