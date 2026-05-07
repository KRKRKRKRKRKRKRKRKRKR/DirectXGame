#include "Engine/Engine.h"

const std::wstring KwindowTitle = L"DirectXGame";
const int KwindowWidth = 1280;
const int KwindowHeight = 720;

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	Engine engine;
	engine.Initialize(KwindowTitle, KwindowWidth, KwindowHeight);
	engine.Run();
	engine.Finalize();

	return 0;
}