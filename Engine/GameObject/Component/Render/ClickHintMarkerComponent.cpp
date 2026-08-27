#include "ClickHintMarkerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/Easing.h"
#include "../../../Graphics/Pipeline/BlendMode.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr const char* kMarkerModelDirectory = "Resources/Model";
	constexpr const char* kMarkerModelFilename  = "Circle.obj";
}

void ClickHintMarkerComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)transform;
	(void)ctx;
	elapsed_ += deltaTime;
}

void ClickHintMarkerComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	if (!renderer) return;

	// Circle.objの読み込みは初回呼び出し時に1度だけ試みる（ReflexPlayerComponent::
	// DrawPlanningVisualizationと同じ理由：LoadModelはGPUリソースを新規確保するため
	// 毎フレーム呼ぶわけにはいかない）
	if (!tryLoadCircleModel_) {
		tryLoadCircleModel_ = true;
		circleModelHandle_ = renderer->LoadModel(kMarkerModelDirectory, kMarkerModelFilename);
		circleModelLoaded_ = true;
	}
	if (!circleModelLoaded_) return;

	float duration = (std::max)(pulseDuration, 0.0f);
	int waves = (std::max)(waveCount, 1);

	// waveCount本の波紋を、それぞれduration/waveCountぶん位相をずらして同じ地点に重ねて描く
	// （ReflexPlayerComponent::DrawPlanningVisualizationと同じ計算式）
	for (int wave = 0; wave < waves; wave++) {
		float t = 0.0f;
		if (duration > 0.0f) {
			float phaseOffset = duration * (static_cast<float>(wave) / static_cast<float>(waves));
			float waveElapsed = std::fmod(elapsed_ + phaseOffset, duration);
			t = waveElapsed / duration;
		}
		float eased = Easing::Apply(Easing::Type::kInOutSine, t);
		float scale = pulseMinScale + (pulseMaxScale - pulseMinScale) * eased;
		float alpha = color.w * (1.0f - eased);

		Transform markerTransform;
		markerTransform.translation = transform.translation;
		markerTransform.rotation = transform.rotation;
		markerTransform.scale = { scale, scale, scale };

		Vector4 fadedColor = { color.x, color.y, color.z, alpha };
		renderer->DrawModel(circleModelHandle_, markerTransform, fadedColor, {}, lighting, BlendMode::kNormal);
	}
}

void ClickHintMarkerComponent::DrawImGui(const char* namePrefix) {
	RenderComponentBase::DrawImGui(namePrefix);

	std::string minScaleLabel = std::string(namePrefix) + "波紋の最小スケール";
	std::string maxScaleLabel = std::string(namePrefix) + "波紋の最大スケール";
	std::string durationLabel = std::string(namePrefix) + "波紋の周期(秒)";
	std::string waveCountLabel = std::string(namePrefix) + "波紋の本数";
	ImGui::DragFloat(minScaleLabel.c_str(), &pulseMinScale, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(maxScaleLabel.c_str(), &pulseMaxScale, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(durationLabel.c_str(), &pulseDuration, 0.01f, 0.0f, 10.0f);
	ImGui::DragInt(waveCountLabel.c_str(), &waveCount, 1, 1, 10);
}

void ClickHintMarkerComponent::ToJson(nlohmann::json& out) const {
	RenderComponentBase::ToJson(out);
	out["pulseMinScale"] = pulseMinScale;
	out["pulseMaxScale"] = pulseMaxScale;
	out["pulseDuration"] = pulseDuration;
	out["waveCount"] = waveCount;
}

void ClickHintMarkerComponent::FromJson(const nlohmann::json& in) {
	RenderComponentBase::FromJson(in);
	pulseMinScale = in.value("pulseMinScale", pulseMinScale);
	pulseMaxScale = in.value("pulseMaxScale", pulseMaxScale);
	pulseDuration = in.value("pulseDuration", pulseDuration);
	waveCount = in.value("waveCount", waveCount);
}

REGISTER_SIMPLE_COMPONENT(ClickHintMarkerComponent, "ClickHintMarker", "クリック誘導マーカー", "描画");
