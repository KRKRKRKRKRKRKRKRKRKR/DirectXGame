#include "Engine.h"
#include "../Debug/Debug.h"
#include "Utils/Logger.h"

void Engine::Initialize(const std::wstring& windowTitle, int width, int height) {
	Debug::RegisterCrashHandler();
	Logger::Initialize();
	window_.Create(windowTitle, width, height);
	Debug::EnableDebugLayer();
	directX_.Initialize(window_.GetHWND(), window_.GetClientWidth(), window_.GetClientHeight());
	renderer_.Initialize(directX_.GetDevice(), directX_.GetCommandList(), directX_.GetDescriptorHeaps(), window_.GetClientWidth(), window_.GetClientHeight());
	directX_.WaitForGPUCompletion();
	renderer_.SetCommandList(directX_.GetCommandList());
	InputDevice::GetInstance().Initialize(window_.GetInstance(), window_.GetHWND());
	Debug::SetupInfoQueue(directX_.GetDevice());
	camera_.Initialize({0.0f, 0.5f, -5.0f});
	imgui_.Initialize(window_.GetHWND(), &directX_);
	AudioManager::GetInstance().Initialize();
	deltaTime_.Start();

	window_.SetResizeCallback([this](int32_t width, int32_t height) {
		directX_.Resize(width, height);
		renderer_.Resize(width, height);
	});
}

bool Engine::Update() {
	if (!window_.ProcessMessage()) return false;
	InputDevice::GetInstance().Update();
	deltaTime_.Update();
	directX_.BeginFrame();
	renderer_.ResetFrameIndex();
	renderer_.SetCommandList(directX_.GetCommandList());
	renderer_.SetBackBufferTarget(directX_.GetRTVHandle(), directX_.GetDSVHandle());
	imgui_.BeginFrame();
	return true;
}

void Engine::Flush() {
	renderer_.FlushAll();
	imgui_.EndFrame(&directX_);
	directX_.EndFrame();
}

void Engine::Finalize() {
	// GPUが直前フレームのコマンド（Present等）を実行完了する前に、
	// ImGuiやRendererがGPUリソース（フォントテクスチャ等）を解放してしまわないよう、
	// 他の終了処理より先にGPU完了を待つ（STATE_CREATION WARNING対策）
	directX_.WaitForGPUCompletion();

	AudioManager::GetInstance().Finalize();
	imgui_.Finalize();
	renderer_.Finalize();
	InputDevice::GetInstance().Finalize();
	directX_.Finalize();
}

