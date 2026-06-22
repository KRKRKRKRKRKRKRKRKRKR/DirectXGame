#include "Camera.h"
#include <DirectXMath.h>
#include "../InputDevice/InputDevice.h" // Camera と InputDevice が同階層なのでそのまま
void Camera::Initialize(const Vector3& position, const Vector3& rotation, float fov, float nearClip, float farClip) {
	cameraData_.position = position;
	cameraData_.rotation = rotation;
	cameraData_.fov = fov;
	cameraData_.nearClip = nearClip;
	cameraData_.farClip = farClip;
}


float Camera::GetAspectRatio(const int clientWidth,const int clientHeight) const {
	return static_cast<float>(clientWidth) / static_cast<float>(clientHeight);
}

Matrix4x4 Camera::GetViewMatrix() const {
	Matrix4x4 viewMatrix = TransformMath::MakeAffineMatrix(Vector3(1.0f, 1.0f, 1.0f), cameraData_.rotation, cameraData_.position);
	return MatrixMath::Inverse(viewMatrix);
}

Matrix4x4 Camera::GetProjectionMatrix(float aspectRatio) const {
	return TransformMath::MakePerspectiveForMatrix(DirectX::XMConvertToRadians(cameraData_.fov), aspectRatio, cameraData_.nearClip, cameraData_.farClip);
}

void Camera::HandleInput(float deltaTime) {
	//右クリックが押されている間だけカメラを操作
	if (Input::IsMouseRightPressed()) {
		const float mouseSensitivity = 0.004f;

		long mouseX = Input::GetMouseMoveX();
		long mouseY = Input::GetMouseMoveY();

		// マウスの移動量をカメラの回転に加算
		cameraData_.rotation.y += static_cast<float>(mouseX) * mouseSensitivity;
		cameraData_.rotation.x += static_cast<float>(mouseY) * mouseSensitivity;

		// 上下の角度制限（クランプ）
		if (cameraData_.rotation.x > 1.55f)  cameraData_.rotation.x = 1.55f;
		if (cameraData_.rotation.x < -1.55f) cameraData_.rotation.x = -1.55f;

		long wheel = Input::GetMouseWheel();
		if (wheel != 0) {
			// ホイール感度の調整
			const float zoomSensitivity = 0.1f;

			// 視野角（FOV）を変化させる場合（値を小さくするとズームイン、大きくするとズームアウト）
			// ※ cameraData_.fov の初期値は一般的に 45度（約0.785f）など
			cameraData_.fov -= static_cast<float>(wheel) * zoomSensitivity;

			// 視野角が狭くなりすぎたり（拡大しすぎ）、広くなりすぎたり（魚眼レンズ化）しないよう制限
			// 約10度〜90度の範囲に制限する例
			if (cameraData_.fov < 10) cameraData_.fov = 10;
			if (cameraData_.fov > 90)  cameraData_.fov = 90;
		}
	}

	// --- キーボードによる移動 ---
	Vector3 localMove = { 0.0f, 0.0f, 0.0f };

	if (Input::IsPressed(DIK_W)) { localMove.z += 10.0f * deltaTime; }
	if (Input::IsPressed(DIK_S)) { localMove.z -= 10.0f * deltaTime; }
	if (Input::IsPressed(DIK_A)) { localMove.x -= 10.0f * deltaTime; }
	if (Input::IsPressed(DIK_D)) { localMove.x += 10.0f * deltaTime; }

	if (Input::IsPressed(DIK_SPACE)) { cameraData_.position.y += 10.0f * deltaTime; }
	if (Input::IsPressed(DIK_LSHIFT)) { cameraData_.position.y -= 10.0f * deltaTime; }

	if (Input::IsPressed(DIK_UPARROW)) { cameraData_.rotation.x -= 0.05f * deltaTime; }
	if (Input::IsPressed(DIK_DOWNARROW)) { cameraData_.rotation.x += 0.05f * deltaTime; }
	if (Input::IsPressed(DIK_LEFTARROW)) { cameraData_.rotation.y -= 0.05f * deltaTime; }
	if (Input::IsPressed(DIK_RIGHTARROW)) { cameraData_.rotation.y += 0.05f * deltaTime; }

	Matrix4x4 rotMatrix = TransformMath::MakeAffineMatrix(
		{ 1.0f, 1.0f, 1.0f },
		cameraData_.rotation,
		{ 0.0f, 0.0f, 0.0f }
	);

	Vector3 worldMove = TransformMath::Transform(localMove, rotMatrix);

	cameraData_.position.x += worldMove.x;
	cameraData_.position.y += worldMove.y;
	cameraData_.position.z += worldMove.z;
}