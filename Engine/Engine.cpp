#include "Engine.h"
#include "../Externals/imgui/imgui.h"
#include "../Debug/Debug.h"
#include "Utils/Logger.h"

void Engine::Initialize(const std::wstring& windowTitle, int width, int height) {
	Debug::RegisterCrashHandler();
	Logger::Initialize();
	window_.Create(windowTitle, width, height);
	Debug::EnableDebugLayer();
	directX_.Initialize(window_.GetHWND(), window_.GetClientWidth(), window_.GetClientHeight());
	InputDevice::GetInstance().Initialize(window_.GetInstance(), window_.GetHWND());
	Debug::SetupInfoQueue(directX_.GetDevice());
	camera_.Initialize({0.0f, 0.5f, -5.0f});
	imgui_.Initialize(window_.GetHWND(), &directX_);
	game_.Initialize(&directX_, &camera_);
	deltaTime_.Start();
}

void Engine::Run() {
	while (window_.ProcessMessage()) {
		InputDevice::GetInstance().Update();
		deltaTime_.Update();
		directX_.BeginFrame();
		imgui_.BeginFrame();
		game_.Update(deltaTime_.GetDeltaTime());
		game_.Render();
		imgui_.EndFrame(&directX_);
		directX_.EndFrame();
	}
}

void Engine::Finalize() {
	imgui_.Finalize();
	InputDevice::GetInstance().Finalize();
	directX_.Finalize();
}

