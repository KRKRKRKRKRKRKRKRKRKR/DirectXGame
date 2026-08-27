#include "RankingCameraScrollerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Math/EasingPreview.h"
#include <string>

void RankingCameraScrollerComponent::DrawImGui(const char* namePrefix) {
	std::string wheelSensitivityLabel = std::string(namePrefix) + "ホイール感度";
	ImGui::DragFloat(wheelSensitivityLabel.c_str(), &wheelSensitivity, 0.01f, 0.01f, 5.0f);

	std::string highlightColorLabel = std::string(namePrefix) + "自分の順位のハイライト色";
	ImGui::ColorEdit4(highlightColorLabel.c_str(), &highlightColor.x);

	ImGui::Separator();
	ImGui::Text("Titleボタン（カメラに追従）");
	std::string titleButtonXLabel = std::string(namePrefix) + "Titleボタンのカメラ相対Xオフセット";
	std::string titleButtonYLabel = std::string(namePrefix) + "Titleボタンのカメラ相対Yオフセット";
	std::string titleButtonZLabel = std::string(namePrefix) + "Titleボタンのカメラ相対Zオフセット";
	ImGui::DragFloat(titleButtonXLabel.c_str(), &titleButtonX, 0.05f, -50.0f, 50.0f);
	ImGui::DragFloat(titleButtonYLabel.c_str(), &titleButtonY, 0.05f, -50.0f, 50.0f);
	ImGui::DragFloat(titleButtonZLabel.c_str(), &titleButtonZ, 0.05f, -50.0f, 50.0f);

	ImGui::Separator();
	ImGui::Text("自動フォーカス演出（Clear画面から遷移時、1位から自分の順位までスクロール）");
	std::string autoFocusDurationLabel = std::string(namePrefix) + "演出時間(秒)";
	ImGui::DragFloat(autoFocusDurationLabel.c_str(), &autoFocusDuration, 0.05f, 0.1f, 10.0f);

	std::string autoFocusEasingLabel = std::string(namePrefix) + "演出のイージング";
	int easingIndex = static_cast<int>(autoFocusEasing);
	const char* const* easingNames = Easing::GetTypeNames();
	if (ImGui::BeginCombo(autoFocusEasingLabel.c_str(), easingNames[easingIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == easingIndex);
			if (ImGui::Selectable(easingNames[i], selected)) autoFocusEasing = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
			EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();
	std::string scrollLabel = std::string(namePrefix) + "スクロール量";
	ImGui::BeginDisabled();
	ImGui::DragFloat(scrollLabel.c_str(), &scrollOffset, 0.0f);
	ImGui::EndDisabled();
}

void RankingCameraScrollerComponent::ToJson(nlohmann::json& out) const {
	out["wheelSensitivity"] = wheelSensitivity;
	out["highlightColor"] = Vector4ToJson(highlightColor);
	out["titleButtonX"] = titleButtonX;
	out["titleButtonY"] = titleButtonY;
	out["titleButtonZ"] = titleButtonZ;
	out["autoFocusDuration"] = autoFocusDuration;
	out["autoFocusEasing"] = static_cast<int>(autoFocusEasing);
}

void RankingCameraScrollerComponent::FromJson(const nlohmann::json& in) {
	wheelSensitivity = in.value("wheelSensitivity", wheelSensitivity);
	if (in.contains("highlightColor")) highlightColor = Vector4FromJson(in["highlightColor"]);
	titleButtonX = in.value("titleButtonX", titleButtonX);
	titleButtonY = in.value("titleButtonY", titleButtonY);
	titleButtonZ = in.value("titleButtonZ", titleButtonZ);
	autoFocusDuration = in.value("autoFocusDuration", autoFocusDuration);
	autoFocusEasing = static_cast<Easing::Type>(in.value("autoFocusEasing", static_cast<int>(autoFocusEasing)));
	// scrollOffset/baseY等の実行時専用状態はあえて復元しない（ロード直後は必ず1位から）
}

REGISTER_SIMPLE_COMPONENT(RankingCameraScrollerComponent, "RankingCameraScroller", "ランキングカメラスクロール", "カメラ");
