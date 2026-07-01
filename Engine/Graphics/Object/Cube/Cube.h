#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"

using Microsoft::WRL::ComPtr;

class Cube : public IDrawable {
public:
	Cube() = default;
	virtual ~Cube();

	static constexpr uint32_t kMaxInstanceCount = 4096;

	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
		DescriptorHeaps* heaps);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);
	void SetColor(const Vector4& color, uint32_t index);
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
		TextureManager* textureManager, TextureHandle texture);

	void Draw(ID3D12GraphicsCommandList* commandList,
		uint32_t instanceCount, uint32_t startInstance = 0) override;

	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override { return pipelineState_; }

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
	ID3D12PipelineState* pipelineState_ = nullptr;
	TextureManager* textureManager_ = nullptr;

	void CreateVertexResource(ID3D12Device* device);
	void WriteVertexData();
	void CreateWvpResource(ID3D12Device* device);
	void CreateColorResource(ID3D12Device* device);
};
