#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include "../IDrawable.h"
#include "../../../../Math/MathTypes.h"
#include "../../Texture/TextureManager.h"
#include "../../DescriptorHeaps/DescriptorHeaps.h"
#include "../../ResourceFactory/InstancedWvpColorBuffer.h"
#include "../../Pipeline/BlendMode.h"

using Microsoft::WRL::ComPtr;

class Pipeline;
class SkinnedPipeline;

class Model : public IDrawable {
public:
	Model() = default;
	virtual ~Model();

	static constexpr uint32_t kMaxInstanceCount = 4096;
	static constexpr uint32_t kMaxBoneCount     = 64; // HumanModel.fbx(13本)等に十分な余裕を持たせた上限

	void Initialize(ID3D12Device* device, TextureManager* textureManager,
		ID3D12RootSignature* rootSignature, Pipeline* pipeline,
		SkinnedPipeline* skinnedPipeline, ID3D12RootSignature* skinnedRootSignature,
		DescriptorHeaps* heaps,
		const std::string& directoryPath, const std::string& filename,
		uint32_t wvpHeapIndex, uint32_t colorHeapIndex, uint32_t boneMatrixHeapIndex);

	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index = 0);
	void SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, const Matrix4x4& worldInverseTranspose, uint32_t index = 0);
	void SetColor(const Vector4& color, uint32_t index = 0);
	void SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
		TextureManager* textureManager, TextureHandle texture, BlendMode blendMode = BlendMode::kNone, float blendStrength = 1.0f,
		bool enableAlphaTest = false, float alphaThreshold = 0.5f);

	TextureHandle GetTextureHandle() const { return textureHandle_; }

	// アニメーションを1フレーム進め、ボーン行列パレットを更新する。ボーンが無いモデルは何もしない
	void UpdateAnimation(float deltaTime);

	void Draw(ID3D12GraphicsCommandList* commandList,
		uint32_t instanceCount, uint32_t startInstance = 0) override;

	ID3D12RootSignature* GetRootSignature() const override { return rootSignature_; }
	ID3D12PipelineState* GetPipelineState() const override;

private:
	// 学校資料に準拠した構造体
	struct MaterialData {
		std::string textureFilePath;
	};

	struct ModelData {
		std::vector<VertexData>        vertices;
		std::vector<SkinnedVertexData> skinnedVertices; // hasSkeleton時のみ使用（verticesと同じ頂点数・同じ並び）
		MaterialData                   material;
	};

	// ---- スキニング/アニメーション用メタデータ（Step1: 読み込み・ログ確認のみ。GPU適用は未実装）----

	// ノード階層（Assimpの aiNode をフラット化したもの。DFS順で親が必ず子より前に来る）
	struct NodeData {
		std::string name;
		int         parentIndex = -1;
		Matrix4x4   localTransform{}; // バインド時のローカル変換（aiNode::mTransformation）
	};

	// 位置/回転/スケールの時刻付きキーフレーム（1ボーン分）
	struct NodeAnimation {
		std::vector<std::pair<float, Vector3>>              positionKeys;
		std::vector<std::pair<float, DirectX::XMFLOAT4>>    rotationKeys; // クォータニオン(x,y,z,w)
		std::vector<std::pair<float, Vector3>>              scaleKeys;
	};

	struct SkeletonData {
		bool                                       hasSkeleton = false;
		std::vector<NodeData>                      nodeTree;
		// ノード名→nodeTree内indexの逆引き。ロード後は不変なので初回構築時（WriteBindPoseBoneMatrices）
		// に一度だけ作り、毎フレーム呼ばれるUpdateAnimationではこれを使い回して再構築を避ける
		std::unordered_map<std::string, size_t>   nodeNameToIndex;
		std::unordered_map<std::string, uint32_t>  boneNameToIndex;
		std::vector<Matrix4x4>                     boneOffsetMatrices;

		bool                                        hasAnimation = false;
		std::string                                 animationName;
		float                                        animationDuration    = 0.0f;
		float                                        ticksPerSecond       = 25.0f;
		std::unordered_map<std::string, NodeAnimation> animationChannels;
	};

	uint32_t      vertexCount_   = 0;
	TextureHandle textureHandle_ = kTextureNone;

	ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	InstancedWvpColorBuffer wvpColorBuffer_;

	ID3D12RootSignature* rootSignature_  = nullptr;
	Pipeline*            pipeline_       = nullptr;
	TextureManager*      textureManager_ = nullptr;

	// スキニング用（skeleton_.hasSkeleton==trueの場合のみ使用）
	SkinnedPipeline*      skinnedPipeline_      = nullptr;
	ID3D12RootSignature*  skinnedRootSignature_ = nullptr;
	ComPtr<ID3D12Resource> boneMatrixResource_;
	uint8_t*  boneMatrixMappedData_ = nullptr;
	D3D12_GPU_DESCRIPTOR_HANDLE boneMatrixSrvHandle_{};

	// ボーン・アニメーションのメタデータ
	SkeletonData skeleton_;
	float animationTime_ = 0.0f; // 現在の再生時刻（tick単位、ループする）

	// MTLファイルからテクスチャパスを読む（学校資料の LoadMaterialTemplateFile 相当）
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath,
		const std::string& filename);

	// OBJファイルを読む（自前パーサー）
	ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	// OBJ以外（FBX/glTF等）をAssimp経由で読む
	ModelData LoadModelFileWithAssimp(const std::string& directoryPath, const std::string& filename);

	// aiSceneからノード階層・ボーン・アニメーションのメタデータを抽出し skeleton_ に格納する
	void LoadSkeletonAndAnimation(const struct aiScene* scene);

	void CreateVertexResource (ID3D12Device* device, const ModelData& modelData);
	void CreateBoneMatrixResource(ID3D12Device* device, DescriptorHeaps* heaps, uint32_t boneMatrixHeapIndex);

	// Step2疎通確認: 全ボーン行列を単位行列で埋める
	void WriteIdentityBoneMatrices();

	// Step3: ノード階層からバインドポーズのボーン行列を計算して書き込む
	void WriteBindPoseBoneMatrices();

	// Step4: 指定時刻(tick)でのノードのローカル変換を計算する。
	// アニメーションチャンネルが無いノードはバインド時のlocalTransformをそのまま返す
	Matrix4x4 CalcNodeLocalTransform(const NodeData& node, float timeInTicks) const;
};
