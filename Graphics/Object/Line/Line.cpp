#include "Line.h"
#include "../../Texture/TextureManager.h"
#include "../../../Utils/Logger.h"

#include <cassert>
#include <cstring>

namespace {
	constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
		return (value + (alignment - 1)) & ~(alignment - 1);
	}
}

// ============================================================
// デストラクタ
// ============================================================

Line::~Line() {
	// 頂点バッファはマップしたまま保持しているので先に解除
	if (vertexResource_) {
		vertexResource_->Unmap(0, nullptr);
		vertexMappedData_ = nullptr;
		vertexResource_.Reset();
	}

	if (wvpResource_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
		wvpResource_.Reset();
	}

	materialResource_.Reset();
}

// ============================================================
// 初期化
// ============================================================

void Line::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState) {
	if (!device || !textureManager || !rootSignature || !pipelineState) {
		Logger::Log("Line::Initialize : Invalid parameters\n");
		assert(false);
		return;
	}

	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipelineState_ = pipelineState;

	CreateVertexResource(device);
	CreateMaterialResource(device);
	CreateWvpMatrixResource(device);

	Logger::Log("Line initialized successfully\n");
}

// ============================================================
// リソース作成
// ============================================================

void Line::CreateVertexResource(ID3D12Device* device) {
	// インスタンス数 × 1ライン2頂点 分をまとめて確保し、マップしたままにする
	const size_t bufferSize = sizeof(VertexData) * kVerticesPerLine * kMaxInstanceCount;
	vertexResource_ = textureManager_->CreateBufferResource(bufferSize);

	if (!vertexResource_) {
		Logger::Log("Line::CreateVertexResource : Failed to create vertex buffer\n");
		assert(false);
		return;
	}

	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexMappedData_));
	if (FAILED(hr) || !vertexMappedData_) {
		Logger::Log("Line::CreateVertexResource : Failed to map vertex buffer\n");
		assert(false);
		return;
	}

	// 頂点バッファビュー
	// StrideInBytes は1頂点のサイズ、SizeInBytes はバッファ全体
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Line::CreateMaterialResource(ID3D12Device* device) {
	materialResource_ = textureManager_->CreateBufferResource(sizeof(Vector4));

	if (!materialResource_) {
		Logger::Log("Line::CreateMaterialResource : Failed to create material buffer\n");
		assert(false);
		return;
	}

	Vector4* materialData = nullptr;
	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	if (FAILED(hr) || !materialData) {
		Logger::Log("Line::CreateMaterialResource : Failed to map material buffer\n");
		assert(false);
		return;
	}

	// デフォルト: 白色
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);
}

void Line::CreateWvpMatrixResource(ID3D12Device* device) {
	wvpStride_ = AlignUp(static_cast<uint32_t>(sizeof(Matrix4x4)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	wvpResource_ = textureManager_->CreateBufferResource(static_cast<size_t>(wvpStride_) * kMaxInstanceCount);

	if (!wvpResource_) {
		Logger::Log("Line::CreateWvpMatrixResource : Failed to create WVP buffer\n");
		assert(false);
		return;
	}

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	if (FAILED(hr) || !wvpMappedData_) {
		Logger::Log("Line::CreateWvpMatrixResource : Failed to map WVP buffer\n");
		assert(false);
		return;
	}
}

// ============================================================
// セッター
// ============================================================

void Line::SetLine(const Vector3& start, const Vector3& end, uint32_t lineIndex) {
	if (!vertexMappedData_) {
		Logger::Log("Line::SetLine : Vertex resource is not initialized\n");
		return;
	}
	if (lineIndex >= kMaxInstanceCount) {
		Logger::Log("Line::SetLine : lineIndex exceeds kMaxInstanceCount\n");
		assert(false);
		return;
	}

	// lineIndex 番目のライン = 頂点[lineIndex*2], [lineIndex*2+1]
	VertexData* v = vertexMappedData_ + lineIndex * kVerticesPerLine;
	v[0].position = Vector4(start.x, start.y, start.z, 1.0f);
	v[1].position = Vector4(end.x, end.y, end.z, 1.0f);
}

void Line::SetColor(const Vector4& color) {
	if (!materialResource_) {
		Logger::Log("Line::SetColor : Material resource is not initialized\n");
		return;
	}

	Vector4* materialData = nullptr;
	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	if (FAILED(hr) || !materialData) {
		Logger::Log("Line::SetColor : Failed to map material buffer\n");
		return;
	}

	*materialData = color;
	materialResource_->Unmap(0, nullptr);
}

void Line::SetWvpMatrix(const Matrix4x4& wvpMatrix, uint32_t wvpIndex) {
	if (!wvpMappedData_ || !wvpResource_) {
		Logger::Log("Line::SetWvpMatrix : WVP resource is not initialized\n");
		return;
	}

	char* destination = reinterpret_cast<char*>(wvpMappedData_) + wvpIndex * wvpStride_;
	std::memcpy(destination, &wvpMatrix, sizeof(Matrix4x4));
}

void Line::SetViewportAndScissorRect(int32_t width, int32_t height) {
	viewport_.Width = static_cast<float>(width);
	viewport_.Height = static_cast<float>(height);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
	scissorRect_.left = 0;
	scissorRect_.right = width;
	scissorRect_.top = 0;
	scissorRect_.bottom = height;
}

// ============================================================
// コマンド発行
// ============================================================

void Line::SetPipelineCommands(ID3D12GraphicsCommandList* commandList) {
	if (!commandList) {
		Logger::Log("Line::SetPipelineCommands : Invalid parameters\n");
		return;
	}

	commandList->RSSetViewports(1, &viewport_);
	commandList->RSSetScissorRects(1, &scissorRect_);
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipelineState_);

}

void Line::Draw(ID3D12GraphicsCommandList* commandList, uint32_t wvpIndex) {
	if (!commandList) {
		Logger::Log("Line::Draw : Invalid command list\n");
		return;
	}

	// lineIndex に対応する頂点オフセットから2頂点を描画
	const uint32_t startVertex = wvpIndex * kVerticesPerLine;

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

	// マテリアル（色）をセット（ルートパラメータ0）
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// WVP 行列をセット（ルートパラメータ1）
	D3D12_GPU_VIRTUAL_ADDRESS wvpAddress = wvpResource_->GetGPUVirtualAddress() + (wvpStride_ * wvpIndex);
	commandList->SetGraphicsRootConstantBufferView(1, wvpAddress);

	// 2頂点、startVertex から描画
	commandList->DrawInstanced(kVerticesPerLine, 1, startVertex, 0);
}