#include "Sprite.h"
#include "../../../../Math/MatrixMath.h"
#include "../../ResourceFactory/ResourceFactory.h"
void Sprite::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState) {
	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipelineState_ = pipelineState;
	CreateVertexResource(device);
	WriteVertexData();
	CreateMaterialResource(device);
	CreateTransformationMatrixResource(device);
}

void Sprite::SetWvpMatrix(const Matrix4x4& wvpMatrix) {
	*transformationMatrixData_ = wvpMatrix;
}

void Sprite::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureID textureID) {
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipelineState_);
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress()); // 1 = WVP
	commandList->SetGraphicsRootDescriptorTable(2, textureManager->GetSrvGpuHandle(textureID));              // 2 = Texture

}

void Sprite::Draw(ID3D12GraphicsCommandList* commandList, uint32_t wvpIndex) {
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(kVertexCount, 1, 0, 0);

}

void Sprite::CreateMaterialResource(ID3D12Device* device) {
	materialResource_ = ResourceFactory::CreateBufferResource(device, sizeof(Vector4));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	*materialData_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f); // 白
}

void Sprite::CreateVertexResource(ID3D12Device* device) {
	// 頂点バッファリソースの作成
	vertexResource_ = ResourceFactory::CreateBufferResource(device, sizeof(VertexData) * kVertexCount);
	vertexResource_->SetName(L"Sprite Vertex Buffer");
	// 頂点バッファビューの設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * kVertexCount);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Sprite::WriteVertexData() {
	VertexData* vertexData = nullptr;
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	vertexData[0] = { {-0.5f, -0.5f, 0.0f, 1.0f}, {0.0f, 1.0f} };
	vertexData[1] = { { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f} };
	vertexData[2] = { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f} };
	vertexData[3] = { {-0.5f,  0.5f, 0.0f, 1.0f}, {0.0f, 0.0f} };
	vertexData[4] = { { 0.5f, -0.5f, 0.0f, 1.0f}, {1.0f, 1.0f} };
	vertexData[5] = { { 0.5f,  0.5f, 0.0f, 1.0f}, {1.0f, 0.0f} };
	vertexResource_->Unmap(0, nullptr);
}

void Sprite::CreateTransformationMatrixResource(ID3D12Device* device) {
	transformationMatrixResource_ = ResourceFactory::CreateBufferResource(device, sizeof(Matrix4x4));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
	*transformationMatrixData_ = MatrixMath::Identity();
}