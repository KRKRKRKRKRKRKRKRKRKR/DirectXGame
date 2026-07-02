#include "Sphere.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../Pipeline/Pipeline.h"
#include "../../Pipeline/PipelineCommandHelper.h"
#include <cassert>
#include <cstring>

Sphere::~Sphere() {
	vertexResource_.Reset();
}

void Sphere::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, Pipeline* pipeline,
	DescriptorHeaps* heaps,
	uint32_t subdivision, float radius) {
	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipeline_ = pipeline;

	CreateVertexResource(device, subdivision, radius);

	// SRVをDescriptorHeapsに登録（WVP用・色用の2枠をアロケータから払い出してもらう）
	uint32_t srvBase = heaps->AllocateSRVIndex(2);
	wvpColorBuffer_.Initialize(device, heaps, kMaxInstanceCount, srvBase, srvBase + 1);
}

void Sphere::SetSubdivision(uint32_t subdivision) {
	subdivision = subdivision < 1 ? 1 : (subdivision > kMaxSubdivision ? kMaxSubdivision : subdivision);
	WriteVertexData(subdivision, radius_);
}

void Sphere::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, index);
}

void Sphere::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, worldInverseTranspose, index);
}

void Sphere::SetColor(const Vector4& color, uint32_t index) {
	wvpColorBuffer_.SetColor(color, index);
}

void Sphere::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	PipelineCommandHelper::ApplyCommon(commandList, rootSignature_, pipeline_->GetPipelineState(blendMode, enableAlphaTest),
		textureManager->GetSrvGpuHandle(texture), wvpColorBuffer_.GetWvpSrvHandle(), wvpColorBuffer_.GetColorSrvHandle(),
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);
}

ID3D12PipelineState* Sphere::GetPipelineState() const {
	return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Sphere::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(vertexCount_, instanceCount, 0, startInstance);
}

void Sphere::CreateVertexResource(ID3D12Device* device, uint32_t subdivision, float radius) {
	radius_ = radius;

	// 頂点バッファは常に kMaxSubdivision 分のサイズで確保する（SetSubdivisionでの再確保を避けるため）
	const uint32_t maxVertexCount = kMaxSubdivision * kMaxSubdivision * 6;
	const size_t bufferSize = sizeof(VertexData) * maxVertexCount;

	vertexResource_ = ResourceFactory::CreateBufferResource(device, bufferSize);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = static_cast<UINT>(bufferSize);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	WriteVertexData(subdivision, radius);
}

void Sphere::WriteVertexData(uint32_t subdivision, float radius) {
	vertexCount_ = subdivision * subdivision * 6;

	VertexData* vertexData = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	assert(SUCCEEDED(hr) && vertexData);

	const float kLonEvery = DirectX::XM_2PI / subdivision;
	const float kLatEvery = DirectX::XM_PI / subdivision;

	for (uint32_t latIndex = 0; latIndex < subdivision; ++latIndex) {
		float lat = -DirectX::XM_PIDIV2 + latIndex * kLatEvery;
		for (uint32_t lonIndex = 0; lonIndex < subdivision; ++lonIndex) {
			uint32_t start = (latIndex * subdivision + lonIndex) * 6;
			float lon = lonIndex * kLonEvery;

			Vector3 a = { radius * cosf(lat) * cosf(lon),            radius * sinf(lat),            radius * cosf(lat) * sinf(lon) };
			Vector3 b = { radius * cosf(lat + kLatEvery) * cosf(lon),            radius * sinf(lat + kLatEvery), radius * cosf(lat + kLatEvery) * sinf(lon) };
			Vector3 c = { radius * cosf(lat) * cosf(lon + kLonEvery), radius * sinf(lat),            radius * cosf(lat) * sinf(lon + kLonEvery) };
			Vector3 d = { radius * cosf(lat + kLatEvery) * cosf(lon + kLonEvery), radius * sinf(lat + kLatEvery), radius * cosf(lat + kLatEvery) * sinf(lon + kLonEvery) };

			float u0 = lonIndex / static_cast<float>(subdivision);
			float u1 = (lonIndex + 1) / static_cast<float>(subdivision);
			float v0 = 1.0f - latIndex / static_cast<float>(subdivision);
			float v1 = 1.0f - (latIndex + 1) / static_cast<float>(subdivision);

			// 三角形1: a, b, c
			vertexData[start + 0] = { {a.x, a.y, a.z, 1.0f}, {u0, v0}, {a.x / radius, a.y / radius, a.z / radius} };
			vertexData[start + 1] = { {b.x, b.y, b.z, 1.0f}, {u0, v1}, {b.x / radius, b.y / radius, b.z / radius} };
			vertexData[start + 2] = { {c.x, c.y, c.z, 1.0f}, {u1, v0}, {c.x / radius, c.y / radius, c.z / radius} };

			// 三角形2: c, b, d
			vertexData[start + 3] = { {c.x, c.y, c.z, 1.0f}, {u1, v0}, {c.x / radius, c.y / radius, c.z / radius} };
			vertexData[start + 4] = { {b.x, b.y, b.z, 1.0f}, {u0, v1}, {b.x / radius, b.y / radius, b.z / radius} };
			vertexData[start + 5] = { {d.x, d.y, d.z, 1.0f}, {u1, v1}, {d.x / radius, d.y / radius, d.z / radius} };
		}
	}

	vertexResource_->Unmap(0, nullptr);
}
