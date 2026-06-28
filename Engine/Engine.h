#pragma once
#include "Window/Window.h"
#include "Graphics/Renderer/DirectXManager.h"
#include "Graphics/Renderer/Renderer.h"
#include "../Math/MathTypes.h"
#include "Camera/Camera.h"
#include "../Externals/imgui/imguiManager.h"
#include "InputDevice/InputDevice.h"
#include "Utils/DeltaTime.h"
#include "../Game/Game.h"

class Engine {
public:
	Engine() = default;
	~Engine() = default;

	void Initialize(const std::wstring& windowTitle, int width, int height);
	void Run();
	void Finalize();

private:
	Window window_;
	DirectXManager directX_;
	Renderer renderer_;
	Camera camera_;
	ImGuiManager imgui_;
	DeltaTime deltaTime_;
	Game game_;
};