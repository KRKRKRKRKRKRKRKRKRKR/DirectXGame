#pragma once

#include <d3d12.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <string>
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../Pipeline/Pipeline.h"
#include "../Pipeline/LinePipeline.h"
#include "../Texture/TextureManager.h"
#include "../Object/Triangle/Triangle.h"
#include "../Object/Cube/Cube.h"
#include "../Object/Line/Line.h"
#include "../Object/Sprite/Sprite.h"
#include "../Object/Sphere/Sphere.h"
#include "../Object/Model/Model.h"
#include "../Lighting/DirectionalLight.h"
#include "../../../Math/MathTypes.h"
#include "../../../Math/TransformMath.h"

class DescriptorHeaps;

class Renderer {
public:
	Renderer() = default;
	~Renderer() = default;

	void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, DescriptorHeaps* heaps, int width, int height);
	void Finalize();

	void SetCommandList(ID3D12GraphicsCommandList* commandList) { commandList_ = commandList; }

	// テクスチャをファイルから読み込んでハンドルを返す（同じパスは二重ロードしない）
	TextureHandle LoadTexture(const std::string& filePath);

	// OBJ を読み込んでハンドルを返す
	using ModelHandle = uint32_t;
	ModelHandle LoadModel(const std::string& directoryPath, const std::string& filename);

	void DrawModel(ModelHandle handle, const Transform& t, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true);
	void FlushModels();

	// フレーム先頭で一度だけ呼ぶ。以降の Draw(Transform,...) はこの行列を使う
	void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
		view_ = view; projection_ = projection;
	}

	// Transform 版（内部で world * view * proj を計算）
	void DrawTriangle(const Transform& t, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true);
	void DrawSphere  (const Transform& t, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true);
	void DrawCube    (const Transform& t, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true);

	// WVP 直接指定版（既存、後方互換用）
	void DrawTriangle(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true);
	void DrawSphere  (const Matrix4x4& wvp, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true);
	void DrawCube    (const Matrix4x4& wvp, const Vector4& color, TextureHandle texture = kTextureNone, bool useLighting = true);

	void FlushTriangles();
	void FlushSpheres();
	void FlushCubes();

	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const Matrix4x4& view, const Matrix4x4& projection);
	void FlushLines();
	void DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection);
	// 3Dスプライト（カメラ付き WVP）
	void DrawSprite3D(const Transform& transform, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = true, const UVTransform& uvTransform = {});
	// 2DスプライトUI（ピクセル座標、奥行きなし）。FlushSprites2D() で最後に描画される
	void DrawSprite2D(const Transform& transform, const Vector4& color = { 1,1,1,1 }, TextureHandle texture = kTextureNone, bool useLighting = false, const UVTransform& uvTransform = {});
	void FlushSprites2D();

	DirectionalLight& GetLight() { return light_; }

	void SetTriangleSmoothness(float s) { triangle_->SetSmoothness(s); }
	void SetCubeSmoothness(float s)     { cube_->SetSmoothness(s); }

	void InitializeGridLines();
	void ResetFrameIndex();

	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_; }
	int GetClientWidth() const { return windowWidth_; }
	int GetClientHeight() const { return windowHeight_; }

private:
	struct TriangleCommand {
		Matrix4x4     wvp;
		Matrix4x4     world;
		Vector4       color;
		TextureHandle texture;
		bool          useLighting;
	};
	struct LineCommand {
		Vector3   start;
		Vector3   end;
		Vector4   color;
		Matrix4x4 viewProj;
	};
	struct SphereCommand {
		Matrix4x4     wvp;
		Matrix4x4     world;
		Vector4       color;
		TextureHandle texture;
		bool          useLighting;
	};
	struct CubeCommand {
		Matrix4x4     wvp;
		Matrix4x4     world;
		Vector4       color;
		TextureHandle texture;
		bool          useLighting;
	};
	struct Sprite2DCommand {
		Matrix4x4     wvp;
		Matrix4x4     world;
		Vector4       color;
		TextureHandle texture;
		bool          useLighting;
		UVTransform   uvTransform;
	};
	struct ModelCommand {
		ModelHandle   handle;
		Matrix4x4     wvp;
		Matrix4x4     world;
		Vector4       color;
		TextureHandle texture;
		bool          useLighting;
	};

	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;

	ShaderCompiler shaderCompiler_;
	Pipeline pipeline_;
	Pipeline spritePipeline2D_;
	LinePipeline linePipeline_;
	TextureManager textureManager_;

	std::unique_ptr<Triangle> triangle_;
	std::unique_ptr<Cube>     cube_;
	std::unique_ptr<Line>     line_;
	std::unique_ptr<Sprite>   sprite3D_;
	std::unique_ptr<Sprite>   sprite2D_;
	std::unique_ptr<Sphere>   sphere_;
	DirectionalLight light_;

	std::vector<std::unique_ptr<Model>> models_;
	uint32_t nextModelHeapIndex_ = 20; // 0-19 は他オブジェクトが使用

	std::vector<TriangleCommand>  triangleCommands_;
	std::vector<CubeCommand>      cubeCommands_;
	std::vector<LineCommand>      lineCommands_;
	std::vector<SphereCommand>    sphereCommands_;
	std::vector<Sprite2DCommand>  sprite2DCommands_;
	std::vector<ModelCommand>     modelCommands_;

	DescriptorHeaps* heaps_ = nullptr;
	uint32_t currentLineIndex_ = 0;
	int windowWidth_ = 0;
	int windowHeight_ = 0;

	Matrix4x4 view_{};
	Matrix4x4 projection_{};
};
