#include "Cube.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../Utils/Logger.h"
#include <cassert>
#include <cstring>
#include <cmath>

Cube::~Cube() {
	if (wvpResource_ && wvpMappedData_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
		wvpResource_.Reset();
	}
	if (colorResource_ && colorMappedData_) {
		colorResource_->Unmap(0, nullptr);
		colorMappedData_ = nullptr;
		colorResource_.Reset();
	}
	vertexResource_.Reset();
}

void Cube::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState,
	DescriptorHeaps* heaps) {

	textureManager_ = textureManager;
	rootSignature_  = rootSignature;
	pipelineState_  = pipelineState;

	CreateVertexResource(device);
	WriteVertexData();
	CreateWvpResource(device);
	CreateColorResource(device);

	// Triangle が 10,11 を使っているので Cube は 12,13 を使う
	auto wvpSrv   = heaps->CreateStructuredBufferSRV(device, wvpResource_.Get(),   kMaxInstanceCount, sizeof(TransformationMatrix), 12);
	auto colorSrv = heaps->CreateStructuredBufferSRV(device, colorResource_.Get(), kMaxInstanceCount, sizeof(Vector4),   13);

	wvpSrvHandle_   = wvpSrv.gpuHandle;
	colorSrvHandle_ = colorSrv.gpuHandle;

	Logger::Log("Cube initialized successfully\n");
}

void Cube::CreateVertexResource(ID3D12Device* device) {
	vertexResource_ = ResourceFactory::CreateBufferResource(device, sizeof(VertexData) * kVertexCount);
	assert(vertexResource_);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes    = sizeof(VertexData) * kVertexCount;
	vertexBufferView_.StrideInBytes  = sizeof(VertexData);
}

void Cube::WriteVertexData() {
	VertexData* v = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&v));
	assert(SUCCEEDED(hr) && v);

	// 原点中心の cube は「位置ベクトルを正規化したもの」がスムース法線と一致する
	// 例: 角 (-0.5, 0.5, 0.5) の隣接3面の法線合計 = (-1,0,0)+(0,1,0)+(0,0,1) = (-1,1,1)
	//     normalize(-1,1,1) == normalize(-0.5,0.5,0.5) ← 位置正規化と同じ
	auto sn = [](float x, float y, float z) -> Vector3 {
		float len = sqrtf(x*x + y*y + z*z);
		return { x/len, y/len, z/len };
	};

	// 各面を2三角形（6頂点）で定義。左手座標系、時計回り
	// 前面 Z+
	v[ 0] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 1, 0 }, sn(-0.5f,  0.5f,  0.5f) };
	v[ 1] = {{  0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, sn( 0.5f,  0.5f,  0.5f) };
	v[ 2] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, sn(-0.5f, -0.5f,  0.5f) };
	v[ 3] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, sn(-0.5f, -0.5f,  0.5f) };
	v[ 4] = {{  0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, sn( 0.5f,  0.5f,  0.5f) };
	v[ 5] = {{  0.5f, -0.5f,  0.5f, 1 }, { 0, 1 }, sn( 0.5f, -0.5f,  0.5f) };

	// 後面 Z-
	v[ 6] = {{  0.5f,  0.5f, -0.5f, 1 }, { 1, 0 }, sn( 0.5f,  0.5f, -0.5f) };
	v[ 7] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, sn(-0.5f,  0.5f, -0.5f) };
	v[ 8] = {{  0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, sn( 0.5f, -0.5f, -0.5f) };
	v[ 9] = {{  0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, sn( 0.5f, -0.5f, -0.5f) };
	v[10] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, sn(-0.5f,  0.5f, -0.5f) };
	v[11] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 0, 1 }, sn(-0.5f, -0.5f, -0.5f) };

	// 左面 X-
	v[12] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 1, 0 }, sn(-0.5f,  0.5f, -0.5f) };
	v[13] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, sn(-0.5f,  0.5f,  0.5f) };
	v[14] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, sn(-0.5f, -0.5f, -0.5f) };
	v[15] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, sn(-0.5f, -0.5f, -0.5f) };
	v[16] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, sn(-0.5f,  0.5f,  0.5f) };
	v[17] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 0, 1 }, sn(-0.5f, -0.5f,  0.5f) };

	// 右面 X+
	v[18] = {{  0.5f,  0.5f,  0.5f, 1 }, { 1, 0 }, sn( 0.5f,  0.5f,  0.5f) };
	v[19] = {{  0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, sn( 0.5f,  0.5f, -0.5f) };
	v[20] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, sn( 0.5f, -0.5f,  0.5f) };
	v[21] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, sn( 0.5f, -0.5f,  0.5f) };
	v[22] = {{  0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, sn( 0.5f,  0.5f, -0.5f) };
	v[23] = {{  0.5f, -0.5f, -0.5f, 1 }, { 0, 1 }, sn( 0.5f, -0.5f, -0.5f) };

	// 上面 Y+
	v[24] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 0, 1 }, sn(-0.5f,  0.5f, -0.5f) };
	v[25] = {{  0.5f,  0.5f, -0.5f, 1 }, { 1, 1 }, sn( 0.5f,  0.5f, -0.5f) };
	v[26] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, sn(-0.5f,  0.5f,  0.5f) };
	v[27] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, sn(-0.5f,  0.5f,  0.5f) };
	v[28] = {{  0.5f,  0.5f, -0.5f, 1 }, { 1, 1 }, sn( 0.5f,  0.5f, -0.5f) };
	v[29] = {{  0.5f,  0.5f,  0.5f, 1 }, { 1, 0 }, sn( 0.5f,  0.5f,  0.5f) };

	// 下面 Y-
	v[30] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 0, 1 }, sn(-0.5f, -0.5f,  0.5f) };
	v[31] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, sn( 0.5f, -0.5f,  0.5f) };
	v[32] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 0, 0 }, sn(-0.5f, -0.5f, -0.5f) };
	v[33] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 0, 0 }, sn(-0.5f, -0.5f, -0.5f) };
	v[34] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, sn( 0.5f, -0.5f,  0.5f) };
	v[35] = {{  0.5f, -0.5f, -0.5f, 1 }, { 1, 0 }, sn( 0.5f, -0.5f, -0.5f) };

	vertexResource_->Unmap(0, nullptr);
}

