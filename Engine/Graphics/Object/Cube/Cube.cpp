#include "Cube.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../Utils/Logger.h"
#include "../../Pipeline/Pipeline.h"
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
	ID3D12RootSignature* rootSignature, Pipeline* pipeline,
	DescriptorHeaps* heaps) {

	textureManager_ = textureManager;
	rootSignature_  = rootSignature;
	pipeline_       = pipeline;

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
	// smoothness_ で flat(面法線) と smooth(位置正規化) をブレンドし正規化
	auto blend = [&](float px, float py, float pz, float nx, float ny, float nz) -> Vector3 {
		float slen = sqrtf(px*px + py*py + pz*pz);
		float sx = px/slen, sy = py/slen, sz = pz/slen; // smooth normal
		float x = nx + (sx - nx) * smoothness_;
		float y = ny + (sy - ny) * smoothness_;
		float z = nz + (sz - nz) * smoothness_;
		float len = sqrtf(x*x + y*y + z*z);
		return { x/len, y/len, z/len };
	};

	// 各面を2三角形（6頂点）で定義。左手座標系、時計回り
	// blend(位置xyz, 面法線xyz) で smoothness_ に応じた法線を生成
	// 前面 Z+  face normal=(0,0,1)
	v[ 0] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 1, 0 }, blend(-0.5f,  0.5f,  0.5f,  0, 0,  1) };
	v[ 1] = {{  0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, blend( 0.5f,  0.5f,  0.5f,  0, 0,  1) };
	v[ 2] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, blend(-0.5f, -0.5f,  0.5f,  0, 0,  1) };
	v[ 3] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, blend(-0.5f, -0.5f,  0.5f,  0, 0,  1) };
	v[ 4] = {{  0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, blend( 0.5f,  0.5f,  0.5f,  0, 0,  1) };
	v[ 5] = {{  0.5f, -0.5f,  0.5f, 1 }, { 0, 1 }, blend( 0.5f, -0.5f,  0.5f,  0, 0,  1) };

	// 後面 Z-  face normal=(0,0,-1)
	v[ 6] = {{  0.5f,  0.5f, -0.5f, 1 }, { 1, 0 }, blend( 0.5f,  0.5f, -0.5f,  0, 0, -1) };
	v[ 7] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, blend(-0.5f,  0.5f, -0.5f,  0, 0, -1) };
	v[ 8] = {{  0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, blend( 0.5f, -0.5f, -0.5f,  0, 0, -1) };
	v[ 9] = {{  0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, blend( 0.5f, -0.5f, -0.5f,  0, 0, -1) };
	v[10] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, blend(-0.5f,  0.5f, -0.5f,  0, 0, -1) };
	v[11] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 0, 1 }, blend(-0.5f, -0.5f, -0.5f,  0, 0, -1) };

	// 左面 X-  face normal=(-1,0,0)
	v[12] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 1, 0 }, blend(-0.5f,  0.5f, -0.5f, -1, 0,  0) };
	v[13] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, blend(-0.5f,  0.5f,  0.5f, -1, 0,  0) };
	v[14] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, blend(-0.5f, -0.5f, -0.5f, -1, 0,  0) };
	v[15] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 1, 1 }, blend(-0.5f, -0.5f, -0.5f, -1, 0,  0) };
	v[16] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, blend(-0.5f,  0.5f,  0.5f, -1, 0,  0) };
	v[17] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 0, 1 }, blend(-0.5f, -0.5f,  0.5f, -1, 0,  0) };

	// 右面 X+  face normal=(1,0,0)
	v[18] = {{  0.5f,  0.5f,  0.5f, 1 }, { 1, 0 }, blend( 0.5f,  0.5f,  0.5f,  1, 0,  0) };
	v[19] = {{  0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, blend( 0.5f,  0.5f, -0.5f,  1, 0,  0) };
	v[20] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, blend( 0.5f, -0.5f,  0.5f,  1, 0,  0) };
	v[21] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, blend( 0.5f, -0.5f,  0.5f,  1, 0,  0) };
	v[22] = {{  0.5f,  0.5f, -0.5f, 1 }, { 0, 0 }, blend( 0.5f,  0.5f, -0.5f,  1, 0,  0) };
	v[23] = {{  0.5f, -0.5f, -0.5f, 1 }, { 0, 1 }, blend( 0.5f, -0.5f, -0.5f,  1, 0,  0) };

	// 上面 Y+  face normal=(0,1,0)
	v[24] = {{ -0.5f,  0.5f, -0.5f, 1 }, { 0, 1 }, blend(-0.5f,  0.5f, -0.5f,  0,  1, 0) };
	v[25] = {{  0.5f,  0.5f, -0.5f, 1 }, { 1, 1 }, blend( 0.5f,  0.5f, -0.5f,  0,  1, 0) };
	v[26] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, blend(-0.5f,  0.5f,  0.5f,  0,  1, 0) };
	v[27] = {{ -0.5f,  0.5f,  0.5f, 1 }, { 0, 0 }, blend(-0.5f,  0.5f,  0.5f,  0,  1, 0) };
	v[28] = {{  0.5f,  0.5f, -0.5f, 1 }, { 1, 1 }, blend( 0.5f,  0.5f, -0.5f,  0,  1, 0) };
	v[29] = {{  0.5f,  0.5f,  0.5f, 1 }, { 1, 0 }, blend( 0.5f,  0.5f,  0.5f,  0,  1, 0) };

	// 下面 Y-  face normal=(0,-1,0)
	v[30] = {{ -0.5f, -0.5f,  0.5f, 1 }, { 0, 1 }, blend(-0.5f, -0.5f,  0.5f,  0, -1, 0) };
	v[31] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, blend( 0.5f, -0.5f,  0.5f,  0, -1, 0) };
	v[32] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 0, 0 }, blend(-0.5f, -0.5f, -0.5f,  0, -1, 0) };
	v[33] = {{ -0.5f, -0.5f, -0.5f, 1 }, { 0, 0 }, blend(-0.5f, -0.5f, -0.5f,  0, -1, 0) };
	v[34] = {{  0.5f, -0.5f,  0.5f, 1 }, { 1, 1 }, blend( 0.5f, -0.5f,  0.5f,  0, -1, 0) };
	v[35] = {{  0.5f, -0.5f, -0.5f, 1 }, { 1, 0 }, blend( 0.5f, -0.5f, -0.5f,  0, -1, 0) };

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

void Cube::SetSmoothness(float s) {
	smoothness_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
	WriteVertexData();
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
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength) {
	commandList->SetGraphicsRootSignature(rootSignature_);
	commandList->SetPipelineState(pipeline_->GetPipelineState(blendMode));
	const float blendFactor[4] = { blendStrength, blendStrength, blendStrength, blendStrength };
	commandList->OMSetBlendFactor(blendFactor);
	commandList->SetGraphicsRootDescriptorTable(0, textureManager->GetSrvGpuHandle(texture)); // t0: テクスチャ
	commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle_);                              // t1: WVP
	commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle_);                            // t2: 色
	commandList->SetGraphicsRoot32BitConstant(5, static_cast<UINT>(blendMode), 0);              // b2.x: blendMode
	commandList->SetGraphicsRoot32BitConstant(5, *reinterpret_cast<const UINT*>(&blendStrength), 1); // b2.y: blendStrength
}

ID3D12PipelineState* Cube::GetPipelineState() const {
	return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Cube::Draw(ID3D12GraphicsCommandList* commandList,
	uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(kVertexCount, instanceCount, 0, startInstance);
}
