#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../Math/MathTypes.h"

using Microsoft::WRL::ComPtr;

class TextureManager;

// 三角形描画オブジェクト
class Triangle : public IDrawable {
public:
	Triangle() = default;
	virtual ~Triangle();

	// 初期化（DirectXManager から呼び出し）
	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);

	// ワールド・ビュー・プロジェクション行列を設定
	void SetWvpMatrix(const Matrix4x4& wvpMatrix,uint32_t wvpIndex);

	// ビューポート・シザーレクトを設定
	void SetViewportAndScissorRect(int32_t width, int32_t height);

	// パイプラインコマンドを設定
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager);

	// IDrawable 実装
	void Draw(ID3D12GraphicsCommandList* commandList,uint32_t wvpIndex) ;
	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override { return pipelineState_; }

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


	// WVP 行列関連
	ComPtr<ID3D12Resource> wvpResource_ = nullptr;
	uint8_t* wvpMappedData_ = nullptr;
	uint32_t wvpStride_ = 0;

	// ビューポート・シザーレクト
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT scissorRect_{};

	// パイプライン参照
	ID3D12RootSignature* rootSignature_ = nullptr;
	ID3D12PipelineState* pipelineState_ = nullptr;

	// テクスチャ マネージャー参照
	TextureManager* textureManager_ = nullptr;

	// 描画定数
	const uint32_t kMaxInstanceCount = 1024;

	const int kVertexCount = 12;

	// 内部初期化関数
	void CreateVertexResource(ID3D12Device* device);
	void CreateMaterialResource(ID3D12Device* device);
	void CreateWvpMatrixResource(ID3D12Device* device);
	void WriteVertexData();
};