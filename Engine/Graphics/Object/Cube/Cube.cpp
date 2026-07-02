#include "Cube.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../Utils/Logger.h"
#include "../../Pipeline/Pipeline.h"
#include "../../Pipeline/PipelineCommandHelper.h"
#include <cassert>
#include <cstring>
#include <cmath>

Cube::~Cube() {
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

	// SRVをDescriptorHeapsに登録（WVP用・色用の2枠をアロケータから払い出してもらう）
	uint32_t srvBase = heaps->AllocateSRVIndex(2);
	wvpColorBuffer_.Initialize(device, heaps, kMaxInstanceCount, srvBase, srvBase + 1);

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

void Cube::SetSmoothness(float s) {
	smoothness_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
	WriteVertexData();
}

void Cube::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, index);
}

void Cube::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, worldInverseTranspose, index);
}

void Cube::SetColor(const Vector4& color, uint32_t index) {
	wvpColorBuffer_.SetColor(color, index);
}

void Cube::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	PipelineCommandHelper::ApplyCommon(commandList, rootSignature_, pipeline_->GetPipelineState(blendMode, enableAlphaTest),
		textureManager->GetSrvGpuHandle(texture), wvpColorBuffer_.GetWvpSrvHandle(), wvpColorBuffer_.GetColorSrvHandle(),
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);
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
