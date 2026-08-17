#pragma once
#include "../../IComponent.h"

// ゲームパッドの右スティックで、オーナーのTransform.rotationのyaw/pitchを回すマウスルック相当の
// コンポーネント。CameraComponentは自身では回転を持たずオーナーのTransformから向きを導出するため、
// これをCameraComponentと同じGameObjectに付けるとRスティックでの視点操作になる。
// GamepadMoveComponent（左スティックで同じyawを基準に移動）とペアで付けるのが典型的な使い方
class GamepadCameraLookComponent : public IComponent {
public:
	bool  enabled = true;
	float sensitivity = 2.5f;   // 1秒あたりの回転量（ラジアン、スティック全開時）
	bool  invertY = false;

	// 真上・真下を向いて反転しないようピッチ角に掛ける制限（ラジアン）。既定は±89度相当
	float minPitch = -1.55334f;
	float maxPitch = 1.55334f;

	int controllerIndex = 0; // 0〜3。将来複数コントローラ（マルチプレイヤー）に対応する余地として公開しておく

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["sensitivity"] = sensitivity;
		out["invertY"] = invertY;
		out["minPitch"] = minPitch;
		out["maxPitch"] = maxPitch;
		out["controllerIndex"] = controllerIndex;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		sensitivity = in.value("sensitivity", sensitivity);
		invertY = in.value("invertY", invertY);
		minPitch = in.value("minPitch", minPitch);
		maxPitch = in.value("maxPitch", maxPitch);
		controllerIndex = in.value("controllerIndex", controllerIndex);
	}
};