void Cube::CreateWvpResource(ID3D12Device* device) {
	wvpStride_   = sizeof(TransformationMatrix);
	wvpResource_ = ResourceFactory::CreateBufferResource(device, wvpStride_ * kMaxInstanceCount);
	assert(wvpResource_);

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);
}

void Cube::CreateColorResource(ID3D12Device* device) {
	colorStride_   = sizeof(Vector4);
	colorResource_ = ResourceFactory::CreateBufferResource(device, colorStride_ * kMaxInstanceCount);
	assert(colorResource_);

	HRESULT hr = colorResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorMappedData_));
	assert(SUCCEEDED(hr) && colorMappedData_);

	// デフォルト白
	for (uint32_t i = 0; i < kMaxInstanceCount; i++) {
		Vector4* data = reinterpret_cast<Vector4*>(colorMappedData_ + i * colorStride_);
		*data = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void Cube::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	if (!wvpMappedData_) return;
	TransformationMatrix* dst = reinterpret_cast<TransformationMatrix*>(
		reinterpret_cast<char*>(wvpMappedData_) + index * wvpStride_);
	dst->WVP   = wvpMatrix;
	dst->World = world;
}

void Cube::SetColor(const Vector4& color, uint32_t index) {
	if (!colorMappedData_) return;
	Vector4* data = reinterpret_cast<Vector4*>(colorMappedData_ + index * colorStride_);
	*data = color;
}

void Cube::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture) {
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipelineState_);
	commandList->SetGraphicsRootDescriptorTable(0, textureManager->GetSrvGpuHandle(texture)); // t0: テクスチャ
	commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle_);                              // t1: WVP
	commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle_);                            // t2: 色
}

void Cube::Draw(ID3D12GraphicsCommandList* commandList,
	uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(kVertexCount, instanceCount, 0, startInstance);
}
