// Assimp経由のモデル読み込み（FBX/glTF等）。ボーン・ノード階層・アニメーションの
// メタデータ抽出もここで行う（GPUリソース化はModel.cpp側の責務）
#include "Model.h"
#include "../../../Utils/Logger.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <algorithm>
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
}

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

	// 各aiMaterialのdiffuseテクスチャパスを先に解決しておく（サブメッシュ側からmMaterialIndexで引く）
	std::vector<std::string> materialTexturePaths(scene->mNumMaterials);
	for (unsigned int mi = 0; mi < scene->mNumMaterials; ++mi) {
		aiMaterial* material = scene->mMaterials[mi];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			materialTexturePaths[mi] = directoryPath + "/" + texturePath.C_Str();
		}
	}

	// マルチメッシュ/マルチマテリアル対応：aiMesh1個＝サブメッシュ1個として扱う
	// （Assimpは元々マテリアルが違うポリゴンを別々のaiMeshに分割して渡してくるため、
	// mMaterialIndexをそのままサブメッシュのテクスチャ解決に使える）
	for (unsigned int m = 0; m < scene->mNumMeshes; ++m) {
		const aiMesh* mesh = scene->mMeshes[m];

		SubMeshData subMesh;
		subMesh.vertexOffset = static_cast<uint32_t>(modelData.vertices.size());
		if (mesh->mMaterialIndex < materialTexturePaths.size()) {
			subMesh.material.textureFilePath = materialTexturePaths[mesh->mMaterialIndex];
		}

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

		subMesh.vertexCount = static_cast<uint32_t>(modelData.vertices.size()) - subMesh.vertexOffset;
		modelData.subMeshes.push_back(subMesh);
	}

	// メッシュが1個も無いファイルへの安全策：空のサブメッシュを1個用意しておく
	// （Model::Initializeは常に1個以上のsubMeshesを期待するため）
	if (modelData.subMeshes.empty()) {
		modelData.subMeshes.push_back(SubMeshData{});
	}

	return modelData;
}

// ---- ボーン階層・アニメーションのメタデータ読み込み ----

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
