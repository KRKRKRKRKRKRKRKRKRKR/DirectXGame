#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <vector>
#include "IDrawable.h"
#include "../DirectXGame/Math/MathTypes.h"
#include "../DirectXGame/Math/MatrixMath.h"
using Microsoft::WRL::ComPtr;

class TextureManager;

// 複数の三角形インスタンスを効率的に描画
// DrawInstanced で複数の三角形を 1回の GPU draw call で描画
class TriangleBatch : public IDrawable {
public:
	TriangleBatch() = default;
	virtual ~TriangleBatch();

	// 初期化（最大インスタンス数を指定）
	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
		uint32_t maxInstanceCount = 100);

	// インスタンス追加（WVP行列プールへのインデックスを返す）
	uint32_t AddInstance(const Matrix4x4& wvpMatrix);

	// インスタンスの WVP 行列を更新
	void UpdateInstance(uint32_t instanceIndex, const Matrix4x4& wvpMatrix);

	// すべてのインスタンス描画情報をリセット
	void ClearInstances();

	// IDrawable 実装
	// → 注: TriangleBatch は複数インスタンスを管理するため、
	//   単一の WVP インデックスは返さない（カスタム実装で対応）
	void Draw(ID3D12GraphicsCommandList* commandList) override;
	uint32_t GetWvpIndex() const override { return 0; }
	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override { return pipelineState_; }

	// ゲッター
	ComPtr<ID3D12Resource> GetVertexResource() const { return vertexResource_; }
	uint32_t GetInstanceCount() const { return instanceCount_; }
	uint32_t GetMaxInstanceCount() const { return maxInstanceCount_; }

	// WVP 行列プール関連
	ComPtr<ID3D12Resource> GetWvpResource() const { return wvpResource_; }
	uint32_t GetWvpStride() const { return wvpStride_; }
	D3D12_GPU_VIRTUAL_ADDRESS GetWvpGpuAddress(uint32_t instanceIndex) const;

private:
	// 頂点バッファ（共有：三角形 6頂点 のみ）
	ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	// マテリアル（共有：すべてのインスタンスで同じ）
	ComPtr<ID3D12Resource> materialResource_ = nullptr;

	// WVP 行列プール（複数インスタンス分）
	ComPtr<ID3D12Resource> wvpResource_ = nullptr;
	uint8_t* wvpMappedData_ = nullptr;
	uint32_t wvpStride_ = 0;
	uint32_t maxInstanceCount_ = 0;
	uint32_t instanceCount_ = 0;
	std::vector<Matrix4x4> instanceWvpMatrices_;

	// パイプライン参照
	ID3D12RootSignature* rootSignature_ = nullptr;
	ID3D12PipelineState* pipelineState_ = nullptr;

	// テクスチャ マネージャー参照
	TextureManager* textureManager_ = nullptr;

	// 内部初期化関数
	void CreateVertexResource(ID3D12Device* device);
	void CreateMaterialResource(ID3D12Device* device);
	void CreateWvpMatrixPool(ID3D12Device* device);
	void WriteVertexData();
};