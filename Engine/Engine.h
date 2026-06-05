#pragma once
#include "../Window/Window.h"
#include "../Graphics/DirectXManager.h"
#include "../Math/MathTypes.h"
#include "../Camera/Camera.h"
#include "../Externals/imgui/imguiManager.h"
#include "../InputDevice/InputDevice.h"
#include "../TrailParticle3D.h"

#include <random>
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
	Camera camera_;
	ImGuiManager imgui_;

	Transform transform1_;
	Transform transform3_;

	CameraData cameraData_;

	void CameraControl();
	void DrawGrid();

	static constexpr int kMaxTriangles = 10;
	Transform triangleTransforms_[kMaxTriangles];
	TrailParticle3D trailParticles_[kMaxTriangles];
	void DrawImGui();
	TrailParticleParameter trailParam_; 
};