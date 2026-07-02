#include "Model.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../Utils/Logger.h"
#include "../../Pipeline/Pipeline.h"
#include "../../Pipeline/SkinnedPipeline.h"
#include "../../../../Math/MatrixMath.h"
#include <fstream>
#include <sstream>
#include <cassert>
#include <cstring>
#include <functional>
#include <algorithm>
#include <cmath>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace {
	// Assimpのaiマトリックス4x4は「列ベクトル・左からかける(v'=M*v)」規約。
	// DirectXMath/HLSLのmulは「行ベクトル・右からかける(v'=v*M)」規約なので、
	// 単純コピーではなく転置してからコピーする必要がある
	Matrix4x4 ToMatrix4x4(const aiMatrix4x4& m) {
		Matrix4x4 out{};
		out._11 = m.a1; out._12 = m.b1; out._13 = m.c1; out._14 = m.d1;
		out._21 = m.a2; out._22 = m.b2; out._23 = m.c2; out._24 = m.d2;
		out._31 = m.a3; out._32 = m.b3; out._33 = m.c3; out._34 = m.d3;
		out._41 = m.a4; out._42 = m.b4; out._43 = m.c4; out._44 = m.d4;
		return out;
	}

	// time以下で最大のキーのindexを探す（末尾なら最後のキーを返す）
	template<typename T>
	size_t FindKeyIndex(const std::vector<std::pair<float, T>>& keys, float time) {
		for (size_t i = 0; i + 1 < keys.size(); ++i) {
			if (time < keys[i + 1].first) return i;
		}
		return keys.empty() ? 0 : keys.size() - 1;
	}

	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t) {
		return Vector3{
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t };
	}

	// Position/Scaleキー配列を時刻で線形補間する共通処理
	Vector3 InterpolateVector3Keys(const std::vector<std::pair<float, Vector3>>& keys, float time, const Vector3& defaultValue) {
		if (keys.empty()) return defaultValue;
		if (keys.size() == 1) return keys[0].second;

		size_t i = FindKeyIndex(keys, time);
		if (i + 1 >= keys.size()) return keys[i].second;

		float t0 = keys[i].first;
		float t1 = keys[i + 1].first;
		float t  = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
		return LerpVector3(keys[i].second, keys[i + 1].second, t);
	}

	// Rotationキー配列を時刻でSLERP補間する
	DirectX::XMFLOAT4 InterpolateRotationKeys(const std::vector<std::pair<float, DirectX::XMFLOAT4>>& keys, float time, const DirectX::XMFLOAT4& defaultValue) {
		if (keys.empty()) return defaultValue;
		if (keys.size() == 1) return keys[0].second;

		size_t i = FindKeyIndex(keys, time);
		if (i + 1 >= keys.size()) return keys[i].second;

		float t0 = keys[i].first;
		float t1 = keys[i + 1].first;
		float t  = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;

		DirectX::XMVECTOR q0 = DirectX::XMLoadFloat4(&keys[i].second);
		DirectX::XMVECTOR q1 = DirectX::XMLoadFloat4(&keys[i + 1].second);
		DirectX::XMVECTOR q  = DirectX::XMQuaternionSlerp(q0, q1, t);

		DirectX::XMFLOAT4 result;
		DirectX::XMStoreFloat4(&result, q);
		return result;
	}
}

// ---- デストラクタ ----

Model::~Model() {
	if (wvpResource_ && wvpMappedData_) {
		wvpResource_->Unmap(0, nullptr);
		wvpMappedData_ = nullptr;
	}
	if (colorResource_ && colorMappedData_) {
		colorResource_->Unmap(0, nullptr);
		colorMappedData_ = nullptr;
	}
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
	CreateWvpResource(device);
	CreateColorResource(device);

	auto wvpSrv   = heaps->CreateStructuredBufferSRV(device, wvpResource_.Get(),   kMaxInstanceCount, sizeof(TransformationMatrix), wvpHeapIndex);
	auto colorSrv = heaps->CreateStructuredBufferSRV(device, colorResource_.Get(), kMaxInstanceCount, sizeof(Vector4),   colorHeapIndex);

	wvpSrvHandle_   = wvpSrv.gpuHandle;
	colorSrvHandle_ = colorSrv.gpuHandle;

	if (skeleton_.hasSkeleton) {
		CreateBoneMatrixResource(device, heaps, boneMatrixHeapIndex);
		WriteIdentityBoneMatrices(); // 念のため未対応ボーンのスロットを単位行列で初期化しておく
		WriteBindPoseBoneMatrices(); // Step3: 実際のバインドポーズ行列を書き込む
	}

	Logger::Log("Model loaded: " + directoryPath + "/" + filename +
		" | " + std::to_string(vertexCount_) + " vertices\n");
}

