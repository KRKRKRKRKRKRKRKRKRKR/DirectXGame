// 10DaysJam
#include "GridWallSpawnComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <algorithm>
#include <string>

void GridWallSpawnComponent::DrawImGui(const char* namePrefix) {
	// 壁は固定パターン（GridPuzzleScene::FixedWallPatternVariants、7x7盤面専用の手動デザイン
	// 複数パターンを回転・反転したバリエーションからランダム選択）を配置するため、個数調整UIは
	// 表示しない（pieceCountは現在参照されていない）

	// passCostはimpassable=trueの間は参照されないため、無効化してユーザーに伝える
	ImGui::BeginDisabled(impassable);
	std::string passCostLabel = std::string(namePrefix) + "通過コスト(初期値)";
	ImGui::DragInt(passCostLabel.c_str(), &passCost, 1, 1, 999);
	ImGui::EndDisabled();

	std::string colorLabel = std::string(namePrefix) + "壁の色(初期値)";
	ImGui::ColorEdit4(colorLabel.c_str(), &wallColor.x);

	std::string impassableLabel = std::string(namePrefix) + "新規生成する壁を通行不可にする";
	ImGui::Checkbox(impassableLabel.c_str(), &impassable);

	std::string toggleLabel = std::string(namePrefix) + (impassable
		? "現在の壁を全て「超えられない壁」に切り替え"
		: "現在の壁を全て「超えられる壁」に切り替え");
	if (ImGui::Button(toggleLabel.c_str())) impassableToggleRequested_ = true;

	ImGui::Separator();
	std::string resetLabel = std::string(namePrefix) + "リセット(全ての壁を削除して再配置)";
	if (ImGui::Button(resetLabel.c_str())) resetRequested_ = true;
}

void GridWallSpawnComponent::ToJson(nlohmann::json& out) const {
	out["pieceCount"] = pieceCount;
	out["passCost"] = passCost;
	out["impassable"] = impassable;
	out["wallColor"] = Vector4ToJson(wallColor);
}

void GridWallSpawnComponent::FromJson(const nlohmann::json& in) {
	pieceCount = in.value("pieceCount", pieceCount);
	passCost = in.value("passCost", passCost);
	impassable = in.value("impassable", impassable);
	if (in.contains("wallColor")) wallColor = Vector4FromJson(in["wallColor"]);
}

REGISTER_SIMPLE_COMPONENT(GridWallSpawnComponent, "GridWallSpawn", "グリッド壁スポーン", "物理");
