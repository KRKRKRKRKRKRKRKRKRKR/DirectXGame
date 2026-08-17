#pragma once
#include "../../IComponent.h"

// ゲームパッドの左スティックで、オーナー自身が向いている方向（transform.rotation.yのyaw）を
// 基準にTransform.translationをXZ平面上に動かす、FPS/TPS風の移動コンポーネント。
// GamepadCameraLookComponent（右スティックでyawを回す）とペアで同じGameObjectに付けると、
// 「Rスティックで向きを変え、Lスティックでその向きに進む」という一般的なゲームパッド操作になる。
// ワールド軸固定で移動したいだけならGamepadControllerComponentを使う
class GamepadMoveComponent : public IComponent {
public:
	bool  enabled = true;
	float moveSpeed = 5.0f;      // 1秒あたりの移動量
	int   controllerIndex = 0;   // 0〜3。将来複数コントローラ（マルチプレイヤー）に対応する余地として公開しておく

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
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
