#pragma once
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Camera/Camera.h"
#include "../Particle/TrailParticle3D.h"
#include "../Math/MathTypes.h"
class Game {
public:
	Game() = default;
	~Game() = default;

	void Initialize(Renderer* renderer, Camera* camera);
	void Update(float deltaTime);
	void Render();

private:
	Renderer* renderer_ = nullptr;
	Camera* camera_ = nullptr;

	Transform transform1_;
	Transform transform2_;

	static constexpr int kMaxTriangles = 20;
	Transform triangleTransforms_[kMaxTriangles];
	TrailParticle3D trailParticles_[kMaxTriangles];
	TrailParticleParameter trailParam_;

	TextureHandle textureID_ = kTextureNone;
	TextureHandle texHandles_[2] = {};

	void DrawGrid();
	void DrawImGui();
};
