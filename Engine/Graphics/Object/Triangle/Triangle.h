#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../ResourceFactory/InstancedWvpColorBuffer.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"
using Microsoft::WRL::ComPtr;

class Pipeline;

// 三角形描画オブジェクト
class Triangle : public IDrawable {
public:
	Triangle() = default;
	virtual ~Triangle();

	static constexpr uint32_t kMaxInstanceCount = 4096;

	// 初期化（DirectXManager から呼び出し）
	void Initialize(ID3D12Device* device, TextureManager* textureManager, ID3D12RootSignature* rootSignature, Pipeline* pipeline, DescriptorHeaps* heaps);

	// ワールド・ビュー・プロジェクション行列を設定
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t wvpIndex);
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t wvpIndex);

	// 0=フラット, 1=スムース のブレンド率。変更時に頂点バッファを書き直す
	void SetSmoothness(float s);

	// パイプラインコマンドを設定
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	void SetColor(const Vector4& color, uint32_t materialIndex);

	// IDrawable 実装
	void Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance = 0) override;

	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override;

private:

	// 頂点バッファ関連
	ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	InstancedWvpColorBuffer wvpColorBuffer_;

	float smoothness_ = 1.0f;

	// パイプライン参照
	ID3D12RootSignature* rootSignature_ = nullptr;
	Pipeline* pipeline_ = nullptr;

	// テクスチャ マネージャー参照
	TextureManager* textureManager_ = nullptr;

	// 描画定数
	static constexpr int kVertexCount = 12;

	// 内部初期化関数
	void CreateVertexResource(ID3D12Device* device);
	void WriteVertexData();
};