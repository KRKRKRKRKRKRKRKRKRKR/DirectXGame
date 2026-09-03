// 10DaysJam
#include "GridWallComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <string>

void GridWallComponent::DrawImGui(const char* namePrefix) {
	std::string colLabel = std::string(namePrefix) + "配置マス 列";
	std::string rowLabel = std::string(namePrefix) + "配置マス 行";
	ImGui::DragInt(colLabel.c_str(), &col, 1, 0, 999);
	ImGui::DragInt(rowLabel.c_str(), &row, 1, 0, 999);

	std::string passCostLabel = std::string(namePrefix) + "通過コスト";
	ImGui::DragInt(passCostLabel.c_str(), &passCost, 1, 1, 999);

	std::string colorLabel = std::string(namePrefix) + "壁の色";
	ImGui::ColorEdit4(colorLabel.c_str(), &color.x);
}

void GridWallComponent::ToJson(nlohmann::json& out) const {
	out["col"] = col;
	out["row"] = row;
	out["passCost"] = passCost;
	out["color"] = Vector4ToJson(color);
}

void GridWallComponent::FromJson(const nlohmann::json& in) {
	col = in.value("col", col);
	row = in.value("row", row);
	passCost = in.value("passCost", passCost);
	if (in.contains("color")) color = Vector4FromJson(in["color"]);
}

REGISTER_SIMPLE_COMPONENT(GridWallComponent, "GridWall", "グリッド壁", "物理");
