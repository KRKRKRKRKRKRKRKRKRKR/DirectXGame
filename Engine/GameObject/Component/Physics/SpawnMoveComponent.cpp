#include "SpawnMoveComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/EaseUtil.h"
#include "../../../../Math/EasingPreview.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void SpawnMoveComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (finished) return;

	elapsed += deltaTime;
	if (elapsed >= duration) {
		elapsed = duration;
		finished = true;
	}

	float t = EaseUtil::Clamp01(duration > 0.0f ? elapsed / duration : 1.0f);
	float easedT = Easing::Apply(easing, t);
	transform.translation = EaseUtil::Lerp(startPos, targetPos, easedT);
}

void SpawnMoveComponent::DrawImGui(const char* namePrefix) {
	std::string zOffsetLabel = std::string(namePrefix) + "開始位置のZオフセット";
	ImGui::DragFloat(zOffsetLabel.c_str(), &zOffset, 0.1f, -100.0f, 100.0f);

	std::string durationLabel = std::string(namePrefix) + "移動時間(秒)";
	ImGui::DragFloat(durationLabel.c_str(), &duration, 0.05f, 0.05f, 10.0f);

	// 移動に適用するイージング（https://easings.net/ja 準拠）をコンボで選択する
	// （ParticleEmitterComponentのサイズイージング選択と同じ形式）
	std::string easingLabel = std::string(namePrefix) + "イージング";
	int easingIndex = static_cast<int>(easing);
	const char* const* easingNames = Easing::GetTypeNames();
	if (ImGui::BeginCombo(easingLabel.c_str(), easingNames[easingIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == easingIndex);
			if (ImGui::Selectable(easingNames[i], selected)) easing = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
			EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
		}
		ImGui::EndCombo();
	}

	// 実行中のみ意味を持つ内部状態は参考表示のみ（値の編集はさせない）
	std::string elapsedLabel = std::string(namePrefix) + "経過時間";
	ImGui::BeginDisabled();
	ImGui::DragFloat(elapsedLabel.c_str(), &elapsed, 0.0f, 0.0f, duration);
	ImGui::EndDisabled();
}

REGISTER_SIMPLE_COMPONENT(SpawnMoveComponent, "SpawnMove", "スポーン移動演出", "物理");
