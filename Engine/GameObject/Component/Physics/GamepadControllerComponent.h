#pragma once
#include "../../IComponent.h"

// ゲームパッド（Xbox コントローラ等、XInput対応機器）の左スティックでTransform.translationを
// XZ平面上に動かす、PlayerControllerComponent（WASD版）のゲームパッド版。
// GetGamepadLeftStickX/Yは既にInputDevice側で円形デッドゾーン込みで-1.0〜1.0に正規化されている
// ため、PlayerControllerComponentのような追加の正規化は不要
class GamepadControllerComponent : public IComponent {
public:
	bool  enabled = true;
	float moveSpeed = 5.0f;      // 1秒あたりの移動量
	int   controllerIndex = 0;   // 0〜3。将来複数コントローラ（マルチプレイヤー）に対応する余地として公開しておく

	void Update(float deltaTime, Transform& transform) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["moveSpeed"] = moveSpeed;
		out["controllerIndex"] = controllerIndex;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		moveSpeed = in.value("moveSpeed", moveSpeed);
		controllerIndex = in.value("controllerIndex", controllerIndex);
	}
};
