#include "Camera.h"
#include <DirectXMath.h>

void Camera::Initialize(const Vector3& position, const Vector3& rotation, float fov, float nearClip, float farClip) {
	cameraData_.position = position;
	cameraData_.rotation = rotation;
	cameraData_.fov = fov;
	cameraData_.nearClip = nearClip;
	cameraData_.farClip = farClip;
}

void Camera::Update() {
	// カメラの更新処理（必要に応じて実装）
}

float Camera::GetAspeRatio(const int clientWidth,const int clientHeight) const {
	return static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
}

Matrix4x4 Camera::GetViewMatrix() const {
	Matrix4x4 viewMatrix = TransformMath::MakeAffineMatrix(Vector3(1.0f, 1.0f, 1.0f), cameraData_.rotation, cameraData_.position);
	return MatrixMath::Inverse(viewMatrix);
}

Matrix4x4 Camera::GetProjectionMatrix(float aspectRatio) const {
	return TransformMath::MakePerspectiveForMatrix(DirectX::XMConvertToRadians(cameraData_.fov), aspectRatio, cameraData_.nearClip, cameraData_.farClip);
}