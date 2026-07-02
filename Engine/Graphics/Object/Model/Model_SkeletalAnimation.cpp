// GPUスキニング用ボーン行列パレットの構築・更新。バインドポーズ計算とキーフレーム
// アニメーション再生の両方をここに集約する
#include "Model.h"
#include "../../ResourceFactory/ResourceFactory.h"
#include "../../../../Math/MatrixMath.h"
#include <cassert>
#include <cstring>
#include <cmath>

namespace {
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

	// ボーン名からノードを引くための逆引きマップ（nodeTree内のindex）。
	// ノード階層はロード後不変なのでここで一度だけ構築し、毎フレーム呼ばれるUpdateAnimationで使い回す
	skeleton_.nodeNameToIndex.clear();
	for (size_t i = 0; i < skeleton_.nodeTree.size(); ++i) {
		skeleton_.nodeNameToIndex[skeleton_.nodeTree[i].name] = i;
	}

	for (const auto& [boneName, boneIndex] : skeleton_.boneNameToIndex) {
		auto it = skeleton_.nodeNameToIndex.find(boneName);
		if (it == skeleton_.nodeNameToIndex.end()) continue;

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

	for (const auto& [boneName, boneIndex] : skeleton_.boneNameToIndex) {
		auto it = skeleton_.nodeNameToIndex.find(boneName);
		if (it == skeleton_.nodeNameToIndex.end()) continue;

		Matrix4x4 boneMatrix = skeleton_.boneOffsetMatrices[boneIndex] * globalTransforms[it->second];

		BoneMatrix* dst = reinterpret_cast<BoneMatrix*>(
			boneMatrixMappedData_ + boneIndex * sizeof(BoneMatrix));
		dst->skinMatrix = boneMatrix;
	}
}
