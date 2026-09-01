#include "SceneTransitionComponent.h"
#include "SceneRegistry.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Externals/imgui/imgui.h"
#include <dinput.h>
#include <string>

namespace {
	// トリガーキーのコンボ用テーブル。0番目(なし)以外はdinput.hのDIK_*コードそのもの
	struct KeyOption {
		int code;
		const char* label;
	};
	constexpr KeyOption kKeyOptions[] = {
		{ 0, "なし" },
		{ DIK_ESCAPE, "ESC" },
		{ DIK_RETURN, "Enter" },
		{ DIK_SPACE, "Space" },
		{ DIK_F1, "F1" },
		{ DIK_F2, "F2" },
		{ DIK_F3, "F3" },
		{ DIK_F4, "F4" },
	};
	constexpr int kKeyOptionCount = sizeof(kKeyOptions) / sizeof(kKeyOptions[0]);

	const char* KeyLabelFor(int code) {
		for (const auto& opt : kKeyOptions) {
			if (opt.code == code) return opt.label;
		}
		return "?";
	}
}

void SceneTransitionComponent::DrawImGui(const char* namePrefix) {
	std::string enabledLabel = std::string(namePrefix) + "有効";
	std::string targetLabel = std::string(namePrefix) + "遷移先シーン";
	std::string keyLabel = std::string(namePrefix) + "トリガーキー";
	std::string useButtonLabel = std::string(namePrefix) + "ボタンクリックで遷移";
	std::string hitboxLabel = std::string(namePrefix) + "ヒットボックスのタグ";
	std::string textLabel = std::string(namePrefix) + "見た目反映先のタグ（任意）";

	ImGui::Checkbox(enabledLabel.c_str(), &enabled);

	// SceneRegistryに登録済みのシーン名から選ばせる（手打ちでの綴り間違いを防ぐ）
	if (ImGui::BeginCombo(targetLabel.c_str(), targetScene.empty() ? "(未選択)" : targetScene.c_str())) {
		for (const std::string& name : SceneRegistry::GetAllNames()) {
			bool selected = (targetScene == name);
			if (ImGui::Selectable(name.c_str(), selected)) targetScene = name;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	if (ImGui::BeginCombo(keyLabel.c_str(), KeyLabelFor(triggerKey))) {
		for (const auto& opt : kKeyOptions) {
			bool selected = (triggerKey == opt.code);
			if (ImGui::Selectable(opt.label, selected)) triggerKey = opt.code;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Checkbox(useButtonLabel.c_str(), &useButtonClick);
	if (useButtonClick) {
		char hitboxBuf[128];
		char textBuf[128];
		strncpy_s(hitboxBuf, hitboxTag.c_str(), sizeof(hitboxBuf) - 1);
		strncpy_s(textBuf, textTag.c_str(), sizeof(textBuf) - 1);
		if (ImGui::InputText(hitboxLabel.c_str(), hitboxBuf, sizeof(hitboxBuf))) hitboxTag = hitboxBuf;
		if (ImGui::InputText(textLabel.c_str(), textBuf, sizeof(textBuf))) textTag = textBuf;
		ImGui::TextDisabled("  (ヒットボックス側のGameObjectにOBBColliderComponent+PlayButtonComponentが必要)");
	}
}

void SceneTransitionComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["targetScene"] = targetScene;
	out["triggerKey"] = triggerKey;
	out["useButtonClick"] = useButtonClick;
	out["hitboxTag"] = hitboxTag;
	out["textTag"] = textTag;
}

void SceneTransitionComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	targetScene = in.value("targetScene", targetScene);
	triggerKey = in.value("triggerKey", triggerKey);
	useButtonClick = in.value("useButtonClick", useButtonClick);
	hitboxTag = in.value("hitboxTag", hitboxTag);
	textTag = in.value("textTag", textTag);
}

REGISTER_SIMPLE_COMPONENT(SceneTransitionComponent, "SceneTransition", "シーン遷移", "シーン");
