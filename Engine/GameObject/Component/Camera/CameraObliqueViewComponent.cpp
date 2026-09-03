#include "CameraObliqueViewComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <DirectXMath.h>
#include <cmath>
#include <string>

void CameraObliqueViewComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)deltaTime;
	(void)ctx;
	if (!enabled) return;

	float pitchRad = DirectX::XMConvertToRadians(pitchDegrees);
	Vector3 back = { 0.0f, distance * sinf(pitchRad), -distance * cosf(pitchRad) };
	transform.translation = boxCenter + back;
	transform.rotation = { pitchRad, 0.0f, 0.0f };
}

void CameraObliqueViewComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "有効化";
	std::string pitchLabel  = std::string(namePrefix) + "見下ろし角（度）";
	std::string distLabel   = std::string(namePrefix) + "箱中心からの距離";
	std::string centerLabel = std::string(namePrefix) + "箱の中心座標";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::DragFloat(pitchLabel.c_str(), &pitchDegrees, 0.5f, 0.0f, 89.0f);
	ImGui::DragFloat(distLabel.c_str(), &distance, 0.1f, 0.1f, 1000.0f);
	ImGui::DragFloat3(centerLabel.c_str(), &boxCenter.x, 0.1f);
}

void CameraObliqueViewComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["pitchDegrees"] = pitchDegrees;
	out["distance"] = distance;
	out["boxCenter"] = Vector3ToJson(boxCenter);
}

void CameraObliqueViewComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	pitchDegrees = in.value("pitchDegrees", pitchDegrees);
	distance = in.value("distance", distance);
	if (in.contains("boxCenter")) boxCenter = Vector3FromJson(in["boxCenter"]);
}

REGISTER_SIMPLE_COMPONENT(CameraObliqueViewComponent, "CameraObliqueView", "斜め見下ろしカメラ", "カメラ");
