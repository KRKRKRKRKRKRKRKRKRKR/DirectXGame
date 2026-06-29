#pragma once
#include "../Engine/Graphics/Renderer/Renderer.h"
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

	UVTransform sprite2DUV;
	UVTransform sprite3DUV;

	Vector4 sphereColor   = { 1,1,1,1 };
	Vector4 cubeColor     = { 1,1,1,1 };
	Vector4 triangleColor = { 1,1,1,1 };
	Vector4 sprite3DColor = { 1,1,1,1 };
	Vector4 sprite2DColor = { 1,1,1,1 };

	Sound bgm;

	bool sphereLighting   = true;
	bool cubeLighting     = true;
	bool triangleLighting = true;
	bool sprite3DLighting = true;
	bool sprite2DLighting = false;

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

	Vector4 gridCubeColor_   = { 1,1,1,1 };
	bool    gridCubeLighting_ = true;

	void DrawGrid();
	void DrawImGui();
};
