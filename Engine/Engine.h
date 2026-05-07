#pragma once
#include "../Window/WindowManager.h"
#include "../Graphics/DirectXManager.h"
#include "../Debug/DebugManager.h"
#include "../Utils/Logger.h"
#include "../Renderer/PrimitiveRenderer.h"
#include "../Math/MathTypes.h"
#include "../Math/TransformMath.h"
#include "../Math/MatrixMath.h"
#include "../Camera/CameraManager.h"
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

	WindowManager window_;
	DirectXManager directX_;
	PrimitiveRenderer primitiveRenderer_;
	CameraManager camera_;
	ImGuiManager imgui_;

	Transform transform_;
};