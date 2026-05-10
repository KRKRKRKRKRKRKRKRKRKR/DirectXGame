#pragma once
#include "../Window/Window.h"
#include "../Graphics/DirectXManager.h"

#include "../Graphics/Renderer/Primitive/PrimitiveRenderer.h"
#include "../Math/MathTypes.h"

#include "../Camera/Camera.h"
#include "../Externals/imgui/imguiManager.h"

class Engine {
public:
	Engine() = default;
	~Engine() = default;

	void Initialize(const std::wstring& windowTitle,int width,int height);

	void Run();

	void Finalize();

private:
	void Update();

	void Render();

	Window window_;
	DirectXManager directX_;
	PrimitiveRenderer primitiveRenderer_;
	Camera camera_;
	ImGuiManager imgui_;

	Transform transform_;
};