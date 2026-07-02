#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../Pipeline/BlendMode.h"
using Microsoft::WRL::ComPtr;

class TextureManager;
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

	// 0=フラット, 1=スムース のブレンド率。変更時に頂点バッファを書き直す
	void SetSmoothness(float s);

	// ビューポート・シザーレクトを設定
	void SetViewportAndScissorRect(int32_t width, int32_t height);

	// パイプラインコマンドを設定
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	void SetColor(const Vector4& color, uint32_t materialIndex);

	// IDrawable 実装
	void Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance = 0);

	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override;
	// ゲッター
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return vertexBufferView_; }
	ComPtr<ID3D12Resource> GetVertexResource() const { return vertexResource_; }
	const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
	const D3D12_RECT& GetScissorRect() const { return scissorRect_; }

private:

	// 頂点バッファ関連
	ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// マテリアル・テクスチャ関連
	ComPtr<ID3D12Resource> materialResource_ = nullptr;
	uint32_t materialStride_ = 0;
	uint8_t* materialMappedData_ = nullptr;

	// SRV用ヒープ（WVP・色それぞれ）
	ComPtr<ID3D12DescriptorHeap> srvHeap_;
	D3D12_GPU_DESCRIPTOR_HANDLE wvpSrvHandle_{};
	D3D12_GPU_DESCRIPTOR_HANDLE colorSrvHandle_{};

	// WVP 行列関連
	ComPtr<ID3D12Resource> wvpResource_ = nullptr;
	uint8_t* wvpMappedData_ = nullptr;
	uint32_t wvpStride_ = 0;

	float smoothness_ = 1.0f;

	// ビューポート・シザーレクト
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};

	// パイプライン参照
	ID3D12RootSignature* rootSignature_ = nullptr;
	Pipeline* pipeline_ = nullptr;

	// テクスチャ マネージャー参照
	TextureManager* textureManager_ = nullptr;

	// 描画定数
	static constexpr int kVertexCount = 12;

	// 内部初期化関数
	void CreateVertexResource(ID3D12Device* device);
	void CreateMaterialResource(ID3D12Device* device);
	void CreateWvpMatrixResource(ID3D12Device* device);
	void CreateSRVHeap(ID3D12Device* device);
	void WriteVertexData();
};