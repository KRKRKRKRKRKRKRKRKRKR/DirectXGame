#include "CameraManager.h"
#include <DirectXMath.h>

void CameraManager::Initialize(const Vector3& position, const Vector3& rotation, float fovY, float nearClip, float farClip) {
	cameraData_.position = position;
	cameraData_.rotation = rotation;
	cameraData_.fovY = fovY;
	cameraData_.nearClip = nearClip;
	cameraData_.farClip = farClip;
}

void CameraManager::Update() {
	// カメラの更新処理（必要に応じて実装）
}

float CameraManager::GetAspeRatio(const int clientWidth,const int clientHeight) const {
	return static_cast<float>(clientWidth) / static_cast<float>(clientHeight);

}

Matrix4x4 CameraManager::GetViewMatrix() const {
	Matrix4x4 viewMatrix = TransformMath::MakeAffineMatrix(Vector3(1.0f, 1.0f, 1.0f), cameraData_.rotation, cameraData_.position);
	return MatrixMath::Inverse(viewMatrix);
}

Matrix4x4 CameraManager::GetProjectionMatrix(float aspectRatio) const {
	return TransformMath::MakePerspectiveForMatrix(DirectX::XMConvertToRadians(cameraData_.fovY), aspectRatio, cameraData_.nearClip, cameraData_.farClip);
}