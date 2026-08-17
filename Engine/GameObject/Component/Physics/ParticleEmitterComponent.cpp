#include "ParticleEmitterComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/EasingPreview.h"
#include <algorithm>
#include <string>

void ParticleEmitterComponent::DrawImGui(const char* namePrefix) {
	std::string countLabel = std::string(namePrefix) + "個数";
	ImGui::DragInt(countLabel.c_str(), &count, 1, 1, 200);
	count = (std::max)(1, count);

	std::string sizeStartLabel = std::string(namePrefix) + "初期サイズ(min/max)";
	ImGui::DragFloat2(sizeStartLabel.c_str(), &sizeStartMin, 0.01f, 0.0f, 10.0f);
	sizeStartMax = (std::max)(sizeStartMin, sizeStartMax);

	std::string sizeEndLabel = std::string(namePrefix) + "終了サイズ(min/max)";
	ImGui::DragFloat2(sizeEndLabel.c_str(), &sizeEndMin, 0.01f, 0.0f, 10.0f);
	sizeEndMax = (std::max)(sizeEndMin, sizeEndMax);

	std::string speedLabel = std::string(namePrefix) + "速度(min/max)";
	ImGui::DragFloat2(speedLabel.c_str(), &speedMin, 0.1f, 0.0f, 100.0f);
	speedMax = (std::max)(speedMin, speedMax);

	std::string lifeTimeLabel = std::string(namePrefix) + "寿命(秒)";
	ImGui::DragFloat(lifeTimeLabel.c_str(), &lifeTime, 0.05f, 0.05f, 10.0f);

	// サイズ変化に適用するイージング（https://easings.net/ja 準拠）をコンボで選択する
	// （ReflexPlayerComponentの実行フェーズ移動イージング選択と同じ形式）
	std::string easingLabel = std::string(namePrefix) + "サイズのイージング";
	int easingIndex = static_cast<int>(sizeEasing);
	const char* const* easingNames = Easing::GetTypeNames();
	if (ImGui::BeginCombo(easingLabel.c_str(), easingNames[easingIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == easingIndex);
			if (ImGui::Selectable(easingNames[i], selected)) sizeEasing = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
			EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
		}
		ImGui::EndCombo();
	}

	std::string rotationLabel = std::string(namePrefix) + "回転させる";
	std::string rotationMinLabel = std::string(namePrefix) + "回転速度(min/max)(度/秒)";
	ImGui::Separator();
	ImGui::Checkbox(rotationLabel.c_str(), &enableRotation);
	if (enableRotation) {
		ImGui::DragFloat2(rotationMinLabel.c_str(), &rotationSpeedMin, 1.0f, 0.0f, 1000.0f);
		rotationSpeedMax = (std::max)(rotationSpeedMin, rotationSpeedMax);
	}
}

REGISTER_SIMPLE_COMPONENT(ParticleEmitterComponent, "ParticleEmitter", "パーティクル発生設定", "物理");
