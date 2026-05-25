#include "TriangleBatch.h"
#include "../DirectXGame/TextureManager.h"
#include "../DirectXGame/Utils/Logger.h"
#include "../DirectXGame/Utils/StringUtils.h"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <format>
namespace {
	constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
		return (value + (alignment - 1)) & ~(alignment - 1);
	}
}

TriangleBatch::~TriangleBatch() {
	// リソース解放
	if (wvpResource_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
		wvpResource_.Reset();
	}

	vertexResource_.Reset();
	materialResource_.Reset();
}

void TriangleBatch::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
	uint32_t maxInstanceCount) {
	if (!device || !textureManager || !rootSignature || !pipelineState) {
		Logger::Log("TriangleBatch::Initialize : Invalid parameters\n");
		assert(false);
		return;
	}

	if (maxInstanceCount == 0 || maxInstanceCount > 10000) {
		Logger::Log("TriangleBatch::Initialize : Invalid maxInstanceCount (0-10000)\n");
		assert(false);
		return;
	}

	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipelineState_ = pipelineState;
	maxInstanceCount_ = maxInstanceCount;
	instanceCount_ = 0;

	// リソース作成
	CreateVertexResource(device);
	CreateMaterialResource(device);
	CreateWvpMatrixPool(device);

	// 頂点データ書き込み
	WriteVertexData();

	// 行列プール初期化
	instanceWvpMatrices_.resize(maxInstanceCount_);
	std::fill(instanceWvpMatrices_.begin(), instanceWvpMatrices_.end(), MatrixMath::Identity());

	Logger::Log(std::format("TriangleBatch initialized with max {} instances\n", maxInstanceCount_));
}

void TriangleBatch::CreateVertexResource(ID3D12Device* device) {
	// 三角形は 6頂点（共有）
	vertexResource_ = textureManager_->CreateBufferResource(sizeof(VertexData) * 6);

	if (!vertexResource_) {
		Logger::Log("TriangleBatch::CreateVertexResource : Failed to create vertex buffer\n");
		assert(false);
		return;
	}

	// 頂点バッファビュー設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void TriangleBatch::CreateMaterialResource(ID3D12Device* device) {
	materialResource_ = textureManager_->CreateBufferResource(sizeof(Vector4));

	if (!materialResource_) {
		Logger::Log("TriangleBatch::CreateMaterialResource : Failed to create material buffer\n");
		assert(false);
		return;
	}

	Vector4* materialData = nullptr;
	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	if (FAILED(hr) || !materialData) {
		Logger::Log("TriangleBatch::CreateMaterialResource : Failed to map material buffer\n");
		assert(false);
		return;
	}

	// デフォルト: 白色
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);
}

void TriangleBatch::CreateWvpMatrixPool(ID3D12Device* device) {
	// WVP 行列プール（最大インスタンス数分）
	wvpStride_ = AlignUp(static_cast<uint32_t>(sizeof(Matrix4x4)),
		D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	size_t poolSize = static_cast<size_t>(wvpStride_) * maxInstanceCount_;

	wvpResource_ = textureManager_->CreateBufferResource(poolSize);

	if (!wvpResource_) {
		Logger::Log("TriangleBatch::CreateWvpMatrixPool : Failed to create WVP pool\n");
		assert(false);
		return;
	}

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	if (FAILED(hr) || !wvpMappedData_) {
		Logger::Log("TriangleBatch::CreateWvpMatrixPool : Failed to map WVP pool\n");
		assert(false);
		return;
	}

	// 初期値: Identity 行列
	for (uint32_t i = 0; i < maxInstanceCount_; ++i) {
		std::memcpy(wvpMappedData_ + static_cast<size_t>(i) * wvpStride_,
			&MatrixMath::Identity(), sizeof(Matrix4x4));
	}
}

void TriangleBatch::WriteVertexData() {
	if (!vertexResource_) {
		Logger::Log("TriangleBatch::WriteVertexData : Vertex resource is not initialized\n");
		return;
	}

	VertexData* vertexData = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	if (FAILED(hr) || !vertexData) {
		Logger::Log("TriangleBatch::WriteVertexData : Failed to map vertex buffer\n");
		assert(false);
		return;
	}

	// 三角形の頂点データ（2つの三角形で四角形）
	vertexData[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[0].texcoord = Vector2(0.0f, 1.0f);

	vertexData[1].position = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
	vertexData[1].texcoord = Vector2(0.5f, 0.0f);

	vertexData[2].position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[2].texcoord = Vector2(1.0f, 1.0f);

	vertexData[3].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[3].texcoord = Vector2(0.0f, 1.0f);

	vertexData[4].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	vertexData[4].texcoord = Vector2(0.5f, 0.0f);

	vertexData[5].position = Vector4(0.5f, -0.5f, -0.5f, 1.0f);
	vertexData[5].texcoord = Vector2(1.0f, 1.0f);

	vertexResource_->Unmap(0, nullptr);
}

uint32_t TriangleBatch::AddInstance(const Matrix4x4& wvpMatrix) {
	if (instanceCount_ >= maxInstanceCount_) {
		Logger::Log("TriangleBatch::AddInstance : Batch is full\n");
		return UINT32_MAX;
	}

	uint32_t index = instanceCount_++;
	instanceWvpMatrices_[index] = wvpMatrix;

	// GPU メモリに即座に反映
	std::memcpy(wvpMappedData_ + static_cast<size_t>(index) * wvpStride_,
		&wvpMatrix, sizeof(Matrix4x4));

	return index;
}

void TriangleBatch::UpdateInstance(uint32_t instanceIndex, const Matrix4x4& wvpMatrix) {
	if (instanceIndex >= instanceCount_) {
		Logger::Log("TriangleBatch::UpdateInstance : Invalid instance index\n");
		return;
	}

	instanceWvpMatrices_[instanceIndex] = wvpMatrix;
	std::memcpy(wvpMappedData_ + static_cast<size_t>(instanceIndex) * wvpStride_,
		&wvpMatrix, sizeof(Matrix4x4));
}

void TriangleBatch::ClearInstances() {
	instanceCount_ = 0;
	std::fill(instanceWvpMatrices_.begin(), instanceWvpMatrices_.end(), MatrixMath::Identity());
}

D3D12_GPU_VIRTUAL_ADDRESS TriangleBatch::GetWvpGpuAddress(uint32_t instanceIndex) const {
	if (!wvpResource_ || instanceIndex >= maxInstanceCount_) {
		return 0;
	}
	return wvpResource_->GetGPUVirtualAddress() + static_cast<UINT64>(instanceIndex) * wvpStride_;
}

void TriangleBatch::Draw(ID3D12GraphicsCommandList* commandList) {
	if (!commandList || instanceCount_ == 0) {
		return;
	}

	// 頂点バッファを設定（共有）
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// プリミティブトポロジーを設定
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// マテリアルバッファをセット（ルートパラメータ0）
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// ★ ここが重要: DrawInstanced で複数インスタンスを一括描画
	// VertexCountPerInstance = 6（三角形の頂点数）
	// InstanceCount = instanceCount_（登録されたインスタンス数）
	// → GPU は SV_InstanceID を使って各インスタンスの WVP 行列を参照
	commandList->DrawInstanced(6, instanceCount_, 0, 0);
}