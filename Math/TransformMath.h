#pragma once
#include "MathTypes.h"
#include <DirectXMath.h>

namespace TransformMath {

	// アフィン変換行列の作成
	inline Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotation, const Vector3& translation) {
		Matrix4x4 result;
		// DirectXMathで各行列を作成
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);
		// SRT順で合成（Scale → Rotate → Translate）
		DirectX::XMMATRIX mat = S * R * T;
		DirectX::XMStoreFloat4x4(&result, mat);
		return result;
	}

	//正射影行列
	inline Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
		Matrix4x4 result;
		DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearClip, farClip));
		return result;
	}

	//透視投影行列
	inline Matrix4x4 MakePerspectiveForMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
		Matrix4x4 result;
		DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearClip, farClip));
		return result;
	}

	//ビューポート行列
	inline Matrix4x4 MakeViewPortMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
		Matrix4x4 result = {};
		result._11 = width / 2.0f;
		result._22 = -height / 2.0f;
		result._33 = maxDepth - minDepth;
		result._41 = left + width / 2.0f;
		result._42 = top + height / 2.0f;
		result._43 = minDepth;
		result._44 = 1.0f;
		return result;
	}

	// ベクトルを行列で変換
	inline Vector3 Transform(const Vector3& v, const Matrix4x4& m) {
		DirectX::XMMATRIX mat = DirectX::XMLoadFloat4x4(&m);
		DirectX::XMVECTOR vec = DirectX::XMLoadFloat3(&v);
		DirectX::XMVECTOR result = DirectX::XMVector3TransformCoord(vec, mat);
		Vector3 out;
		DirectX::XMStoreFloat3(&out, result);
		return out;
	}

	// オイラー角(ラジアン、XMMatrixRotationRollPitchYaw規約) → 正規化済み方向ベクトル。
	// 基準方向{0,0,1}(ワールドZ+)をその回転で変換するだけの単純な変換。
	// SpotLightの方向をギズモの回転結果から書き戻す際などに使う
	inline Vector3 EulerRadiansToDirection(const Vector3& eulerRadians) {
		DirectX::XMMATRIX rot = DirectX::XMMatrixRotationRollPitchYaw(eulerRadians.x, eulerRadians.y, eulerRadians.z);
		DirectX::XMVECTOR baseDir = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(baseDir, rot));
		Vector3 result;
		DirectX::XMStoreFloat3(&result, dir);
		return result;
	}
}