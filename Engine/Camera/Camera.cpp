#include "Camera.h"
#include <DirectXMath.h>
#include "../InputDevice/InputDevice.h" // Camera と InputDevice が同階層なのでそのまま
#include "../../Math/VectorMath.h"
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
	(void)deltaTime; // Unity Sceneビュー方式はマウス移動量そのものを使うためフレームレート非依存

	const float rotateSensitivity = 0.004f;
	const float panSensitivity = 0.005f;
	// ホイールの生値はマウス移動量(数十程度)と桁が違い1ノッチ=約120のため、
	// rotate/panと同じ感覚の係数を使うと1ノッチで数十〜百ユニット動いてしまう。桁を合わせて小さくする
	const float dollySensitivity = 0.01f;

	long mouseX = Input::GetMouseMoveX();
	long mouseY = Input::GetMouseMoveY();

	// マウス移動量をrotation.x/yへ反映し、上下約88度でクランプする
	// （Look/Orbitの両方から使う共通処理。ジンバルロック防止）
	auto applyRotationDelta = [&]() {
		cameraData_.rotation.y += static_cast<float>(mouseX) * rotateSensitivity;
		cameraData_.rotation.x += static_cast<float>(mouseY) * rotateSensitivity;
		if (cameraData_.rotation.x > 1.55f)  cameraData_.rotation.x = 1.55f;
		if (cameraData_.rotation.x < -1.55f) cameraData_.rotation.x = -1.55f;
	};

	bool altPressed = Input::IsPressed(DIK_LMENU) || Input::IsPressed(DIK_RMENU);

	if (altPressed && Input::IsMouseLeftPressed()) {
		// Orbit：注視点（focusDistance_だけ前方にある点）を中心に周回する。
		// 回転前のforwardからpivotを求めておき、回転更新後の新しいforwardで
		// pivotから逆算してpositionを求め直すことで、注視点を固定したまま回り込める
		Vector3 forwardBefore = TransformMath::EulerRadiansToDirection(cameraData_.rotation);
		Vector3 pivot = cameraData_.position + forwardBefore * focusDistance_;

		applyRotationDelta();

		Vector3 forwardAfter = TransformMath::EulerRadiansToDirection(cameraData_.rotation);
		cameraData_.position = pivot - forwardAfter * focusDistance_;
	} else if (Input::IsMouseRightPressed()) {
		// Look：その場で視点だけ回転する（位置は変えない）
		applyRotationDelta();
	}

	if (Input::IsMouseMiddlePressed()) {
		// Pan：画面に対して平行に移動する
		Matrix4x4 rot = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, cameraData_.rotation, { 0.0f, 0.0f, 0.0f });
		Vector3 right = TransformMath::Transform({ 1.0f, 0.0f, 0.0f }, rot);
		Vector3 up    = TransformMath::Transform({ 0.0f, 1.0f, 0.0f }, rot);
		cameraData_.position = cameraData_.position
			- right * (static_cast<float>(mouseX) * panSensitivity)
			+ up    * (static_cast<float>(mouseY) * panSensitivity);
	}

	long wheel = Input::GetMouseWheel();
	if (wheel != 0) {
		// Dolly：注視点へ向かって前後に移動する（ボタン操作不要、常時有効）
		Vector3 forward = TransformMath::EulerRadiansToDirection(cameraData_.rotation);
		float amount = static_cast<float>(wheel) * dollySensitivity;
		cameraData_.position = cameraData_.position + forward * amount;
		focusDistance_ -= amount;
		if (focusDistance_ < 0.5f) focusDistance_ = 0.5f;
	}
}