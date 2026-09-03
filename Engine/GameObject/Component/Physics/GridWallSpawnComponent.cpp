// 10DaysJam
#include "GridWallSpawnComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <algorithm>
#include <string>

void GridWallSpawnComponent::DrawImGui(const char* namePrefix) {
	std::string countLabel = std::string(namePrefix) + "壁ブロックの数(テトロミノ1個=4マス)";
	ImGui::DragInt(countLabel.c_str(), &pieceCount, 1.0f, 0, 999);
	pieceCount = (std::max)(pieceCount, 0);

	std::string passCostLabel = std::string(namePrefix) + "通過コスト(初期値)";
	ImGui::DragInt(passCostLabel.c_str(), &passCost, 1, 1, 999);

	std::string colorLabel = std::string(namePrefix) + "壁の色(初期値)";
	ImGui::ColorEdit4(colorLabel.c_str(), &wallColor.x);

	ImGui::Separator();
	std::string resetLabel = std::string(namePrefix) + "リセット(全ての壁を削除して再配置)";
	if (ImGui::Button(resetLabel.c_str())) resetRequested_ = true;
}

void GridWallSpawnComponent::ToJson(nlohmann::json& out) const {
	out["pieceCount"] = pieceCount;
	out["passCost"] = passCost;
	out["wallColor"] = Vector4ToJson(wallColor);
}

void GridWallSpawnComponent::FromJson(const nlohmann::json& in) {
	pieceCount = in.value("pieceCount", pieceCount);
	passCost = in.value("passCost", passCost);
	if (in.contains("wallColor")) wallColor = Vector4FromJson(in["wallColor"]);
}

REGISTER_SIMPLE_COMPONENT(GridWallSpawnComponent, "GridWallSpawn", "グリッド壁スポーン", "物理");
