#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"

using Microsoft::WRL::ComPtr;

class TextureManager;

// ライン描画オブジェクト（始点・終点の2点指定）
class Line : public IDrawable {
public:
	Line() = default;
	virtual ~Line();

	// 初期化（DirectXManager から呼び出し）
	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);

	// ワールド・ビュー・プロジェクション行列を設定
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, uint32_t wvpIndex);

	// ライン始点・終点を設定
	void SetLine(const Vector3& start, const Vector3& end, uint32_t lineIndex);

	// ライン色を設定（RGBA, 0.0f〜1.0f）
	void SetColor(const Vector4& color);

	// ビューポート・シザーレクトを設定
	void SetViewportAndScissorRect(int32_t width, int32_t height);

	// パイプラインコマンドを設定
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList);

	// IDrawable 実装
	void Draw(ID3D12GraphicsCommandList* commandList, uint32_t wvpIndex) override;
	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override { return pipelineState_; }

	// ゲッター
	const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
	const D3D12_RECT& GetScissorRect() const { return scissorRect_; }
	
	// 内部描画関数（複数ラインをまとめて描画）
	void DrawBatch(ID3D12GraphicsCommandList* commandList,uint32_t startVertex,uint32_t lineCount,uint32_t wvpIndex);
private:
	// 頂点データ（位置のみ、テクスチャなし）
	struct VertexData {
		Vector4 position;
	};

	// 頂点バッファ関連
	ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	VertexData* vertexMappedData_ = nullptr;

	// マテリアル（色）関連
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

	// テクスチャマネージャー参照
	TextureManager* textureManager_ = nullptr;

	// 描画定数
	// 1ライン = 頂点2つ、最大ライン数分まとめて確保
	static constexpr uint32_t kVerticesPerLine = 2;
	const uint32_t kMaxInstanceCount = 1024;

	// 内部初期化関数
	void CreateVertexResource(ID3D12Device* device);
	void CreateMaterialResource(ID3D12Device* device);
	void CreateWvpMatrixResource(ID3D12Device* device);
};