// 10DaysJam
#include "GridBoardComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

void GridBoardComponent::WorldToNearestGrid(const Vector3& worldPos, int& outCol, int& outRow) const {
	outCol = static_cast<int>(std::lround(worldPos.x / cellSpacing));
	outRow = static_cast<int>(std::lround(-worldPos.z / cellSpacing));
}

void GridBoardComponent::DrawImGui(const char* namePrefix) {
	// 盤面サイズは7x7固定（企画上の決定）のため、Inspectorからは変更できない読み取り専用表示にする
	ImGui::Text("%s", (std::string(namePrefix) + "盤面サイズ: " + std::to_string(columns) + " x " + std::to_string(rows) + " (固定)").c_str());

	std::string spacingLabel = std::string(namePrefix) + "マス間隔";
	ImGui::DragFloat(spacingLabel.c_str(), &cellSpacing, 0.05f, 0.1f, 10.0f);

	std::string colorALabel = std::string(namePrefix) + "タイルの色A";
	std::string colorBLabel = std::string(namePrefix) + "タイルの色B";
	ImGui::ColorEdit4(colorALabel.c_str(), &tileColorA.x);
	ImGui::ColorEdit4(colorBLabel.c_str(), &tileColorB.x);
}

void GridBoardComponent::ToJson(nlohmann::json& out) const {
	out["columns"] = columns;
	out["rows"] = rows;
	out["cellSpacing"] = cellSpacing;
	out["tileColorA"] = Vector4ToJson(tileColorA);
	out["tileColorB"] = Vector4ToJson(tileColorB);
}

void GridBoardComponent::FromJson(const nlohmann::json& in) {
	// 盤面サイズは7x7固定（企画上の決定）。古いscene.jsonに別サイズが保存されていても
	// 読み込み時に強制的に矯正する（保存値は読まない）
	columns = kFixedColumns;
	rows = kFixedRows;
	cellSpacing = in.value("cellSpacing", cellSpacing);
	if (in.contains("tileColorA")) tileColorA = Vector4FromJson(in["tileColorA"]);
	if (in.contains("tileColorB")) tileColorB = Vector4FromJson(in["tileColorB"]);
}

REGISTER_SIMPLE_COMPONENT(GridBoardComponent, "GridBoard", "グリッド盤面サイズ", "物理");
