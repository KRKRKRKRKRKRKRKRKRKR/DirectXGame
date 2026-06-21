#pragma once
#include "../Window/Window.h"
#include "../Graphics/DirectXManager.h"
#include "../Math/MathTypes.h"
#include "../Camera/Camera.h"
#include "../Externals/imgui/imguiManager.h"
#include "../InputDevice/InputDevice.h"
#include "../TrailParticle3D.h"
#include "../Utils/DeltaTime.h"

#include <random>
class Engine {
public:
	Engine() = default;
	~Engine() = default;

	void Initialize(const std::wstring& windowTitle,int width,int height);

	void Run();

	void Finalize();

private:
	void Update(float deltaTime);

	void Render();

	Window window_;
	DirectXManager directX_;
	Camera camera_;
	ImGuiManager imgui_;

	Transform transform1_;
	Transform transform2_;

	TextureID textureID_ = TextureID::None;
	void DrawGrid();

	static constexpr int kMaxTriangles = 20;
	Transform triangleTransforms_[kMaxTriangles];
	TrailParticle3D trailParticles_[kMaxTriangles];
	void DrawImGui();
	TrailParticleParameter trailParam_; 
	DeltaTime deltaTime_;
};