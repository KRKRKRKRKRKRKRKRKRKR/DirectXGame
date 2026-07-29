#include "Sprite.h"
#include "../../../../Math/MatrixMath.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../Pipeline/Pipeline.h"
#include "../../Pipeline/PipelineCommandHelper.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cmath>

void Sprite::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, Pipeline* pipeline,
	DescriptorHeaps* heaps, uint32_t wvpHeapIndex, uint32_t colorHeapIndex) {
	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipeline_ = pipeline;

	CreateVertexResource(device);
	CreateIndexResource(device);
	WriteVertexData();

	wvpColorBuffer_.Initialize(device, heaps, kMaxInstanceCount, wvpHeapIndex, colorHeapIndex);
}

void Sprite::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, index);
}

void Sprite::SetColor(const Vector4& color, uint32_t index) {
	wvpColorBuffer_.SetColor(color, index);
}

void Sprite::SetUVTransform(const UVTransform& uvTransform, uint32_t index) {
	if (!vertexMappedData_ || index >= kMaxInstanceCount) return;

	// 基準UV（頂点順: 左下, 右下, 左上, 右上）
	// flipV_=false→3D（Y-up）、true→2D（Y-down スクリーン座標）
	constexpr float baseU[kVertexCount]   = { 0.0f, 1.0f, 0.0f, 1.0f };
	constexpr float baseV3D[kVertexCount] = { 1.0f, 1.0f, 0.0f, 0.0f };
	constexpr float baseV2D[kVertexCount] = { 0.0f, 0.0f, 1.0f, 1.0f };
	const float* baseV = flipV_ ? baseV2D : baseV3D;

	float c = cosf(uvTransform.rotation);
	float s = sinf(uvTransform.rotation);

	VertexData* slot = vertexMappedData_ + static_cast<size_t>(index) * kVertexCount;
	for (int i = 0; i < kVertexCount; ++i) {
		// 中心を原点に移動してスケール
		float u = (baseU[i] - 0.5f) * uvTransform.scale.x;
		float v = (baseV[i] - 0.5f) * uvTransform.scale.y;

		// 回転
		float ru = u * c - v * s;
		float rv = u * s + v * c;

		// 中心に戻してオフセット加算
		slot[i].texcoord.x = ru + 0.5f + uvTransform.offset.x;
		slot[i].texcoord.y = rv + 0.5f + uvTransform.offset.y;
	}
}

void Sprite::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	PipelineCommandHelper::ApplyCommon(commandList, rootSignature_, pipeline_->GetPipelineState(blendMode, enableAlphaTest),
		textureManager->GetSrvGpuHandle(texture), wvpColorBuffer_.GetWvpSrvHandle(), wvpColorBuffer_.GetColorSrvHandle(),
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);
}

ID3D12PipelineState* Sprite::GetPipelineState() const {
	return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Sprite::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// BaseVertexLocationでstartInstance番目の4頂点スロットを指す（indexバッファ自体は
	// 常に0,1,2,2,1,3の相対値のまま。インスタンスごとにUVが焼き込まれた別スロットを読ませる）
	commandList->DrawIndexedInstanced(kIndexCount, instanceCount, 0, static_cast<INT>(startInstance) * kVertexCount, startInstance);
}

void Sprite::CreateVertexResource(ID3D12Device* device) {
	vertexResource_ = ResourceFactory::CreateBufferResource(device, sizeof(VertexData) * kVertexCount * kMaxInstanceCount);
	vertexResource_->SetName(L"Sprite Vertex Buffer");
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes    = static_cast<UINT>(sizeof(VertexData) * kVertexCount * kMaxInstanceCount);
	vertexBufferView_.StrideInBytes  = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexMappedData_));
}

void Sprite::CreateIndexResource(ID3D12Device* device) {
	indexResource_ = ResourceFactory::CreateBufferResource(device, sizeof(uint16_t) * kIndexCount);
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes    = sizeof(uint16_t) * kIndexCount;
	indexBufferView_.Format         = DXGI_FORMAT_R16_UINT;

	uint16_t* indexData = nullptr;
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
	// 三角形1: 左下→右下→左上、三角形2: 左上→右下→右上
	indexData[0] = 0; indexData[1] = 1; indexData[2] = 2;
	indexData[3] = 2; indexData[4] = 1; indexData[5] = 3;
	indexResource_->Unmap(0, nullptr);
}

void Sprite::WriteVertexData() {
	if (!vertexMappedData_) return;
	// flipV_=true（2D）: Y-down スクリーン座標系に合わせて V を反転
	float vTop    = flipV_ ? 0.0f : 1.0f;
	float vBottom = flipV_ ? 1.0f : 0.0f;
	// 頂点順: 左下[0], 右下[1], 左上[2], 右上[3]
	// インデックス[0,1,2, 2,1,3]で三角形2枚を構成。位置/法線は全インスタンススロットで共通のため、
	// kMaxInstanceCount枚分すべてに同じ基本形状を複製しておく（UVはSetUVTransformが後から個別に上書きする）
	for (uint32_t slot = 0; slot < kMaxInstanceCount; ++slot) {
		VertexData* v = vertexMappedData_ + static_cast<size_t>(slot) * kVertexCount;
		v[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, vTop   }, {0.0f, 0.0f, -1.0f} };
		v[1] = { { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, vTop   }, {0.0f, 0.0f, -1.0f} };
		v[2] = { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, vBottom}, {0.0f, 0.0f, -1.0f} };
		v[3] = { { 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, vBottom}, {0.0f, 0.0f, -1.0f} };
	}
}

void Sprite::SetFlipV(bool flip) {
	flipV_ = flip;
	WriteVertexData();
}
