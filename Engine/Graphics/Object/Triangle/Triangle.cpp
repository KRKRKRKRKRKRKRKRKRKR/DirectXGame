#include "Triangle.h"
#include "../../Texture/TextureManager.h"
#include "../../../Utils/Logger.h"
#include "../../../Graphics/ResourceFactory/ResourceFactory.h"

#include <cassert>
#include <cstring>
#include <cmath>

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
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
	DescriptorHeaps* heaps) {

	// 既存の処理はそのまま
	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipelineState_ = pipelineState;

	CreateVertexResource(device);
	CreateMaterialResource(device);
	CreateWvpMatrixResource(device);
	WriteVertexData();

	// SRVをDescriptorHeapsに登録（index 10: WVP, index 11: 色）
	auto wvpSrv = heaps->CreateStructuredBufferSRV(
		device, wvpResource_.Get(), kMaxInstanceCount, sizeof(Matrix4x4), 10);
	auto colorSrv = heaps->CreateStructuredBufferSRV(
		device, materialResource_.Get(), kMaxInstanceCount, sizeof(Vector4), 11);

	wvpSrvHandle_ = wvpSrv.gpuHandle;
	colorSrvHandle_ = colorSrv.gpuHandle;

	Logger::Log("Triangle initialized successfully\n");
}

void Triangle::CreateVertexResource(ID3D12Device* device) {

	vertexResource_ = ResourceFactory::CreateBufferResource(device,sizeof(VertexData) * kVertexCount);

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
	materialStride_ = sizeof(Vector4);
	materialResource_ = ResourceFactory::CreateBufferResource(device, materialStride_ * kMaxInstanceCount);

	HRESULT hr = materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialMappedData_));
	assert(SUCCEEDED(hr) && materialMappedData_);

	for (uint32_t i = 0; i < kMaxInstanceCount; i++) {
		Vector4* data = reinterpret_cast<Vector4*>(materialMappedData_ + i * materialStride_);
		*data = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void Triangle::CreateWvpMatrixResource(ID3D12Device* device) {
	wvpStride_ = sizeof(Matrix4x4);
	wvpResource_ = ResourceFactory::CreateBufferResource(device, wvpStride_ * kMaxInstanceCount);

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);

}

void Triangle::CreateSRVHeap(ID3D12Device* device) {
	// WVP と 色 の 2つ分のSRVヒープを作る
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&srvHeap_));

	UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeap_->GetCPUDescriptorHandleForHeapStart();

	// WVP の SRV (t1)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.NumElements = kMaxInstanceCount;
	srvDesc.Buffer.StructureByteStride = sizeof(Matrix4x4);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	device->CreateShaderResourceView(wvpResource_.Get(), &srvDesc, cpuHandle);
	wvpSrvHandle_ = srvHeap_->GetGPUDescriptorHandleForHeapStart();

	// 色 の SRV (t2)
	cpuHandle.ptr += descriptorSize;
	srvDesc.Buffer.StructureByteStride = sizeof(Vector4);
	device->CreateShaderResourceView(materialResource_.Get(), &srvDesc, cpuHandle);
	colorSrvHandle_.ptr = srvHeap_->GetGPUDescriptorHandleForHeapStart().ptr + descriptorSize;
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

	// 3頂点 p0,p1,p2 の外積で面法線を計算するラムダ
	auto calcNormal = [](const Vector4& p0, const Vector4& p1, const Vector4& p2) -> Vector3 {
		Vector3 e1 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
		Vector3 e2 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };
		Vector3 n  = { e1.y*e2.z - e1.z*e2.y, e1.z*e2.x - e1.x*e2.z, e1.x*e2.y - e1.y*e2.x };
		float len  = sqrtf(n.x*n.x + n.y*n.y + n.z*n.z);
		return { n.x/len, n.y/len, n.z/len };
	};

	// --- 面1: 底面 (B, D, C) ---
	{ Vector3 n = calcNormal(posB, posD, posC);
	  vertexData[0] = { posB, {0.0f, 1.0f}, n };
	  vertexData[1] = { posD, {0.5f, 0.0f}, n };
	  vertexData[2] = { posC, {1.0f, 1.0f}, n }; }

	// --- 面2: 前面 (A, C, B) ---
	{ Vector3 n = calcNormal(posA, posC, posB);
	  vertexData[3] = { posA, {0.5f, 0.0f}, n };
	  vertexData[4] = { posC, {1.0f, 1.0f}, n };
	  vertexData[5] = { posB, {0.0f, 1.0f}, n }; }

	// --- 面3: 左側面 (A, B, D) ---
	{ Vector3 n = calcNormal(posA, posB, posD);
	  vertexData[6] = { posA, {0.5f, 0.0f}, n };
	  vertexData[7] = { posB, {0.0f, 1.0f}, n };
	  vertexData[8] = { posD, {1.0f, 1.0f}, n }; }

	// --- 面4: 右側面 (A, D, C) ---
	{ Vector3 n = calcNormal(posA, posD, posC);
	  vertexData[9]  = { posA, {0.5f, 0.0f}, n };
	  vertexData[10] = { posD, {0.0f, 1.0f}, n };
	  vertexData[11] = { posC, {1.0f, 1.0f}, n }; }

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

void Triangle::SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager, TextureHandle texture) {
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipelineState_);

	commandList->SetGraphicsRootDescriptorTable(0, textureManager->GetSrvGpuHandle(texture)); // t0: テクスチャ
	commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle_);                              // t1: WVP
	commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle_);                            // t2: 色
}

void Triangle::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(kVertexCount, instanceCount, 0, startInstance);
}
void Triangle::SetColor(const Vector4& color, uint32_t materialIndex) {
	if (!materialMappedData_) return;
	Vector4* data = reinterpret_cast<Vector4*>(materialMappedData_ + materialIndex * materialStride_);
	*data = color;
}

