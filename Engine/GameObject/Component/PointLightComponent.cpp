#include "PointLightComponent.h"
#include "../../Graphics/Renderer/Renderer.h"
#include "../../../Externals/imgui/imgui.h"
#include <string>

void PointLightComponent::SyncToRenderer(Renderer* renderer, const Transform& transform) const {
	auto& light = renderer->GetLight();
	light.SetEnablePointLight(enabled);
	light.SetPointPosition(transform.translation);
	light.SetPointColor(color);
	light.SetPointIntensity(intensity);
	light.SetPointRadius(radius);
	light.SetPointDecay(decay);
}

void PointLightComponent::DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const {
	(void)view; (void)proj; // ラインは描かないため未使用（他のDrawGizmoVisualizationとシグネチャを揃えるために受け取る）
	if (!enabled) return;

	Transform sphere;
	sphere.translation = transform.translation;
	sphere.scale = { 0.3f, 0.3f, 0.3f };
	renderer->DrawSphere(sphere, { color.x, color.y, color.z, 1.0f }, kTextureNone, false);
}

void PointLightComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel    = std::string(namePrefix) + " Enable";
	std::string colorLabel     = std::string(namePrefix) + " Color";
	std::string intensityLabel = std::string(namePrefix) + " Intensity";
	std::string radiusLabel    = std::string(namePrefix) + " Radius";
	std::string decayLabel     = std::string(namePrefix) + " Decay";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::ColorEdit3(colorLabel.c_str(), &color.x);
	ImGui::SliderFloat(intensityLabel.c_str(), &intensity, 0.0f, 5.0f);
	ImGui::SliderFloat(radiusLabel.c_str(), &radius, 0.1f, 30.0f);
	ImGui::SliderFloat(decayLabel.c_str(), &decay, 0.1f, 4.0f);
}
