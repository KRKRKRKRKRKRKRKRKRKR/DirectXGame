#pragma once
#include "../Engine/Graphics/Renderer/Renderer.h"
#include "../Engine/Graphics/Pipeline/BlendMode.h"
#include "../Engine/Camera/Camera.h"
#include "../Engine/Audio/Sound.h"
#include "../Math/MathTypes.h"
#include <vector>
#include <string>
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
	float deltaTime_ = 0.0f;

	Transform sphere;
	Transform cube;
	Transform triangle;
	Transform sprite3D;
	Transform sprite2D;
	Transform floor_;

	UVTransform sprite2DUV;
	UVTransform sprite3DUV;

	Vector4 sphereColor   = { 1,1,1,1 };
	Vector4 cubeColor     = { 1,1,1,1 };
	Vector4 triangleColor = { 1,1,1,1 };
	Vector4 sprite3DColor = { 1,1,1,1 };
	Vector4 sprite2DColor = { 1,1,1,1 };
	Vector4 floorColor_   = { 1,1,1,1 };

	Sound bgm;

	Renderer::ModelHandle modelHandle_ = 0;
	Transform             modelTransform_;
	Vector4               modelColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool                  modelLighting_ = true;
	int                   modelTexIndex_ = 0;

	bool sphereLighting   = true;
	bool cubeLighting     = true;
	bool triangleLighting = true;
	bool sprite3DLighting = true;
	bool sprite2DLighting = false;
	bool floorLighting_   = true;

	std::vector<Transform> gridCubes_;

	struct TextureEntry {
		TextureHandle handle;
		std::string   name;
	};
	std::vector<TextureEntry> textures_;
	int sprite2DTexIndex_  = 0;
	int sprite3DTexIndex_  = 0;
	int triangleTexIndex_  = 0;
	int cubeTexIndex_      = 0;
	int sphereTexIndex_    = 0;
	int gridCubeTexIndex_  = 0;
	int floorTexIndex_     = 0;

	Vector4 gridCubeColor_    = { 1,1,1,1 };
	bool    gridCubeLighting_ = true;
	Vector3 gridCubeRotation_ = { 0.0f, 0.0f, 0.0f };

	float triangleSmoothness_ = 1.0f;
	float cubeSmoothness_     = 1.0f;

	BlendMode gridCubeBlendMode_ = BlendMode::kNone;
	BlendMode triangleBlendMode_ = BlendMode::kNone;
	BlendMode cubeBlendMode_     = BlendMode::kNone;
	BlendMode sphereBlendMode_   = BlendMode::kNone;
	BlendMode modelBlendMode_    = BlendMode::kNone;
	BlendMode sprite3DBlendMode_ = BlendMode::kNone;
	BlendMode sprite2DBlendMode_ = BlendMode::kNone;
	BlendMode floorBlendMode_    = BlendMode::kNone;

	// OMSetBlendFactorに渡す0〜1の強さ。kNormal/kAdd/kSubtractのみ効果がある
	float gridCubeBlendStrength_ = 1.0f;
	float triangleBlendStrength_ = 1.0f;
	float cubeBlendStrength_     = 1.0f;
	float sphereBlendStrength_   = 1.0f;
	float modelBlendStrength_    = 1.0f;
	float sprite3DBlendStrength_ = 1.0f;
	float sprite2DBlendStrength_ = 1.0f;
	float floorBlendStrength_    = 1.0f;

	void DrawGrid();
	void DrawImGui();
};
