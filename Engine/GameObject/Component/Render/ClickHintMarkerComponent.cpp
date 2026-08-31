#include "ClickHintMarkerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/PulseWave.h"
#include "../../../Graphics/Pipeline/BlendMode.h"
#include <algorithm>

void ClickHintMarkerComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)transform;
	(void)ctx;
	elapsed_ += deltaTime;
}

void ClickHintMarkerComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	if (!renderer) return;

	Renderer::ModelHandle circleModelHandle = circleModel_.Get(renderer);
	if (!circleModelHandle) return;

	PulseWaveParams params;
	params.minScale = pulseMinScale;
	params.maxScale = pulseMaxScale;
	params.duration = pulseDuration;
	params.waveCount = waveCount;

	// waveCount本の波紋を、それぞれduration/waveCountぶん位相をずらして同じ地点に重ねて描く
	// （ReflexPlayerComponent::DrawPlanningVisualizationと同じ計算式。PulseWave.hに切り出し済み）
	for (int wave = 0; wave < (std::max)(waveCount, 1); wave++) {
		PulseWaveSample sample = SamplePulseWave(params, elapsed_, wave);

		Transform markerTransform;
		markerTransform.translation = transform.translation;
		markerTransform.rotation = transform.rotation;
		markerTransform.scale = { sample.scale, sample.scale, sample.scale };

		Vector4 fadedColor = { color.x, color.y, color.z, color.w * sample.alphaMultiplier };
		renderer->DrawModel(circleModelHandle, markerTransform, fadedColor, {}, lighting, BlendMode::kNormal);
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
