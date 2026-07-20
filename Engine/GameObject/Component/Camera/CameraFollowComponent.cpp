#include "CameraFollowComponent.h"
#include "../../GameObject.h"
#include "../../ComponentRegistry.h"
#include "../../ObjectArchetypes.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <cmath>
#include <string>

void CameraFollowComponent::Update(float deltaTime, Transform& transform) {
	if (!enabled || !target) return;

	Vector3 targetPos = target->GetWorldTransform().translation;
	Vector3 desired = targetPos + offset;

	if (followLerp <= 0.0f) {
		transform.translation = desired;
	} else {
		// フレームレートに依存しない指数減衰補間：deltaTimeが変動しても収束の速さの体感が変わらない
		float t = 1.0f - std::exp(-followLerp * deltaTime);
		transform.translation = transform.translation + (desired - transform.translation) * t;
	}

	if (lookAtTarget) {
		Vector3 toTarget = targetPos - transform.translation;
		float lenSq = toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
		if (lenSq > 1e-6f) {
			transform.rotation = ObjectArchetypes::DirectionToEulerRadians(toTarget);
		}
	}
}

void CameraFollowComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "有効化";
	std::string offsetLabel = std::string(namePrefix) + "オフセット";
	std::string lerpLabel   = std::string(namePrefix) + "追従の滑らかさ（0でラグなし）";
	std::string lookLabel   = std::string(namePrefix) + "ターゲットの方を向く";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::DragFloat3(offsetLabel.c_str(), &offset.x, 0.1f);
	ImGui::DragFloat(lerpLabel.c_str(), &followLerp, 0.1f, 0.0f, 30.0f);
	ImGui::Checkbox(lookLabel.c_str(), &lookAtTarget);

	if (!target) {
		ImGui::TextDisabled("(追従対象なし：AutoRunComponentを持つオブジェクトが必要です)");
	}
}

void CameraFollowComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["offset"] = Vector3ToJson(offset);
	out["followLerp"] = followLerp;
	out["lookAtTarget"] = lookAtTarget;
}

void CameraFollowComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	if (in.contains("offset")) offset = Vector3FromJson(in["offset"]);
	followLerp = in.value("followLerp", followLerp);
	lookAtTarget = in.value("lookAtTarget", lookAtTarget);
}

REGISTER_SIMPLE_COMPONENT(CameraFollowComponent, "CameraFollow", "カメラ自動追尾", "カメラ");
