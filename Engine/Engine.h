#pragma once
#include "Window/Window.h"
#include "Graphics/Renderer/DirectXManager.h"
#include "Graphics/Renderer/Renderer.h"
#include "../Math/MathTypes.h"
#include "Camera/Camera.h"
#include "../Externals/imgui/imguiManager.h"
#include "InputDevice/InputDevice.h"
#include "Utils/DeltaTime.h"

class Engine {
public:
	Engine() = default;
	~Engine() = default;

	void Initialize(const std::wstring& windowTitle, int width, int height);
	bool Update();  // ウィンドウメッセージ処理・BeginFrame・ImGui開始。falseで終了
	void Flush();   // FlushTriangles・FlushLines・FlushSpheres・ImGui終了・EndFrame
	void Finalize();

	Renderer* GetRenderer() { return &renderer_; }
	Camera*   GetCamera()   { return &camera_; }
	float     GetDeltaTime() { return deltaTime_.GetDeltaTime(); }

private:
	Window window_;
	DirectXManager directX_;
	Renderer renderer_;
	Camera camera_;
	ImGuiManager imgui_;
	DeltaTime deltaTime_;
};