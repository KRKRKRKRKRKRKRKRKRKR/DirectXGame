#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"

using Microsoft::WRL::ComPtr;

class Pipeline;

class Cube : public IDrawable {
public:
	Cube() = default;
	virtual ~Cube();

	static constexpr uint32_t kMaxInstanceCount = 4096;

	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, Pipeline* pipeline,
		DescriptorHeaps* heaps);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);
	void SetColor(const Vector4& color, uint32_t index);

	// 0=フラット, 1=スムース のブレンド率。変更時に頂点バッファを書き直す
	void SetSmoothness(float s);
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
		TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	void Draw(ID3D12GraphicsCommandList* commandList,
		uint32_t instanceCount, uint32_t startInstance = 0) override;

	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override;

private:
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// 6面 × 2三角形 × 3頂点
	static constexpr uint32_t kVertexCount = 36;

	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	ComPtr<ID3D12Resource> wvpResource_;
	uint8_t* wvpMappedData_ = nullptr;
	uint32_t wvpStride_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle_{};

	ComPtr<ID3D12Resource> colorResource_;
	uint8_t* colorMappedData_ = nullptr;
	uint32_t colorStride_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};

	ID3D12RootSignature* rootSignature_ = nullptr;
	Pipeline* pipeline_ = nullptr;
	TextureManager* textureManager_ = nullptr;

	float smoothness_ = 1.0f;

	void CreateVertexResource(ID3D12Device* device);
	void WriteVertexData();
	void CreateWvpResource(ID3D12Device* device);
	void CreateColorResource(ID3D12Device* device);
};
