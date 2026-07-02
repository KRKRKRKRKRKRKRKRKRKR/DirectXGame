// Modelクラスの中核: GPUリソース（頂点バッファ・WVP/色/ボーン行列のStructuredBuffer）の
// 作成・更新・描画コマンド発行を担う。実際のファイル読み込みは
// Model_ObjLoader.cpp / Model_AssimpLoader.cpp / Model_SkeletalAnimation.cpp に分割している
#include "Model.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../Utils/Logger.h"
#include "../../Pipeline/Pipeline.h"
#include "../../Pipeline/SkinnedPipeline.h"
#include "../../Pipeline/PipelineCommandHelper.h"
#include <fstream>
#include <cassert>
#include <cstring>

// ---- デストラクタ ----

Model::~Model() {
	vertexResource_.Reset();
}

// ---- 初期化 ----

void Model::Initialize(ID3D12Device* device, TextureManager* textureManager,
	ID3D12RootSignature* rootSignature, Pipeline* pipeline,
	SkinnedPipeline* skinnedPipeline, ID3D12RootSignature* skinnedRootSignature,
	DescriptorHeaps* heaps,
	const std::string& directoryPath, const std::string& filename,
	uint32_t wvpHeapIndex, uint32_t colorHeapIndex, uint32_t boneMatrixHeapIndex) {

	textureManager_       = textureManager;
	rootSignature_        = rootSignature;
	pipeline_             = pipeline;
	skinnedPipeline_      = skinnedPipeline;
	skinnedRootSignature_ = skinnedRootSignature;

	// OBJは自前パーサー、それ以外（FBX/glTF等）はAssimp経由で読み込む
	std::string ext = filename.substr(filename.find_last_of('.') + 1);
	ModelData modelData = (ext == "obj" || ext == "OBJ")
		? LoadObjFile(directoryPath, filename)
		: LoadModelFileWithAssimp(directoryPath, filename);
	vertexCount_ = skeleton_.hasSkeleton
		? static_cast<uint32_t>(modelData.skinnedVertices.size())
		: static_cast<uint32_t>(modelData.vertices.size());

	// MTLで指定されたテクスチャをロード（ファイルが存在する場合のみ）
	if (!modelData.material.textureFilePath.empty()) {
		std::ifstream check(modelData.material.textureFilePath);
		if (check.is_open()) {
			textureHandle_ = textureManager->Load(modelData.material.textureFilePath, heaps);
		}
	}

	CreateVertexResource(device, modelData);
	wvpColorBuffer_.Initialize(device, heaps, kMaxInstanceCount, wvpHeapIndex, colorHeapIndex);

	if (skeleton_.hasSkeleton) {
		CreateBoneMatrixResource(device, heaps, boneMatrixHeapIndex);
		WriteIdentityBoneMatrices(); // 念のため未対応ボーンのスロットを単位行列で初期化しておく
		WriteBindPoseBoneMatrices(); // 実際のバインドポーズ行列を書き込む
	}

	Logger::Log("Model loaded: " + directoryPath + "/" + filename +
		" | " + std::to_string(vertexCount_) + " vertices\n");
}

// ---- GPUリソース作成 ----

void Model::CreateVertexResource(ID3D12Device* device, const ModelData& modelData) {
	// スキニングモデルはSkinnedVertexData（BLENDINDICES/BLENDWEIGHT付き）、
	// それ以外は従来通りVertexDataで頂点バッファを作る
	size_t stride = skeleton_.hasSkeleton ? sizeof(SkinnedVertexData) : sizeof(VertexData);
	const void* srcData = skeleton_.hasSkeleton
		? static_cast<const void*>(modelData.skinnedVertices.data())
		: static_cast<const void*>(modelData.vertices.data());

	vertexResource_ = ResourceFactory::CreateBufferResource(device, stride * vertexCount_);
	assert(vertexResource_);

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes    = static_cast<UINT>(stride * vertexCount_);
	vertexBufferView_.StrideInBytes  = static_cast<UINT>(stride);

	void* mapped = nullptr;
	HRESULT hr = vertexResource_->Map(0, nullptr, &mapped);
	assert(SUCCEEDED(hr) && mapped);
	std::memcpy(mapped, srcData, stride * vertexCount_);
	vertexResource_->Unmap(0, nullptr);
}

// ---- 描画 ----

void Model::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, index);
}

void Model::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index) {
	wvpColorBuffer_.SetWvpMatrix(wvpMatrix, world, worldInverseTranspose, index);
}

void Model::SetColor(const Vector4& color, uint32_t index) {
	wvpColorBuffer_.SetColor(color, index);
}

void Model::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {

	bool skinned = skeleton_.hasSkeleton;
	ID3D12RootSignature* rs  = skinned ? skinnedRootSignature_ : rootSignature_;
	ID3D12PipelineState* pso = skinned ? skinnedPipeline_->GetPipelineState(blendMode, enableAlphaTest)
	                                    : pipeline_->GetPipelineState(blendMode, enableAlphaTest);

	PipelineCommandHelper::ApplyCommon(commandList, rs, pso,
		textureManager->GetSrvGpuHandle(texture), wvpColorBuffer_.GetWvpSrvHandle(), wvpColorBuffer_.GetColorSrvHandle(),
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);

	if (skinned) {
		commandList->SetGraphicsRootDescriptorTable(6, boneMatrixSrvHandle_); // t3: ボーン行列パレット
	}
}

ID3D12PipelineState* Model::GetPipelineState() const {
	return pipeline_->GetPipelineState(BlendMode::kNone);
}

void Model::Draw(ID3D12GraphicsCommandList* commandList,
	uint32_t instanceCount, uint32_t startInstance) {
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(vertexCount_, instanceCount, 0, startInstance);
}