// ---- MTLパース（学校資料の LoadMaterialTemplateFile 相当）----

Model::MaterialData Model::LoadMaterialTemplateFile(
	const std::string& directoryPath, const std::string& filename) {

	MaterialData materialData;
	std::ifstream file(directoryPath + "/" + filename);
	if (!file.is_open()) return materialData;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			// OBJと同じフォルダにテクスチャがあると想定してパスを結合
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

// ---- OBJパース ----

Model::ModelData Model::LoadObjFile(
	const std::string& directoryPath, const std::string& filename) {

	std::vector<Vector3> positions;
	std::vector<Vector2> texcoords;
	std::vector<Vector3> normals;
	ModelData modelData;

	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream s(line);
		std::string identifier;
		s >> identifier;

		if (identifier == "mtllib") {
			std::string mtlFilename;
			s >> mtlFilename;
			modelData.material = LoadMaterialTemplateFile(directoryPath, mtlFilename);

		} else if (identifier == "v") {
			Vector3 p;
			s >> p.x >> p.y >> p.z;
			p.x *= -1.0f;
			positions.push_back(p);

		} else if (identifier == "vt") {
			Vector2 uv;
			s >> uv.x >> uv.y;
			texcoords.push_back(uv);

		} else if (identifier == "vn") {
			Vector3 n;
			s >> n.x >> n.y >> n.z;
			n.x *= -1.0f;
			normals.push_back(n);

		} else if (identifier == "f") {
			// 全頂点トークンを読む（三角形・四角形・n角形対応）
			std::vector<VertexData> faceVerts;
			std::string token;
			while (s >> token) {
				std::istringstream v(token);
				std::string posStr, uvStr, normalStr;
				std::getline(v, posStr,    '/');
				std::getline(v, uvStr,     '/');
				std::getline(v, normalStr, '/');

				VertexData vert{};
				if (!posStr.empty()) {
					Vector3 p = positions[std::stoi(posStr) - 1];
					vert.position = { p.x, p.y, p.z, 1.0f };
				}
				// UVなし（"pos//normal"形式）のときは (0,0) のまま
				if (!uvStr.empty() && !texcoords.empty()) {
					vert.texcoord = texcoords[std::stoi(uvStr) - 1];
					vert.texcoord.y = 1.0f - vert.texcoord.y;
				}
				if (!normalStr.empty()) {
					vert.normal = normals[std::stoi(normalStr) - 1];
				}
				faceVerts.push_back(vert);
			}

			// ファンアルゴリズムで三角形に分割（左手系に合わせて逆順登録）
			for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
				modelData.vertices.push_back(faceVerts[0]);
				modelData.vertices.push_back(faceVerts[i + 1]);
				modelData.vertices.push_back(faceVerts[i]);
			}
		}
	}
	return modelData;
}

// ---- Assimp経由のモデル読み込み（FBX/glTF等）----

