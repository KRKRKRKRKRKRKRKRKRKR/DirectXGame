#include "Window/WindowManager.h"
#include "Graphics/DirectXManager.h"
#include "Debug/CrashHandler.h"

int WINAPI WinMain(_In_ HINSTANCE,_In_opt_ HINSTANCE,_In_ LPSTR,_In_ int) {
	CrashHandler::Register();

	WindowManager window;
	window.Create();

	DirectXManager dx;
	dx.Initialize();

	while (window.ProcessMessage()) {
		// ゲームの処理
	}

	return 0;
}