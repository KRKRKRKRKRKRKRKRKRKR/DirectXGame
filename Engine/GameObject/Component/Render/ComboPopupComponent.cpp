#include "ComboPopupComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/EasingPreview.h"

void ComboPopupComponent::DrawImGui(const char* namePrefix) {
	std::string baseYLabel = std::string(namePrefix) + "頭上オフセット";
	std::string charScaleLabel = std::string(namePrefix) + "数字サイズ";
	std::string digitSpacingLabel = std::string(namePrefix) + "数字間隔（2桁以上のとき）";
	std::string popInLabel = std::string(namePrefix) + "ポップイン時間（秒）";
	std::string holdLabel = std::string(namePrefix) + "静止表示時間（秒・この間に次のコンボが無いと自動で消える）";
	std::string fadeLabel = std::string(namePrefix) + "フェードアウト時間（秒）";

	ImGui::DragFloat(baseYLabel.c_str(), &baseYOffset, 0.05f, 0.0f, 10.0f);
	ImGui::DragFloat(charScaleLabel.c_str(), &charScale, 0.02f, 0.05f, 10.0f);
	ImGui::DragFloat(digitSpacingLabel.c_str(), &digitSpacing, 0.02f, 0.05f, 10.0f);
	ImGui::DragFloat(popInLabel.c_str(), &popInDuration, 0.01f, 0.01f, 3.0f);
	ImGui::DragFloat(holdLabel.c_str(), &holdDuration, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(fadeLabel.c_str(), &fadeOutDuration, 0.01f, 0.01f, 3.0f);

	std::string popInEasingLabel = std::string(namePrefix) + "ポップインのイージング";
	std::string fadeEasingLabel = std::string(namePrefix) + "フェードアウトのイージング";
	const char* const* easingNames = Easing::GetTypeNames();

	int popInIndex = static_cast<int>(popInEasing);
	if (ImGui::BeginCombo(popInEasingLabel.c_str(), easingNames[popInIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == popInIndex);
			if (ImGui::Selectable(easingNames[i], selected)) popInEasing = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
			EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
		}
		ImGui::EndCombo();
	}

	int fadeIndex = static_cast<int>(fadeOutEasing);
	if (ImGui::BeginCombo(fadeEasingLabel.c_str(), easingNames[fadeIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == fadeIndex);
			if (ImGui::Selectable(easingNames[i], selected)) fadeOutEasing = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
			EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
		}
		ImGui::EndCombo();
	}

	std::string activeLabel = std::string(namePrefix) + (activePopup_.modelObject ? "表示中: " + std::to_string(activePopup_.comboValue) : "非表示");
	ImGui::Text("%s", activeLabel.c_str());
}

void ComboPopupComponent::ToJson(nlohmann::json& out) const {
	out["baseYOffset"] = baseYOffset;
	out["charScale"] = charScale;
	out["digitSpacing"] = digitSpacing;
	out["popInDuration"] = popInDuration;
	out["popInEasing"] = static_cast<int>(popInEasing);
	out["holdDuration"] = holdDuration;
	out["fadeOutDuration"] = fadeOutDuration;
	out["fadeOutEasing"] = static_cast<int>(fadeOutEasing);
}

void ComboPopupComponent::FromJson(const nlohmann::json& in) {
	baseYOffset = in.value("baseYOffset", baseYOffset);
	charScale = in.value("charScale", charScale);
	digitSpacing = in.value("digitSpacing", digitSpacing);
	popInDuration = in.value("popInDuration", popInDuration);
	popInEasing = static_cast<Easing::Type>(in.value("popInEasing", static_cast<int>(popInEasing)));
	holdDuration = in.value("holdDuration", holdDuration);
	fadeOutDuration = in.value("fadeOutDuration", fadeOutDuration);
	fadeOutEasing = static_cast<Easing::Type>(in.value("fadeOutEasing", static_cast<int>(fadeOutEasing)));
	// activePopup_/pendingComboValue_はあえて復元しない（実行時の一時状態のため）
}

REGISTER_SIMPLE_COMPONENT(ComboPopupComponent, "ComboPopup", "コンボポップアップ", "物理");
