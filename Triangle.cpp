#include "Triangle.h"
#include "../DirectXGame/TextureManager.h"
#include "../DirectXGame/Utils/Logger.h"


#include <cassert>
#include <cstring>

namespace {
	constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
		return (value + (alignment - 1)) & ~(alignment - 1);
	}
}

Triangle::~Triangle() {
	// リソース解放
	if (wvpResource_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
		wvpResource_.Reset();
	}

	vertexResource_.Reset();
	materialResource_.Reset();
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
	// 三角形は6頂点（2つの三角形で1つの四角形を構成）
	vertexResource_ = textureManager_->CreateBufferResource(sizeof(VertexData) * 6);

	if (!vertexResource_) {
		Logger::Log("Triangle::CreateVertexResource : Failed to create vertex buffer\n");
		assert(false);
		return;
	}

	// 頂点バッファビュー設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Triangle::CreateMaterialResource(ID3D12Device* device) {
	materialResource_ = textureManager_->CreateBufferResource(sizeof(Vector4));

	if (!materialResource_) {
		Logger::Log("Triangle::CreateMaterialResource : Failed to create material buffer\n");
		assert(false);
		return;
	}

	Vector4* materialData = nullptr;
	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	if (FAILED(hr) || !materialData) {
		Logger::Log("Triangle::CreateMaterialResource : Failed to map material buffer\n");
		assert(false);
		return;
	}

	// デフォルト: 白色（R, G, B, A = 1.0）
	*materialData = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	materialResource_->Unmap(0, nullptr);
}

void Triangle::CreateWvpMatrixResource(ID3D12Device* device) {
	// WVP 行列プール用リソースを作成
	// DirectXManager から提供される行列プールを使用するため、
	// ここでは小さなローカル行列バッファのみ作成

	wvpStride_ = AlignUp(static_cast<uint32_t>(sizeof(Matrix4x4)), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	wvpResource_ = textureManager_->CreateBufferResource(static_cast<size_t>(wvpStride_) * 1);

	if (!wvpResource_) {
		Logger::Log("Triangle::CreateWvpMatrixResource : Failed to create WVP buffer\n");
		assert(false);
		return;
	}

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	if (FAILED(hr) || !wvpMappedData_) {
		Logger::Log("Triangle::CreateWvpMatrixResource : Failed to map WVP buffer\n");
		assert(false);
		return;
	}

	wvpIndex_ = 0;
}

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

	// 三角形の頂点データ（2つの三角形で四角形を構成）
	// 三角形1
	vertexData[0].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[0].texcoord = Vector2(0.0f, 1.0f);

	vertexData[1].position = Vector4(0.0f, 0.5f, 0.0f, 1.0f);
	vertexData[1].texcoord = Vector2(0.5f, 0.0f);

	vertexData[2].position = Vector4(0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[2].texcoord = Vector2(1.0f, 1.0f);

	// 三角形2
	vertexData[3].position = Vector4(-0.5f, -0.5f, 0.0f, 1.0f);
	vertexData[3].texcoord = Vector2(0.0f, 1.0f);

	vertexData[4].position = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	vertexData[4].texcoord = Vector2(0.5f, 0.0f);

	vertexData[5].position = Vector4(0.5f, -0.5f, -0.5f, 1.0f);
	vertexData[5].texcoord = Vector2(1.0f, 1.0f);

	vertexResource_->Unmap(0, nullptr);

	Logger::Log("Triangle vertex data written successfully\n");
}

void Triangle::SetWvpMatrix(const Matrix4x4& wvpMatrix) {
	if (!wvpMappedData_ || !wvpResource_) {
		Logger::Log("Triangle::SetWvpMatrix : WVP resource is not initialized\n");
		return;
	}

	std::memcpy(wvpMappedData_, &wvpMatrix, sizeof(Matrix4x4));
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

void Triangle::SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager) {
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

	// テクスチャ SRV ハンドルを設定（デフォルト: Texture3）
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = textureManager->GetSrvGpuHandle(TextureID::Texture3);
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
}

void Triangle::Draw(ID3D12GraphicsCommandList* commandList) {
	if (!commandList) {
		Logger::Log("Triangle::Draw : Invalid command list\n");
		return;
	}

	// 頂点バッファを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	// プリミティブトポロジーを設定（三角形リスト）
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// マテリアルバッファをセット（ルートパラメータ0）
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	// WVP 行列をセット（ルートパラメータ1）
	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	// テクスチャセットはSetPipelineCommands()で既に設定済み

	// 描画コマンド（6頂点）
	commandList->DrawInstanced(6, 1, 0, 0);
}