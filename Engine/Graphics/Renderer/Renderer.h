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

	void DrawTriangle(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture = kTextureNone);
	void FlushTriangles();
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color, const Matrix4x4& view, const Matrix4x4& projection);
	void FlushLines();
	void DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection);
	void DrawSprite(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void DrawSphere(const Matrix4x4& wvp, TextureHandle texture = kTextureNone);
	void FlushSpheres();
	void DrawCube(const Matrix4x4& wvp, const Vector4& color, TextureHandle texture = kTextureNone);
	void FlushCubes();

	void InitializeGridLines();
	void ResetFrameIndex();

	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_; }
	int GetClientWidth() const { return windowWidth_; }
	int GetClientHeight() const { return windowHeight_; }

private:
	struct TriangleCommand {
		Matrix4x4     wvp;
		Vector4       color;
		TextureHandle texture;
	};
	struct LineCommand {
		Vector3   start;
		Vector3   end;
		Vector4   color;
		Matrix4x4 viewProj;
	};
	struct SphereCommand {
		Matrix4x4     wvp;
		TextureHandle texture;
	};
	struct CubeCommand {
		Matrix4x4     wvp;
		Vector4       color;
		TextureHandle texture;
	};

	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;

	ShaderCompiler shaderCompiler_;
	Pipeline pipeline_;
	LinePipeline linePipeline_;
	TextureManager textureManager_;

	std::unique_ptr<Triangle> triangle_;
	std::unique_ptr<Cube>     cube_;
	std::unique_ptr<Line>     line_;
	std::unique_ptr<Sprite>   sprite_;
	std::unique_ptr<Sphere>   sphere_;

	std::vector<TriangleCommand> triangleCommands_;
	std::vector<CubeCommand>     cubeCommands_;
	std::vector<LineCommand>     lineCommands_;
	std::vector<SphereCommand>   sphereCommands_;

	DescriptorHeaps* heaps_ = nullptr;
	uint32_t currentLineIndex_ = 0;
	int windowWidth_ = 0;
	int windowHeight_ = 0;
};
