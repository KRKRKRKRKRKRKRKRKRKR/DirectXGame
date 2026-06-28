#include "Engine/Engine.h"
#include "Game/Game.h"

const std::wstring KwindowTitle = L"DirectXGame";
const int KwindowWidth = 1280;
const int KwindowHeight = 720;

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	Engine engine;
	Game   game;

	engine.Initialize(KwindowTitle, KwindowWidth, KwindowHeight);
	game.Initialize(engine.GetRenderer(), engine.GetCamera());

	while (engine.Update()) {
		game.Update(engine.GetDeltaTime());
		game.Render();
		engine.Flush();
	}

	engine.Finalize();

	return 0;
}