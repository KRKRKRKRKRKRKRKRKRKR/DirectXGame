#pragma once
#include "../../IComponent.h"

// WASDキー入力でTransform.translationをXZ平面上に動かす、最小の自機操作コンポーネント。
// GravityComponentと同じく「1つのGameObjectに閉じた振る舞い」のためIComponent派生として
// 実装する。地面に沿わせたい/重力を効かせたい場合はGravityComponentを別途アタッチする
// （移動と落下は互いに独立した別コンポーネントのまま組み合わせる）
class PlayerControllerComponent : public IComponent {
public:
	bool  enabled = true;
	float moveSpeed = 5.0f; // 1秒あたりの移動量

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["moveSpeed"] = moveSpeed;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		moveSpeed = in.value("moveSpeed", moveSpeed);
	}
};