Model::ModelData Model::LoadModelFileWithAssimp(
	const std::string& directoryPath, const std::string& filename) {

	ModelData modelData;
	std::string filePath = directoryPath + "/" + filename;

	Assimp::Importer importer;
	// MakeLeftHanded + FlipWindingOrder : 自前OBJパーサーのX軸反転+巻き順逆転と同じ狙いで左手系に変換
	// FlipUVs                           : 自前パーサーの `1.0f - v` と同じ狙いでUVのV軸を反転
	// Triangulate                       : 三角形以外の面を三角形に分割（自前パーサーのファン分割相当）
	// GenNormals                        : 法線が無いモデルにも対応
	const aiScene* scene = importer.ReadFile(filePath,
		aiProcess_Triangulate |
		aiProcess_FlipUVs |
		aiProcess_MakeLeftHanded |
		aiProcess_FlipWindingOrder |
		aiProcess_GenNormals);

	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
		Logger::Log("Model: Assimp failed to load '" + filePath + "': " + importer.GetErrorString() + "\n");
		assert(false);
		return modelData;
	}

	// ボーン名→パレットindex、ノード階層、アニメーションを先に読み込んでおく
	// （頂点ループ内でボーン名からindexを引くため）
	LoadSkeletonAndAnimation(scene);

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh* mesh = scene->mMeshes[m];

		// このメッシュの頂点ごとに、影響するボーンを最大4本まで集計する
		// aiBone::mWeights[] は「このボーンが影響する頂点のリスト」という逆引き形式なので、
		// 頂点ごとの配列に変換する
		std::vector<std::vector<std::pair<uint32_t, float>>> vertexBoneData(mesh->mNumVertices);
		if (mesh->HasBones()) {
			for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
				const aiBone* bone = mesh->mBones[b];
				auto it = skeleton_.boneNameToIndex.find(bone->mName.C_Str());
				if (it == skeleton_.boneNameToIndex.end()) continue;
				uint32_t boneIndex = it->second;

				for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
					const aiVertexWeight& vw = bone->mWeights[w];
					vertexBoneData[vw.mVertexId].push_back({ boneIndex, vw.mWeight });
				}
			}
		}

		for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
			const aiFace& face = mesh->mFaces[f];
			assert(face.mNumIndices == 3); // Triangulate済みなので必ず3頂点

			for (unsigned int e = 0; e < face.mNumIndices; ++e) {
				uint32_t idx = face.mIndices[e];

				VertexData vert{};
				const aiVector3D& pos = mesh->mVertices[idx];
				vert.position = { pos.x, pos.y, pos.z, 1.0f };

				if (mesh->HasNormals()) {
					const aiVector3D& n = mesh->mNormals[idx];
					vert.normal = { n.x, n.y, n.z };
				}

				if (mesh->HasTextureCoords(0)) {
					const aiVector3D& uv = mesh->mTextureCoords[0][idx];
					vert.texcoord = { uv.x, uv.y };
				}

				modelData.vertices.push_back(vert);

				if (skeleton_.hasSkeleton) {
					SkinnedVertexData skinnedVert{};
					skinnedVert.position = vert.position;
					skinnedVert.texcoord = vert.texcoord;
					skinnedVert.normal   = vert.normal;

					// ウェイトの大きい順に上位4件を採用し、合計1.0になるよう正規化する
					auto& influences = vertexBoneData[idx];
					std::sort(influences.begin(), influences.end(),
						[](const auto& a, const auto& b) { return a.second > b.second; });

					float weightSum = 0.0f;
					size_t count = std::min<size_t>(influences.size(), 4);
					for (size_t i = 0; i < count; ++i) {
						skinnedVert.boneIndices[i] = influences[i].first;
						skinnedVert.boneWeights[i] = influences[i].second;
						weightSum += influences[i].second;
					}
					if (weightSum > 1e-5f) {
						for (size_t i = 0; i < count; ++i) {
							skinnedVert.boneWeights[i] /= weightSum;
						}
					} else {
						// どのボーンにも属さない頂点はindex0に固定してバインドポーズ崩壊を防ぐ
						skinnedVert.boneIndices[0] = 0;
						skinnedVert.boneWeights[0] = 1.0f;
					}

					modelData.skinnedVertices.push_back(skinnedVert);
				}
			}
		}
	}

	// 最初に見つかったdiffuseテクスチャのパスをマテリアルとして採用
	for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi) {
		aiMaterial* material = scene->mMaterials[mi];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			modelData.material.textureFilePath = directoryPath + "/" + texturePath.C_Str();
			break;
		}
	}

	return modelData;
}

// ---- ボーン階層・アニメーションのメタデータ読み込み（Step1: ログ確認のみ）----

