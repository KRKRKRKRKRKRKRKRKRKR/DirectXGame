#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../ResourceFactory/InstancedWvpColorBuffer.h"
#include "../../Pipeline/BlendMode.h"
using Microsoft::WRL::ComPtr;

class Pipeline;

class Sphere : public IDrawable {
public:
	Sphere() = default;
	virtual ~Sphere();

	static constexpr uint32_t kMaxInstanceCount = 4096;

	// 頂点バッファは kMaxSubdivision 分のサイズで確保される（分割数変更時の再確保を避けるため）
	static constexpr uint32_t kMaxSubdivision = 30;

	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, Pipeline* pipeline,
		DescriptorHeaps* heaps,
		uint32_t subdivision = kMaxSubdivision, float radius = 1.0f);

	// 分割数を変更（1〜kMaxSubdivision）。頂点データを再計算してMap/Unmapで書き直す
	void SetSubdivision(uint32_t subdivision);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index);
	void SetColor(const Vector4& color, uint32_t index);
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
		TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	// IDrawable 実装
	void Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance = 0) override;
	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override;

private:
	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	InstancedWvpColorBuffer wvpColorBuffer_;

	uint32_t vertexCount_ = 0;
	float    radius_      = 1.0f; // SetSubdivisionで頂点データを再計算する際に使う

	ID3D12RootSignature* rootSignature_ = nullptr;
	Pipeline* pipeline_ = nullptr;
	TextureManager* textureManager_ = nullptr;

	void CreateVertexResource(ID3D12Device* device, uint32_t subdivision, float radius);
	void WriteVertexData(uint32_t subdivision, float radius);
};
