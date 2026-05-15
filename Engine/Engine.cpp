#include "Engine.h"
#include "../Externals/imgui/imgui.h"
#include "../Debug/Debug.h"
#include "../Utils/Logger.h"

void Engine::Initialize(const std::wstring& windowTitle, int width, int height) {
	Debug::RegisterCrashHandler();
	Logger::Initialize();

	window_.Create(windowTitle, width, height);

	Debug::EnableDebugLayer();

	directX_.Initialize(window_.GetHWND(), window_.GetClientWidth(), window_.GetClientHeight());
  Debug::SetupInfoQueue(directX_.GetDevice());
	
	
	camera_.Initialize();
	
	imgui_.Initialize(window_.GetHWND(), &directX_);

	transform_.scale = { 1.0f, 1.0f, 1.0f };
	transform_.rotation = { 0.0f, 0.0f, 0.0f };
	transform_.translation = { 0.0f, 0.0f, 0.0f };
}

void Engine::Run() {
	while (window_.ProcessMessage()) {
		Update();
		Render();
	}
}

void Engine::Finalize() {
	imgui_.Finalize();
	directX_.Finalize();
}

void Engine::Update() {
	transform_.rotation.y += 0.01f;
}

void Engine::Render() {
	directX_.BeginFrame();
	imgui_.BeginFrame();

	float aspectRatio = camera_.GetAspeRatio(window_.GetClientWidth(), window_.GetClientHeight());
	Matrix4x4 viewMatrix = camera_.GetViewMatrix();
	Matrix4x4 projectionMatrix = camera_.GetProjectionMatrix(aspectRatio);

	directX_.DrawTriangleRender(viewMatrix, projectionMatrix, transform_);
	ImGui::ShowDemoWindow();
	
	imgui_.EndFrame(&directX_);
	directX_.EndFrame();
}