void Model::LoadSkeletonAndAnimation(const aiScene* scene) {
	// ---- ノード階層をDFSでフラット化（親は必ず子より前に来る）----
	std::function<void(const aiNode*, int)> collectNodes =
		[this, &collectNodes](const aiNode* node, int parentIndex) {
			NodeData data;
			data.name           = node->mName.C_Str();
			data.parentIndex    = parentIndex;
			data.localTransform = ToMatrix4x4(node->mTransformation);

			int myIndex = static_cast<int>(skeleton_.nodeTree.size());
			skeleton_.nodeTree.push_back(data);

			for (unsigned int i = 0; i < node->mNumChildren; ++i) {
				collectNodes(node->mChildren[i], myIndex);
			}
		};
	collectNodes(scene->mRootNode, -1);

	// ---- ボーン（名前→パレットindex、オフセット行列）を全メッシュから収集 ----
	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh* mesh = scene->mMeshes[m];
		if (!mesh->HasBones()) continue;

		skeleton_.hasSkeleton = true;
		for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
			const aiBone* bone = mesh->mBones[b];
			std::string   boneName = bone->mName.C_Str();

			if (skeleton_.boneNameToIndex.find(boneName) != skeleton_.boneNameToIndex.end()) {
				continue; // 既に登録済み（複数メッシュで共有されるボーン）
			}
			uint32_t boneIndex = static_cast<uint32_t>(skeleton_.boneOffsetMatrices.size());
			skeleton_.boneNameToIndex[boneName] = boneIndex;
			skeleton_.boneOffsetMatrices.push_back(ToMatrix4x4(bone->mOffsetMatrix));
		}
	}

	// ---- アニメーション（今回は最初の1クリップのみ扱う）----
	if (scene->mNumAnimations > 0) {
		const aiAnimation* anim = scene->mAnimations[0];
		skeleton_.hasAnimation      = true;
		skeleton_.animationName     = anim->mName.C_Str();
		skeleton_.animationDuration = static_cast<float>(anim->mDuration);
		skeleton_.ticksPerSecond    = anim->mTicksPerSecond != 0.0
			? static_cast<float>(anim->mTicksPerSecond)
			: 25.0f; // FBXは0のことがあるため慣例のデフォルト値を使う

		for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
			const aiNodeAnim* channel = anim->mChannels[c];
			NodeAnimation nodeAnim;

			for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
				const aiVectorKey& key = channel->mPositionKeys[k];
				nodeAnim.positionKeys.push_back({
					static_cast<float>(key.mTime),
					Vector3{ key.mValue.x, key.mValue.y, key.mValue.z } });
			}
			for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
				const aiQuatKey& key = channel->mRotationKeys[k];
				nodeAnim.rotationKeys.push_back({
					static_cast<float>(key.mTime),
					DirectX::XMFLOAT4{ key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w } });
			}
			for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
				const aiVectorKey& key = channel->mScalingKeys[k];
				nodeAnim.scaleKeys.push_back({
					static_cast<float>(key.mTime),
					Vector3{ key.mValue.x, key.mValue.y, key.mValue.z } });
			}

			skeleton_.animationChannels[channel->mNodeName.C_Str()] = std::move(nodeAnim);
		}
	}

	// ---- 確認用ログ ----
	Logger::Log("Model: skeleton nodeCount=" + std::to_string(skeleton_.nodeTree.size()) +
		", hasSkeleton=" + (skeleton_.hasSkeleton ? std::string("true") : std::string("false")) +
		", boneCount=" + std::to_string(skeleton_.boneOffsetMatrices.size()) + "\n");

	if (skeleton_.hasAnimation) {
		Logger::Log("Model: animation name='" + skeleton_.animationName +
			"', duration=" + std::to_string(skeleton_.animationDuration) +
			", ticksPerSecond=" + std::to_string(skeleton_.ticksPerSecond) +
			", channelCount=" + std::to_string(skeleton_.animationChannels.size()) + "\n");
	} else {
		Logger::Log("Model: no animation found\n");
	}
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

void Model::CreateBoneMatrixResource(ID3D12Device* device, DescriptorHeaps* heaps, uint32_t boneMatrixHeapIndex) {
	boneMatrixResource_ = ResourceFactory::CreateBufferResource(device, sizeof(BoneMatrix) * kMaxBoneCount);
	assert(boneMatrixResource_);

	HRESULT hr = boneMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&boneMatrixMappedData_));
	assert(SUCCEEDED(hr) && boneMatrixMappedData_);

	auto boneSrv = heaps->CreateStructuredBufferSRV(device, boneMatrixResource_.Get(),
		kMaxBoneCount, sizeof(BoneMatrix), boneMatrixHeapIndex);
	boneMatrixSrvHandle_ = boneSrv.gpuHandle;
}

