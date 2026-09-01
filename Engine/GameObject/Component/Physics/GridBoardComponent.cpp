// 10DaysJam
#include "GridBoardComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <string>

void GridBoardComponent::DrawImGui(const char* namePrefix) {
	std::string columnsLabel = std::string(namePrefix) + "盤面の列数(横)";
	std::string rowsLabel = std::string(namePrefix) + "盤面の行数(縦)";
	ImGui::DragInt(columnsLabel.c_str(), &columns, 1.0f, 1, 50);
	ImGui::DragInt(rowsLabel.c_str(), &rows, 1.0f, 1, 50);
	columns = (std::max)(columns, 1);
	rows = (std::max)(rows, 1);

	std::string colorALabel = std::string(namePrefix) + "タイルの色A";
	std::string colorBLabel = std::string(namePrefix) + "タイルの色B";
	ImGui::ColorEdit4(colorALabel.c_str(), &tileColorA.x);
	ImGui::ColorEdit4(colorBLabel.c_str(), &tileColorB.x);
}

void GridBoardComponent::ToJson(nlohmann::json& out) const {
	out["columns"] = columns;
	out["rows"] = rows;
	out["tileColorA"] = Vector4ToJson(tileColorA);
	out["tileColorB"] = Vector4ToJson(tileColorB);
}

void GridBoardComponent::FromJson(const nlohmann::json& in) {
	columns = in.value("columns", columns);
	rows = in.value("rows", rows);
	if (in.contains("tileColorA")) tileColorA = Vector4FromJson(in["tileColorA"]);
	if (in.contains("tileColorB")) tileColorB = Vector4FromJson(in["tileColorB"]);
}

REGISTER_SIMPLE_COMPONENT(GridBoardComponent, "GridBoard", "グリッド盤面サイズ", "物理");
