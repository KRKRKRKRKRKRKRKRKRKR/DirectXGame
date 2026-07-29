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

	// サブメッシュ（頂点範囲）はマルチマテリアル対応のため維持するが、MTL/FBXが指す
	// テクスチャは自動ロードしない。Cube/Sphere/Sprite等の他の描画コンポーネントと同じく、
	// テクスチャはTextureSelectorComponentで手動アタッチする運用に統一する
	// （テクスチャ未割り当てのサブメッシュはtextureHandle=kTextureNone=白テクスチャのまま）。
	// ファイルが実際に指すテクスチャパスはログにだけ残しておき、手動で選ぶ際の参考にできるようにする
	subMeshes_.clear();
	subMeshes_.reserve(modelData.subMeshes.size());
	for (const auto& src : modelData.subMeshes) {
		SubMesh subMesh;
		subMesh.vertexOffset = src.vertexOffset;
		subMesh.vertexCount  = src.vertexCount;
		subMeshes_.push_back(subMesh);
		if (!src.material.textureFilePath.empty()) {
			Logger::Log("Model: sub-mesh material suggests texture '" + src.material.textureFilePath +
				"' (not auto-loaded; attach a TextureSelectorComponent to use it)\n");
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
		" | " + std::to_string(vertexCount_) + " vertices, " +
		std::to_string(subMeshes_.size()) + " sub-mesh(es)\n");
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
	// IDrawableインターフェース互換の後方互換パス：全頂点を単一テクスチャ（先頭サブメッシュの
	// ものがSetPipelineCommandsでバインドされている前提）で描く。マルチマテリアルモデルを
	// 正しく描くにはRenderer::FlushModelsのようにサブメッシュ単位でSetPipelineCommandsForSubMesh
	// →DrawSubMeshを呼ぶこと
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(vertexCount_, instanceCount, 0, startInstance);
}

void Model::SetPipelineCommandsForSubMesh(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, size_t subMeshIndex, TextureHandle overrideTexture,
	BlendMode blendMode, float blendStrength, bool enableAlphaTest, float alphaThreshold) {

	TextureHandle tex = (overrideTexture != kTextureNone) ? overrideTexture : subMeshes_[subMeshIndex].textureHandle;

	bool skinned = skeleton_.hasSkeleton;
	ID3D12RootSignature* rs  = skinned ? skinnedRootSignature_ : rootSignature_;
	ID3D12PipelineState* pso = skinned ? skinnedPipeline_->GetPipelineState(blendMode, enableAlphaTest)
	                                    : pipeline_->GetPipelineState(blendMode, enableAlphaTest);

	PipelineCommandHelper::ApplyCommon(commandList, rs, pso,
		textureManager->GetSrvGpuHandle(tex), wvpColorBuffer_.GetWvpSrvHandle(), wvpColorBuffer_.GetColorSrvHandle(),
		blendMode, blendStrength, enableAlphaTest, alphaThreshold);

	if (skinned) {
		commandList->SetGraphicsRootDescriptorTable(6, boneMatrixSrvHandle_); // t3: ボーン行列パレット
	}
}

void Model::DrawSubMesh(ID3D12GraphicsCommandList* commandList, size_t subMeshIndex,
	uint32_t instanceCount, uint32_t startInstance) {
	const SubMesh& subMesh = subMeshes_[subMeshIndex];
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->DrawInstanced(subMesh.vertexCount, instanceCount, subMesh.vertexOffset, startInstance);
}
