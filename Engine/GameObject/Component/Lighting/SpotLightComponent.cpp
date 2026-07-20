#include "SpotLightComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../../Math/TransformMath.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void SpotLightComponent::SyncToRenderer(Renderer* renderer, const Transform& transform) const {
	auto& light = renderer->GetLight();
	light.SetEnableSpotLight(enabled);
	light.SetSpotPosition(transform.translation);
	light.SetSpotDirection(TransformMath::EulerRadiansToDirection(transform.rotation));
	light.SetSpotColor(color);
	light.SetSpotIntensity(intensity);
	light.SetSpotDistance(distance);
	light.SetSpotDecay(decay);
	light.SetSpotConeAngles(cosAngle, cosFalloffStart);
}

void SpotLightComponent::DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const {
	if (!enabled) return;

	Transform sphere;
	sphere.translation = transform.translation;
	sphere.scale = { 0.3f, 0.3f, 0.3f };
	renderer->DrawSphere(sphere, { color.x, color.y, color.z, 1.0f }, kTextureNone, false);

	Vector3 direction = TransformMath::EulerRadiansToDirection(transform.rotation);
	Vector3 tip = transform.translation + direction * 3.0f;
	renderer->DrawLine(transform.translation, tip, { color.x, color.y, color.z, 1.0f }, view, proj);
}

void SpotLightComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel    = std::string(namePrefix) + "有効化";
	std::string colorLabel     = std::string(namePrefix) + "色";
	std::string intensityLabel = std::string(namePrefix) + "強さ";
	std::string distanceLabel  = std::string(namePrefix) + "届く距離";
	std::string decayLabel     = std::string(namePrefix) + "減衰";
	std::string cosAngleLabel  = std::string(namePrefix) + "照射角（外側）";
	std::string cosFalloffLabel = std::string(namePrefix) + "減衰開始角（内側）";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::ColorEdit3(colorLabel.c_str(), &color.x);
	ImGui::SliderFloat(intensityLabel.c_str(), &intensity, 0.0f, 5.0f);
	ImGui::SliderFloat(distanceLabel.c_str(), &distance, 0.1f, 30.0f);
	ImGui::SliderFloat(decayLabel.c_str(), &decay, 0.1f, 4.0f);
	ImGui::SliderFloat(cosAngleLabel.c_str(), &cosAngle, 0.0f, 0.999f);
	ImGui::SliderFloat(cosFalloffLabel.c_str(), &cosFalloffStart, 0.0f, 0.999f);
}

REGISTER_SIMPLE_COMPONENT(SpotLightComponent, "SpotLight", "スポットライト", "ライティング");
