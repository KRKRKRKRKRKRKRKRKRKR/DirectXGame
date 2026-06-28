#pragma once
#include "../Engine/Graphics/Renderer/DirectXManager.h"
#include "../Engine/Camera/Camera.h"
#include "../Particle/TrailParticle3D.h"
#include "../Math/MathTypes.h"

class Game {
public:
	Game() = default;
	~Game() = default;

	void Initialize(DirectXManager* directX, Camera* camera);
	void Update(float deltaTime);
	void Render();

private:
	DirectXManager* directX_ = nullptr;
	Camera* camera_ = nullptr;

	Transform transform1_;
	Transform transform2_;

	static constexpr int kMaxTriangles = 20;
	Transform triangleTransforms_[kMaxTriangles];
	TrailParticle3D trailParticles_[kMaxTriangles];
	TrailParticleParameter trailParam_;
	TextureID textureID_ = TextureID::None;

	void DrawGrid();
	void DrawImGui();
};