void Model::WriteIdentityBoneMatrices() {
	if (!boneMatrixMappedData_) return;
	BoneMatrix identity{};
	identity.skinMatrix = MatrixMath::Identity();
	for (uint32_t i = 0; i < kMaxBoneCount; ++i) {
		BoneMatrix* dst = reinterpret_cast<BoneMatrix*>(boneMatrixMappedData_ + i * sizeof(BoneMatrix));
		*dst = identity;
	}
}

void Model::WriteBindPoseBoneMatrices() {
	if (!boneMatrixMappedData_ || skeleton_.nodeTree.empty()) return;

	// ノード階層をルートから順に辿り、各ノードのグローバル変換（バインドポーズ、
	// アニメーションのキーは使わずmTransformationのみ）を計算する。
	// nodeTreeはDFS順で親が必ず子より前に来るため、1パスの前方走査で計算できる
	std::vector<Matrix4x4> globalTransforms(skeleton_.nodeTree.size());
	for (size_t i = 0; i < skeleton_.nodeTree.size(); ++i) {
		const NodeData& node = skeleton_.nodeTree[i];
		globalTransforms[i] = (node.parentIndex >= 0)
			? node.localTransform * globalTransforms[node.parentIndex]
			: node.localTransform;
	}

	// ボーン名からノードを引くための逆引きマップ（nodeTree内のindex）
	std::unordered_map<std::string, size_t> nodeNameToIndex;
	for (size_t i = 0; i < skeleton_.nodeTree.size(); ++i) {
		nodeNameToIndex[skeleton_.nodeTree[i].name] = i;
	}

	for (const auto& [boneName, boneIndex] : skeleton_.boneNameToIndex) {
		auto it = nodeNameToIndex.find(boneName);
		if (it == nodeNameToIndex.end()) continue;

		Matrix4x4 boneMatrix = skeleton_.boneOffsetMatrices[boneIndex] * globalTransforms[it->second];

		BoneMatrix* dst = reinterpret_cast<BoneMatrix*>(
			boneMatrixMappedData_ + boneIndex * sizeof(BoneMatrix));
		dst->skinMatrix = boneMatrix;
	}
}

Matrix4x4 Model::CalcNodeLocalTransform(const NodeData& node, float timeInTicks) const {
	auto it = skeleton_.animationChannels.find(node.name);
	if (it == skeleton_.animationChannels.end()) {
		return node.localTransform; // このノードを動かすチャンネルが無ければバインド時の変換のまま
	}
	const NodeAnimation& channel = it->second;

	Vector3            position = InterpolateVector3Keys(channel.positionKeys, timeInTicks, Vector3{ 0.0f, 0.0f, 0.0f });
	Vector3            scale    = InterpolateVector3Keys(channel.scaleKeys,    timeInTicks, Vector3{ 1.0f, 1.0f, 1.0f });
	DirectX::XMFLOAT4  rotation = InterpolateRotationKeys(channel.rotationKeys, timeInTicks, DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 1.0f });

	DirectX::XMMATRIX s = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
	DirectX::XMMATRIX r = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
	DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(position.x, position.y, position.z);

	Matrix4x4 result;
	DirectX::XMStoreFloat4x4(&result, s * r * t);
	return result;
}

void Model::UpdateAnimation(float deltaTime) {
	if (!skeleton_.hasSkeleton || !skeleton_.hasAnimation || !boneMatrixMappedData_) return;

	animationTime_ += deltaTime * skeleton_.ticksPerSecond;
	if (skeleton_.animationDuration > 0.0f) {
		animationTime_ = std::fmod(animationTime_, skeleton_.animationDuration);
	}

	// ノード階層をルートから順に辿り、現在時刻でのグローバル変換を計算する
	// （nodeTreeはDFS順で親が必ず子より前に来るため、1パスの前方走査で計算できる）
	std::vector<Matrix4x4> globalTransforms(skeleton_.nodeTree.size());
	for (size_t i = 0; i < skeleton_.nodeTree.size(); ++i) {
		const NodeData& node = skeleton_.nodeTree[i];
		Matrix4x4 localTransform = CalcNodeLocalTransform(node, animationTime_);
		globalTransforms[i] = (node.parentIndex >= 0)
			? localTransform * globalTransforms[node.parentIndex]
			: localTransform;
	}

	std::unordered_map<std::string, size_t> nodeNameToIndex;
	for (size_t i = 0; i < skeleton_.nodeTree.size(); ++i) {
		nodeNameToIndex[skeleton_.nodeTree[i].name] = i;
	}

	for (const auto& [boneName, boneIndex] : skeleton_.boneNameToIndex) {
		auto it = nodeNameToIndex.find(boneName);
		if (it == nodeNameToIndex.end()) continue;

		Matrix4x4 boneMatrix = skeleton_.boneOffsetMatrices[boneIndex] * globalTransforms[it->second];

		BoneMatrix* dst = reinterpret_cast<BoneMatrix*>(
			boneMatrixMappedData_ + boneIndex * sizeof(BoneMatrix));
		dst->skinMatrix = boneMatrix;
	}
}

