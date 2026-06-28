#pragma once

#include <d3d12.h>
#include <memory>
#include "../ShaderCompiler/ShaderCompiler.h"
#include "../Pipeline/Pipeline.h"
#include "../Pipeline/LinePipeline.h"
#include "../Texture/TextureManager.h"
#include "../Object/Triangle/Triangle.h"
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

	void DrawTriangle(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, const Vector4& color, TextureID textureID);
	void DrawLine(const Matrix4x4& view, const Matrix4x4& projection, const Vector3& start, const Vector3& end, const Vector4& color);
	void DrawGridBatch(const Matrix4x4& view, const Matrix4x4& projection);
	void DrawSprite(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform);
	void DrawSphere(const Matrix4x4& view, const Matrix4x4& projection, const Transform& transform, TextureID textureID);

	void InitializeGridLines();
	void ResetFrameIndex();

	TextureManager* GetTextureManager() { return &textureManager_; }
	Triangle* GetTriangle() { return triangle_.get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_; }
	int GetClientWidth() const { return windowWidth_; }
	int GetClientHeight() const { return windowHeight_; }

private:
	ID3D12Device* device_ = nullptr;
	ID3D12GraphicsCommandList* commandList_ = nullptr;

	ShaderCompiler shaderCompiler_;
	Pipeline pipeline_;
	LinePipeline linePipeline_;
	TextureManager textureManager_;

	std::unique_ptr<Triangle> triangle_;
	std::unique_ptr<Line> line_;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Sphere> sphere_;

	uint32_t currentTriangleIndex_ = 0;
	uint32_t currentLineIndex_ = 0;
	int windowWidth_ = 0;
	int windowHeight_ = 0;
};
