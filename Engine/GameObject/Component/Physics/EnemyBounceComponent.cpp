#include "EnemyBounceComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

namespace {
	// 1軸ぶんの範囲チェック＋反射。3軸とも同じ処理なのでヘルパーにまとめる
	void BounceAxis(float& position, float& velocity, float center, float halfExtent) {
		if (position > center + halfExtent) {
			position = center + halfExtent;
			velocity = -velocity;
		} else if (position < center - halfExtent) {
			position = center - halfExtent;
			velocity = -velocity;
		}
	}
}

void EnemyBounceComponent::Update(float deltaTime, Transform& transform) {
	if (!enabled) return;

	transform.translation = transform.translation + velocity * deltaTime;

	BounceAxis(transform.translation.x, velocity.x, boundsCenter.x, boundsHalfExtent.x);
	BounceAxis(transform.translation.y, velocity.y, boundsCenter.y, boundsHalfExtent.y);
	BounceAxis(transform.translation.z, velocity.z, boundsCenter.z, boundsHalfExtent.z);
}

void EnemyBounceComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "跳ね回りを有効化";
	std::string velocityLabel = std::string(namePrefix) + "移動速度";
	std::string centerLabel = std::string(namePrefix) + "境界の中心";
	std::string extentLabel = std::string(namePrefix) + "境界の半径サイズ";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::DragFloat3(velocityLabel.c_str(), &velocity.x, 0.1f);
	ImGui::DragFloat3(centerLabel.c_str(), &boundsCenter.x, 0.1f);
	ImGui::DragFloat3(extentLabel.c_str(), &boundsHalfExtent.x, 0.1f, 0.01f, 100.0f);
}

void EnemyBounceComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["velocity"] = Vector3ToJson(velocity);
	out["boundsCenter"] = Vector3ToJson(boundsCenter);
	out["boundsHalfExtent"] = Vector3ToJson(boundsHalfExtent);
}

void EnemyBounceComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	if (in.contains("velocity")) velocity = Vector3FromJson(in["velocity"]);
	if (in.contains("boundsCenter")) boundsCenter = Vector3FromJson(in["boundsCenter"]);
	if (in.contains("boundsHalfExtent")) boundsHalfExtent = Vector3FromJson(in["boundsHalfExtent"]);
}

REGISTER_SIMPLE_COMPONENT(EnemyBounceComponent, "EnemyBounce", "敵の跳ね回り", "物理");
