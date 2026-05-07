#pragma once
#include "../Math/MathTypes.h"
#include "../Math/TransformMath.h"
#include "../Math/MatrixMath.h"

class CameraManager {
public:
	CameraManager() = default;
	~CameraManager() = default;

	void Initialize(const Vector3& position = { 0.0f, 0.0f, -5.0f }, const Vector3& rotation = { 0.0f, 0.0f,  0.0f }, float fovY = 45.0f, float nearClip = 0.1f, float farClip = 1000.0f);
	void Update();

	float GetAspeRatio(const int clientWidth,const int clientHeight)const;
	Matrix4x4 GetViewMatrix() const;
	Matrix4x4 GetProjectionMatrix(float aspectRatio) const;
	const CameraData& GetCameraData() const { return cameraData_; };

	void SetPosition(const Vector3& position) { cameraData_.position = position; }
	void SetRotation(const Vector3& rotation) { cameraData_.rotation = rotation; }
	void SetFovY(float fovY) { cameraData_.fovY = fovY; }

private:
	CameraData cameraData_;
};