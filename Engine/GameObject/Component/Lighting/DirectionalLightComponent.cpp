#include "DirectionalLightComponent.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../../Math/TransformMath.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>
#include <cmath>

void DirectionalLightComponent::SyncToRenderer(Renderer* renderer, const Transform& transform) const {
	auto& light = renderer->GetLight();
	light.SetEnableDirectional(enabled);
	light.SetColor(color);
	light.SetAmbient(ambient);
	light.SetHalfLambertPower(halfLambertPower);
	light.SetDirection(TransformMath::EulerRadiansToDirection(transform.rotation));
}

void DirectionalLightComponent::DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const {
	Vector3 direction = TransformMath::EulerRadiansToDirection(transform.rotation);

	Transform sphere;
	sphere.translation = transform.translation;
	sphere.scale = { 0.5f, 0.5f, 0.5f };
	renderer->DrawSphere(sphere, { color.x, color.y, color.z, 1.0f }, kTextureNone, false);

	Vector3 tip = transform.translation + direction * 15.0f;
	renderer->DrawLine(transform.translation, tip, { color.x, color.y, color.z, 1.0f }, view, proj);
}

void DirectionalLightComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel  = std::string(namePrefix) + "有効化";
	std::string colorLabel   = std::string(namePrefix) + "色";
	std::string ambientLabel = std::string(namePrefix) + "環境光の強さ";
	std::string halfLambertLabel = std::string(namePrefix) + "ハーフランバートの強さ";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::ColorEdit3(colorLabel.c_str(), &color.x);
	ImGui::SliderFloat(ambientLabel.c_str(), &ambient, 0.0f, 1.0f);
	ImGui::SliderFloat(halfLambertLabel.c_str(), &halfLambertPower, 0.1f, 8.0f);
}
