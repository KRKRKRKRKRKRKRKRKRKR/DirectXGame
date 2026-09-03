// 10DaysJam
#include "GridItemComponent.h"
#include "GridBoardPlayerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../GameObject.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <string>

namespace {
	constexpr const char* kTypeNames[] = { "攻撃力+1", "コスト+2固定", "コスト±4リスキー" };
}

void GridItemComponent::OnTriggerEnter(GameObject& other) {
	if (triggered) return; // 既に発動済み（削除待ち）なら二重発動しない

	auto* playerMove = other.GetComponent<GridBoardPlayerComponent>();
	if (!playerMove) return; // アイテム同士やその他のTrigger相手には反応しない

	playerMove->ApplyItemEffect(type);
	triggered = true;
}

void GridItemComponent::DrawImGui(const char* namePrefix) {
	std::string typeLabel = std::string(namePrefix) + "アイテム種別";
	int typeIndex = static_cast<int>(type);
	if (ImGui::BeginCombo(typeLabel.c_str(), kTypeNames[typeIndex])) {
		for (int i = 0; i < static_cast<int>(sizeof(kTypeNames) / sizeof(kTypeNames[0])); i++) {
			bool selected = (i == typeIndex);
			if (ImGui::Selectable(kTypeNames[i], selected)) type = static_cast<Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	std::string colLabel = std::string(namePrefix) + "配置マス 列";
	std::string rowLabel = std::string(namePrefix) + "配置マス 行";
	ImGui::DragInt(colLabel.c_str(), &col, 1, 0, 999);
	ImGui::DragInt(rowLabel.c_str(), &row, 1, 0, 999);

	std::string colorLabel = std::string(namePrefix) + "アイテムの色";
	ImGui::ColorEdit4(colorLabel.c_str(), &color.x);
}

void GridItemComponent::ToJson(nlohmann::json& out) const {
	out["type"] = static_cast<int>(type);
	out["col"] = col;
	out["row"] = row;
	out["color"] = Vector4ToJson(color);
}

void GridItemComponent::FromJson(const nlohmann::json& in) {
	type = static_cast<Type>(in.value("type", static_cast<int>(type)));
	col = in.value("col", col);
	row = in.value("row", row);
	if (in.contains("color")) color = Vector4FromJson(in["color"]);
}

REGISTER_SIMPLE_COMPONENT(GridItemComponent, "GridItem", "グリッドアイテム", "物理");