void Model::CreateWvpResource(ID3D12Device* device) {
	wvpStride_   = sizeof(TransformationMatrix);
	wvpResource_ = ResourceFactory::CreateBufferResource(device, wvpStride_ * kMaxInstanceCount);
	assert(wvpResource_);

	HRESULT hr = wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpMappedData_));
	assert(SUCCEEDED(hr) && wvpMappedData_);
}

void Model::CreateColorResource(ID3D12Device* device) {
	colorStride_   = sizeof(Vector4);
	colorResource_ = ResourceFactory::CreateBufferResource(device, colorStride_ * kMaxInstanceCount);
	assert(colorResource_);

	HRESULT hr = colorResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorMappedData_));
	assert(SUCCEEDED(hr) && colorMappedData_);

	// デフォルト白
	for (uint32_t i = 0; i < kMaxInstanceCount; i++) {
		Vector4* dst = reinterpret_cast<Vector4*>(colorMappedData_ + i * colorStride_);
		*dst = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

// ---- 描画 ----

void Model::SetWvpMatrix(const Matrix4x4& wvpMatrix, const Matrix4x4& world, uint32_t index) {
	if (!wvpMappedData_) return;
	TransformationMatrix* dst = reinterpret_cast<TransformationMatrix*>(
		reinterpret_cast<char*>(wvpMappedData_) + index * wvpStride_);
	dst->WVP   = wvpMatrix;
	dst->World = world;
	dst->WorldInverseTranspose = MatrixMath::Transpose(MatrixMath::Inverse(world));
}

void Model::SetColor(const Vector4& color, uint32_t index) {
	if (!colorMappedData_) return;
	Vector4* dst = reinterpret_cast<Vector4*>(colorMappedData_ + index * colorStride_);
	*dst = color;
}

void Model::SetPipelineCommands(ID3D12GraphicsCommandList* commandList,
	TextureManager* textureManager, TextureHandle texture, BlendMode blendMode, float blendStrength,
	bool enableAlphaTest, float alphaThreshold) {

	bool skinned = skeleton_.hasSkeleton;
	ID3D12RootSignature* rs  = skinned ? skinnedRootSignature_ : rootSignature_;
	ID3D12PipelineState* pso = skinned ? skinnedPipeline_->GetPipelineState(blendMode)
	                                    : pipeline_->GetPipelineState(blendMode);

	commandList->SetGraphicsRootSignature(rs);
	commandList->SetPipelineState(pso);
	const float blendFactor[4] = { blendStrength, blendStrength, blendStrength, blendStrength };
	commandList->OMSetBlendFactor(blendFactor);
	commandList->SetGraphicsRootDescriptorTable(0, textureManager->GetSrvGpuHandle(texture));
	commandList->SetGraphicsRootDescriptorTable(1, wvpSrvHandle_);
	commandList->SetGraphicsRootDescriptorTable(2, colorSrvHandle_);
	commandList->SetGraphicsRoot32BitConstant(5, static_cast<UINT>(blendMode), 0);              // b2.x: blendMode
	commandList->SetGraphicsRoot32BitConstant(5, *reinterpret_cast<const UINT*>(&blendStrength), 1); // b2.y: blendStrength
	commandList->SetGraphicsRoot32BitConstant(5, static_cast<UINT>(enableAlphaTest), 2);         // b2.z: enableAlphaTest
	commandList->SetGraphicsRoot32BitConstant(5, *reinterpret_cast<const UINT*>(&alphaThreshold), 3); // b2.w: alphaThreshold

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
