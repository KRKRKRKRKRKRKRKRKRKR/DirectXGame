#include "Triangle.h"
#include "../../Texture/TextureManager.h"
#include "../../../Utils/Logger.h"


#include <cassert>
#include <cstring>

namespace {
	constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
		return (value + (alignment - 1)) & ~(alignment - 1);
	}
}

Triangle::~Triangle() {
	if (wvpResource_ && wvpMappedData_) {  // wvpMappedData_ チェック追加
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
		wvpResource_.Reset();
	}

	if (materialResource_ && materialMappedData_) {  // materialMappedData_ チェック追加
		materialResource_->Unmap(0, nullptr);
		materialMappedData_ = nullptr;
		materialResource_.Reset();
	}

	vertexResource_.Reset();
}

void Triangle::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState) {
	if (!device || !textureManager || !rootSignature || !pipelineState) {
		Logger::Log("Triangle::Initialize : Invalid parameters\n");
		assert(false);
		return;
	}

	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipelineState_ = pipelineState;

	// リソース作成
	CreateVertexResource(device);
	CreateMaterialResource(device);
	CreateWvpMatrixResource(device);

	// 頂点データ書き込み
	WriteVertexData();

	Logger::Log("Triangle initialized successfully\n");
}

void Triangle::CreateVertexResource(ID3D12Device* device) {

	vertexResource_ = textureManager_->CreateBufferResource(sizeof(VertexData) * kVertexCount);

	if (!vertexResource_) {
		Logger::Log("Triangle::CreateVertexResource : Failed to create vertex buffer\n");
		assert(false);
		return;
	}

	// 頂点バッファビュー設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Triangle::CreateMaterialResource(ID3D12Device* device) {
	materialStride_ = AlignUp(static_cast<uint32_t>(sizeof(Vector4)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	materialResource_ = textureManager_->CreateBufferResource(static_cast<size_t>(materialStride_) * kMaxInstanceCount);

	if (!materialResource_) {
		Logger::Log("Triangle::CreateMaterialResource : Failed to create material buffer\n");
		assert(false);
		return;
	}

	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialMappedData_));
	if (FAILED(hr) || !materialMappedData_) {
		Logger::Log("Triangle::CreateMaterialResource : Failed to map material buffer\n");
		assert(false);
		return;
	}

	// 全インスタンスを白で初期化
	for (uint32_t i = 0; i < kMaxInstanceCount; i++) {
		Vector4* data = reinterpret_cast<Vector4*>(materialMappedData_ + i * materialStride_);
		*data = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void Triangle::CreateWvpMatrixResource(ID3D12Device* device) {

	wvpStride_ = AlignUp(static_cast<uint32_t>(sizeof(Matrix4x4)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	wvpResource_ = textureManager_->CreateBufferResource(static_cast<size_t>(wvpStride_) * kMaxInstanceCount);

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);

}

//正四面体を作る
void Triangle::WriteVertexData() {
	if (!vertexResource_) {
		Logger::Log("Triangle::WriteVertexData : Vertex resource is not initialized\n");
		return;
	}

	VertexData* vertexData = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	if (FAILED(hr) || !vertexData) {
		Logger::Log("Triangle::WriteVertexData : Failed to map vertex buffer\n");
		assert(false);
		return;
	}

	// 正四面体の4つの頂点座標を定義
	Vector4 posA = Vector4(0.0f, 0.5774f, 0.0f, 1.0f); // 上頂点
	Vector4 posB = Vector4(-0.5f, -0.2887f, 0.2887f, 1.0f); // 手前左
	Vector4 posC = Vector4(0.5f, -0.2887f, 0.2887f, 1.0f); // 手前右
	Vector4 posD = Vector4(0.0f, -0.2887f, -0.5774f, 1.0f); // 奥

	// --- 面1: 底面 (B, D, C) ---
	vertexData[0].position = posB;  vertexData[0].texcoord = Vector2(0.0f, 1.0f);
	vertexData[1].position = posD;  vertexData[1].texcoord = Vector2(0.5f, 0.0f);
	vertexData[2].position = posC;  vertexData[2].texcoord = Vector2(1.0f, 1.0f);

	// --- 面2: 前面 (A, C, B) ---
	vertexData[3].position = posA;  vertexData[3].texcoord = Vector2(0.5f, 0.0f);
	vertexData[4].position = posC;  vertexData[4].texcoord = Vector2(1.0f, 1.0f);
	vertexData[5].position = posB;  vertexData[5].texcoord = Vector2(0.0f, 1.0f);

	// --- 面3: 左側面 (A, B, D) ---
	vertexData[6].position = posA;  vertexData[6].texcoord = Vector2(0.5f, 0.0f);
	vertexData[7].position = posB;  vertexData[7].texcoord = Vector2(0.0f, 1.0f);
	vertexData[8].position = posD;  vertexData[8].texcoord = Vector2(1.0f, 1.0f);

	// --- 面4: 右側面 (A, D, C) ---
	vertexData[9].position = posA; vertexData[9].texcoord = Vector2(0.5f, 0.0f);
	vertexData[10].position = posD; vertexData[10].texcoord = Vector2(0.0f, 1.0f);
	vertexData[11].position = posC; vertexData[11].texcoord = Vector2(1.0f, 1.0f);

	vertexResource_->Unmap(0, nullptr);

	Logger::Log("Triangle vertex data written successfully\n");
}

void Triangle::SetWvpMatrix(const Matrix4x4& wvpMatrix, uint32_t wvpIndex) {
	if (!wvpMappedData_ || !wvpResource_) {
		Logger::Log("Triangle::SetWvpMatrix : WVP resource is not initialized\n");
		return;
	}

	char* destination = reinterpret_cast<char*>(wvpMappedData_) + wvpIndex * wvpStride_;
	std::memcpy(destination, &wvpMatrix, sizeof(Matrix4x4));
}

void Triangle::SetViewportAndScissorRect(int32_t width, int32_t height) {
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

void Triangle::SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager, TextureID textureID) {
	if (!commandList || !textureManager) {
		Logger::Log("Triangle::SetPipelineCommands : Invalid parameters\n");
		return;
	}

	// ビューポートとシザーレクトを設定
	commandList->RSSetViewports(1, &viewport_);
	commandList->RSSetScissorRects(1, &scissorRect_);

	// ルートシグネチャとパイプラインステートを設定
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipelineState_);

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = textureManager->GetSrvGpuHandle(textureID);
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
}


void Triangle::Draw(ID3D12GraphicsCommandList* commandList, uint32_t wvpIndex) {
	if (!commandList) {
		Logger::Log("Triangle::Draw : Invalid command list\n");
		return;
	}

	// 頂点バッファを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// プリミティブトポロジーを設定（三角形リスト）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	D3D12_GPU_VIRTUAL_ADDRESS wvpAddress = wvpResource_->GetGPUVirtualAddress() + (wvpStride_ * wvpIndex);
	D3D12_GPU_VIRTUAL_ADDRESS materialAddress =
		materialResource_->GetGPUVirtualAddress() + (materialStride_ * wvpIndex);
	commandList->SetGraphicsRootConstantBufferView(0, materialAddress);
	// WVP 行列をセット（ルートパラメータ1）
	commandList->SetGraphicsRootConstantBufferView(1, wvpAddress);

	// 描画コマンド（6頂点）
	commandList->DrawInstanced(kVertexCount, 1, 0, 0);
}
void Triangle::SetColor(const Vector4& color, uint32_t materialIndex) {
	if (!materialMappedData_) return;
	Vector4* data = reinterpret_cast<Vector4*>(materialMappedData_ + materialIndex * materialStride_);
	*data = color;
}