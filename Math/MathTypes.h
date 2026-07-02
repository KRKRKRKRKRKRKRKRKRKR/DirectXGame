#pragma once
#include <DirectXMath.h>
#include <cstdint>
using Vector2 = DirectX::XMFLOAT2;
using Vector3 = DirectX::XMFLOAT3;
using Vector4 = DirectX::XMFLOAT4;
using Matrix4x4 = DirectX::XMFLOAT4X4;

struct UVTransform {
	Vector2 offset   = { 0.0f, 0.0f };
	float   rotation = 0.0f;
	Vector2 scale    = { 1.0f, 1.0f };
};
struct Transform {
	Vector3 scale = {1.0f, 1.0f, 1.0f};
	Vector3 rotation = {0.0f, 0.0f, 0.0f};
	Vector3 translation = {0.0f, 0.0f, 0.0f};
};

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// スキニングモデル用（BLENDINDICES/BLENDWEIGHT付き）。最大4ボーン/頂点
struct SkinnedVertexData {
	Vector4  position;
	Vector2  texcoord;
	Vector3  normal;
	uint32_t boneIndices[4] = { 0, 0, 0, 0 };
	float    boneWeights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// ボーン行列パレットの1要素。skinMatrixITは法線変換用（今回は簡略化しskinMatrixを流用）
struct BoneMatrix {
	Matrix4x4 skinMatrix;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	// Worldの逆転置行列。不均等スケールがかかっていても法線を正しく変換するために使う
	// （Worldをそのまま法線に適用すると、スケールされた軸の方向に法線が歪む）
	Matrix4x4 WorldInverseTranspose;
};

struct CameraData {
	Vector3 position = {0.0f, 0.0f, -5.0f};
	Vector3 rotation = {0.0f,0.0f,0.0f};
	float fov = 45.0f;
	float nearClip = 0.1f;
	float farClip = 100.0f;
};

struct SphereData {
	Vector3 center;
	float radius;
};

