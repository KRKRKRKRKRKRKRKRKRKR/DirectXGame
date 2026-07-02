#include "Triangle.h"
#include "../../Texture/TextureManager.h"
#include "../../../Utils/Logger.h"
#include "../../../Graphics/ResourceFactory/ResourceFactory.h"
#include "../../Pipeline/Pipeline.h"
#include "../../Pipeline/PipelineCommandHelper.h"

#include <cassert>
#include <cstring>
#include <cmath>

namespace {
	constexpr uint32_t AlignUp(uint32_t value, uint32_t alignment) {
		return (value + (alignment - 1)) & ~(alignment - 1);
	}
}

Triangle::~Triangle() {
	vertexResource_.Reset();
}

void Triangle::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, Pipeline* pipeline,
	DescriptorHeaps* heaps) {

	// 既存の処理はそのまま
	textureManager_ = textureManager;
	rootSignature_ = rootSignature;
	pipeline_ = pipeline;

	CreateVertexResource(device);
	WriteVertexData();

	// SRVをDescriptorHeapsに登録（WVP用・色用の2枠をアロケータから払い出してもらう）
	uint32_t srvBase = heaps->AllocateSRVIndex(2);
	wvpColorBuffer_.Initialize(device, heaps, kMaxInstanceCount, srvBase, srvBase + 1);

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

	// 4面の法線を計算（側面3つは外向きになるよう頂点順を逆にする）
	Vector3 n1 = calcNormal(posB, posD, posC); // 面1: 底面  → 下向き (0,-1,0)
	Vector3 n2 = calcNormal(posA, posB, posC); // 面2: 前面  → 前上向き
	Vector3 n3 = calcNormal(posA, posD, posB); // 面3: 左側面 → 左上向き
	Vector3 n4 = calcNormal(posA, posC, posD); // 面4: 右側面 → 右上向き

	// 各頂点は3面に属するので、その3面の法線を平均・正規化してスムース法線を求める
	//   A → 面2,3,4  /  B → 面1,2,3  /  C → 面1,2,4  /  D → 面1,3,4
	auto avg3 = [](const Vector3& a, const Vector3& b, const Vector3& c) -> Vector3 {
		float x = a.x+b.x+c.x, y = a.y+b.y+c.y, z = a.z+b.z+c.z;
		float len = sqrtf(x*x + y*y + z*z);
		return { x/len, y/len, z/len };
	};
	Vector3 sA = avg3(n2, n3, n4);
	Vector3 sB = avg3(n1, n2, n3);
	Vector3 sC = avg3(n1, n2, n4);
	Vector3 sD = avg3(n1, n3, n4);

	// smoothness_ で flat(面法線) と smooth(平均法線) をブレンド
	// lerp 後に正規化（lerp された中間ベクトルは単位長でないため）
	auto blend = [&](const Vector3& flat, const Vector3& smooth) -> Vector3 {
		float x = flat.x + (smooth.x - flat.x) * smoothness_;
		float y = flat.y + (smooth.y - flat.y) * smoothness_;
		float z = flat.z + (smooth.z - flat.z) * smoothness_;
		float len = sqrtf(x*x + y*y + z*z);
		return { x/len, y/len, z/len };
	};

	// --- 面1: 底面 (B, D, C)  flat=n1 ---
	vertexData[0] = { posB, {0.0f, 1.0f}, blend(n1, sB) };
	vertexData[1] = { posD, {0.5f, 0.0f}, blend(n1, sD) };
	vertexData[2] = { posC, {1.0f, 1.0f}, blend(n1, sC) };

	// --- 面2: 前面 (A, B, C)  flat=n2 ---
	vertexData[3] = { posA, {0.5f, 0.0f}, blend(n2, sA) };
	vertexData[4] = { posB, {0.0f, 1.0f}, blend(n2, sB) };
	vertexData[5] = { posC, {1.0f, 1.0f}, blend(n2, sC) };

	// --- 面3: 左側面 (A, D, B)  flat=n3 ---
	vertexData[6] = { posA, {0.5f, 0.0f}, blend(n3, sA) };
	vertexData[7] = { posD, {1.0f, 1.0f}, blend(n3, sD) };
	vertexData[8] = { posB, {0.0f, 1.0f}, blend(n3, sB) };

	// --- 面4: 右側面 (A, C, D)  flat=n4 ---
	vertexData[9]  = { posA, {0.5f, 0.0f}, blend(n4, sA) };
	vertexData[10] = { posC, {1.0f, 1.0f}, blend(n4, sC) };
	vertexData[11] = { posD, {0.0f, 1.0f}, blend(n4, sD) };

	vertexResource_->Unmap(0, nullptr);

	Logger::Log("Triangle vertex data written successfully\n");
}

void Triangle::SetSmoothness(float s) {
	smoothness_ = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s);
	WriteVertexData();
}

void Triangle::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t wvpIndex) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, wvpIndex);
}

void Triangle::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t wvpIndex) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, worldInverseTranspose, wvpIndex);
}

void Triangle::SetPipelineCommands(ID3D12GraphicsCommandList* commandList, TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {
	PipelineCommandHelper::ApplyCommon(commandList, rootSignature_, pipeline_->GetPipelineState(blendMode, enableAlphaTest),
		textureManager->GetSrvGpuHandle(texture), wvpColorBuffer_.GetWvpSrvHandle(), wvpColorBuffer_.GetColorSrvHandle(),
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);
}

ID3D12PipelineState* Triangle::GetPipelineState() const {
	return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Triangle::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(kVertexCount, instanceCount, 0, startInstance);
}
void Triangle::SetColor(const Vector4& color, uint32_t materialIndex) {
	wvpColorBuffer_.SetColor(color, materialIndex);
}

