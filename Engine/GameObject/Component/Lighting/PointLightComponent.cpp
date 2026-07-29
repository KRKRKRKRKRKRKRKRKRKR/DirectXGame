#include "PointLightComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/Lighting/SceneLight.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void PointLightComponent::SyncToRenderer(Renderer* renderer, const Transform& transform, uint32_t slotIndex) const {
	lastSlotIndex_ = slotIndex;
	auto& light = renderer->GetLight();
	light.SetPointLight(slotIndex, enabled, transform.translation, color, intensity, radius, decay);
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
	std::string enableLabel    = std::string(namePrefix) + "有効化";
	std::string colorLabel     = std::string(namePrefix) + "色";
	std::string intensityLabel = std::string(namePrefix) + "強さ";
	std::string radiusLabel    = std::string(namePrefix) + "半径";
	std::string decayLabel     = std::string(namePrefix) + "減衰";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::ColorEdit3(colorLabel.c_str(), &color.x);
	ImGui::SliderFloat(intensityLabel.c_str(), &intensity, 0.0f, 5.0f);
	ImGui::SliderFloat(radiusLabel.c_str(), &radius, 0.1f, 30.0f);
	ImGui::SliderFloat(decayLabel.c_str(), &decay, 0.1f, 4.0f);

	// シーン全体で同時に効く点光源は最大SceneLight::LightData::kMaxPointLights個までという
	// 固定長配列の上限があるため、常に上限を明記しつつ、このインスタンスが実際に上限を
	// 超えて無効化されている場合は警告を出す（lastSlotIndex_は直近のSyncToRendererでキャッシュ済み）
	ImGui::TextDisabled("(シーン全体で同時に有効な点光源は最大%u個まで)", SceneLight::LightData::kMaxPointLights);
	if (enabled && lastSlotIndex_ >= SceneLight::LightData::kMaxPointLights) {
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
			"上限を超えているため、この光源は反映されていません（%u個目）", lastSlotIndex_ + 1);
	}
}

REGISTER_SIMPLE_COMPONENT(PointLightComponent, "PointLight", "点光源", "ライティング");
