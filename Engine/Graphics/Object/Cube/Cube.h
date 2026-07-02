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

class Cube : public IDrawable {
public:
	Cube() = default;
	virtual ~Cube();

	// 負荷テスト用に大きめの上限を確保（131072=2^17、グリッド最大約362×362相当）
	static constexpr uint32_t kMaxInstanceCount = 131072;

	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, Pipeline* pipeline,
		DescriptorHeaps* heaps);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index);
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index);
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

	InstancedWvpColorBuffer wvpColorBuffer_;

	ID3D12RootSignature* rootSignature_ = nullptr;
	Pipeline* pipeline_ = nullptr;
	TextureManager* textureManager_ = nullptr;

	float smoothness_ = 1.0f;

	void CreateVertexResource(ID3D12Device* device);
	void WriteVertexData();
};
