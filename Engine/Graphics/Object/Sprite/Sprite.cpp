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
	CreateWvpResource(device);
	CreateColorResource(device);

	auto wvpSrv   = heaps->CreateStructuredBufferSRV(device, wvpResource_.Get(),   1, sizeof(TransformationMatrix), wvpHeapIndex);
	auto colorSrv = heaps->CreateStructuredBufferSRV(device, colorResource_.Get(), 1, sizeof(Vector4),   colorHeapIndex);
	wvpSrvHandle_   = wvpSrv.gpuHandle;
	colorSrvHandle_ = colorSrv.gpuHandle;
}

void Sprite::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world) {
	if (wvpMappedData_) {
		wvpMappedData_->WVP   = wvpMatrix;
		wvpMappedData_->World = world;
		wvpMappedData_->WorldInverseTranspose = MatrixMath::Transpose(MatrixMath::Inverse(world));
	}
}

void Sprite::SetColor(const Vector4& color) {
	if (colorMappedData_) *colorMappedData_ = color;
}

void Sprite::SetUVTransform(const UVTransform& uvTransform) {
	if (!vertexMappedData_) return;

	// 基準UV（頂点順: 左下, 右下, 左上, 右上）
	// flipV_=false→3D（Y-up）、true→2D（Y-down スクリーン座標）
	constexpr float baseU[kVertexCount]   = { 0.0f, 1.0f, 0.0f, 1.0f };
	constexpr float baseV3D[kVertexCount] = { 1.0f, 1.0f, 0.0f, 0.0f };
	constexpr float baseV2D[kVertexCount] = { 0.0f, 0.0f, 1.0f, 1.0f };
	const float* baseV = flipV_ ? baseV2D : baseV3D;

	float c = cosf(uvTransform.rotation);
	float s = sinf(uvTransform.rotation);

	for (int i = 0; i < kVertexCount; ++i) {
		// 中心を原点に移動してスケール
		float u = (baseU[i] - 0.5f) * uvTransform.scale.x;
		float v = (baseV[i] - 0.5f) * uvTransform.scale.y;

		// 回転
		float ru = u * c - v * s;
		float rv = u * s + v * c;

		// 中心に戻してオフセット加算
		vertexMappedData_[i].texcoord.x = ru + 0.5f + uvTransform.offset.x;
		vertexMappedData_[i].texcoord.y = rv + 0.5f + uvTransform.offset.y;
	}
}

void Sprite::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	PipelineCommandHelper::ApplyCommon(commandList, rootSignature_, pipeline_->GetPipelineState(blendMode, enableAlphaTest),
		textureManager->GetSrvGpuHandle(texture), wvpSrvHandle_, colorSrvHandle_,
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);
}

ID3D12PipelineState* Sprite::GetPipelineState() const {
	return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Sprite::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
	(void)instanceCount; (void)startInstance;
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawIndexedInstanced(kIndexCount, 1, 0, 0, 0);
}

void Sprite::CreateVertexResource(ID3D12Device* device) {
	vertexResource_ = ResourceFactory::CreateBufferResource(device, sizeof(VertexData) * kVertexCount);
	vertexResource_->SetName(L"Sprite Vertex Buffer");
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes    = static_cast<UINT>(sizeof(VertexData) * kVertexCount);
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
	// インデックス[0,1,2, 2,1,3]で三角形2枚を構成
	vertexMappedData_[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, vTop   }, {0.0f, 0.0f, -1.0f} };
	vertexMappedData_[1] = { { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, vTop   }, {0.0f, 0.0f, -1.0f} };
	vertexMappedData_[2] = { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, vBottom}, {0.0f, 0.0f, -1.0f} };
	vertexMappedData_[3] = { { 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, vBottom}, {0.0f, 0.0f, -1.0f} };
}

void Sprite::SetFlipV(bool flip) {
	flipV_ = flip;
	WriteVertexData();
}

void Sprite::CreateWvpResource(ID3D12Device* device) {
	wvpResource_ = ResourceFactory::CreateBufferResource(device, sizeof(TransformationMatrix));
	assert(wvpResource_);
	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);
	wvpMappedData_->WVP   = MatrixMath::Identity();
	wvpMappedData_->World = MatrixMath::Identity();
}

void Sprite::CreateColorResource(ID3D12Device* device) {
	colorResource_ = ResourceFactory::CreateBufferResource(device, sizeof(Vector4));
	assert(colorResource_);
	HRESULT hr = colorResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorMappedData_));
	assert(SUCCEEDED(hr) && colorMappedData_);
	*colorMappedData_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}
