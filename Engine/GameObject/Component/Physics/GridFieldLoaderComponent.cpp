// 10DaysJam
#include "GridFieldLoaderComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../Utils/StringUtils.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <filesystem>

namespace {
	// GridPuzzleScene::ApplyManualFieldIfRequestedが読み込むパスと一致させること
	constexpr const char* kFieldFolder = "Resources/GridPuzzle/Field";
}

std::vector<std::string> GridFieldLoaderComponent::ScanAvailableFieldNames() {
	std::vector<std::string> names;
	namespace fs = std::filesystem;
	std::error_code ec;
	if (!fs::exists(kFieldFolder, ec)) return names;
	for (auto& entry : fs::directory_iterator(kFieldFolder, ec)) {
		if (ec || !entry.is_regular_file()) continue;
		if (entry.path().extension() != ".txt") continue;
		// path::string()はWindowsのANSIコードページ経由になり、日本語ファイル名が文字化けする
		// （このプロジェクトはUTF-8前提）。wstring()からCP_UTF8で変換することで正しく扱う
		names.push_back(StringUtils::ConvertString(entry.path().stem().wstring()));
	}
	std::sort(names.begin(), names.end());
	return names;
}

void GridFieldLoaderComponent::DrawImGui(const char* namePrefix) {
	std::vector<std::string> names = ScanAvailableFieldNames();

	std::string comboLabel = std::string(namePrefix) + "フィールドファイル";
	const char* previewValue = selectedFieldName.empty() ? "(未選択)" : selectedFieldName.c_str();
	if (ImGui::BeginCombo(comboLabel.c_str(), previewValue)) {
		for (const auto& name : names) {
			bool selected = (name == selectedFieldName);
			if (ImGui::Selectable(name.c_str(), selected)) selectedFieldName = name;
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if (names.empty()) {
		ImGui::TextDisabled("(Resources/GridPuzzle/Field/ に.txtファイルがありません)");
	}

	std::string loadLabel = std::string(namePrefix) + "読み込む(現在のアイテム・壁を置き換える)";
	ImGui::BeginDisabled(selectedFieldName.empty());
	if (ImGui::Button(loadLabel.c_str())) loadRequested_ = true;
	ImGui::EndDisabled();
}

void GridFieldLoaderComponent::ToJson(nlohmann::json& out) const {
	out["selectedFieldName"] = selectedFieldName;
}

void GridFieldLoaderComponent::FromJson(const nlohmann::json& in) {
	selectedFieldName = in.value("selectedFieldName", selectedFieldName);
}

REGISTER_SIMPLE_COMPONENT(GridFieldLoaderComponent, "GridFieldLoader", "グリッド手動配置ローダー", "物理");
