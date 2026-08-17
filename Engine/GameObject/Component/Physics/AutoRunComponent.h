#pragma once
#include "../../IComponent.h"

// Race the Sun等の「自動前進オートランナー」向け操作コンポーネント。PlayerControllerComponentの
// WASD自由移動とは違い、前方(Z+)へ常時進み続け、左右キーではX軸方向にしか操作できない
// （Y/Zの直接操作はさせない。地面に沿わせたい場合はGravityComponentを別途アタッチする）
class AutoRunComponent : public IComponent {
public:
	bool  enabled = true;
	float forwardSpeed = 10.0f; // 1秒あたりの自動前進量
	float turnSpeed = 8.0f;     // 1秒あたりの左右移動量

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["forwardSpeed"] = forwardSpeed;
		out["turnSpeed"] = turnSpeed;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		forwardSpeed = in.value("forwardSpeed", forwardSpeed);
		turnSpeed = in.value("turnSpeed", turnSpeed);
	}
};
