// 10DaysJam
#include "GridItemSpawnComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include <algorithm>
#include <string>

namespace {
	constexpr const char* kTypeNames[] = { "攻撃力+1", "コスト+2固定", "コスト±4リスキー" };
	constexpr int kTypeCount = static_cast<int>(sizeof(kTypeNames) / sizeof(kTypeNames[0]));
}

void GridItemSpawnComponent::DrawImGui(const char* namePrefix) {
	std::string headerLabel = std::string(namePrefix) + "アイテム種類一覧";
	ImGui::Text("%s", headerLabel.c_str());

	int removeIndex = -1;
	for (size_t i = 0; i < spawnEntries.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		SpawnEntry& entry = spawnEntries[i];
		int typeIndex = static_cast<int>(entry.type);
		std::string typeLabel = std::string(namePrefix) + "種別";
		if (ImGui::BeginCombo(typeLabel.c_str(), kTypeNames[typeIndex])) {
			for (int t = 0; t < kTypeCount; t++) {
				bool selected = (t == typeIndex);
				if (ImGui::Selectable(kTypeNames[t], selected)) entry.type = static_cast<GridItemComponent::Type>(t);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		std::string countLabel = std::string(namePrefix) + "個数";
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragInt(countLabel.c_str(), &entry.count, 1.0f, 1, 20);
		entry.count = (std::max)(entry.count, 1);

		ImGui::SameLine();
		std::string removeLabel = std::string(namePrefix) + "削除";
		if (ImGui::Button(removeLabel.c_str())) removeIndex = static_cast<int>(i);

		std::string colorLabel = std::string(namePrefix) + "生成時の色";
		ImGui::ColorEdit4(colorLabel.c_str(), &entry.color.x);

		ImGui::PopID();
	}
	if (removeIndex >= 0) spawnEntries.erase(spawnEntries.begin() + removeIndex);

	std::string addLabel = std::string(namePrefix) + "種類を追加";
	if (ImGui::Button(addLabel.c_str())) spawnEntries.push_back(SpawnEntry{});

	ImGui::Separator();
	std::string displayTopLabel = std::string(namePrefix) + "取得済み表示の一番上の座標";
	std::string displaySpacingLabel = std::string(namePrefix) + "取得済み表示の間隔";
	ImGui::DragFloat3(displayTopLabel.c_str(), &collectedDisplayTop.x, 0.05f);
	ImGui::DragFloat(displaySpacingLabel.c_str(), &collectedDisplaySpacing, 0.05f, 0.05f, 10.0f);

	ImGui::Separator();
	std::string resetLabel = std::string(namePrefix) + "リセット(全アイテムを削除して再配置)";
	if (ImGui::Button(resetLabel.c_str())) resetRequested_ = true;
}

void GridItemSpawnComponent::ToJson(nlohmann::json& out) const {
	nlohmann::json entries = nlohmann::json::array();
	for (const auto& entry : spawnEntries) {
		entries.push_back({
			{ "type", static_cast<int>(entry.type) },
			{ "count", entry.count },
			{ "color", Vector4ToJson(entry.color) },
			});
	}
	out["spawnEntries"] = entries;
	out["collectedDisplayTop"] = Vector3ToJson(collectedDisplayTop);
	out["collectedDisplaySpacing"] = collectedDisplaySpacing;
}

void GridItemSpawnComponent::FromJson(const nlohmann::json& in) {
	if (in.contains("spawnEntries")) {
		spawnEntries.clear();
		for (const auto& entryJson : in["spawnEntries"]) {
			SpawnEntry entry;
			entry.type = static_cast<GridItemComponent::Type>(entryJson.value("type", 0));
			entry.count = entryJson.value("count", 1);
			if (entryJson.contains("color")) entry.color = Vector4FromJson(entryJson["color"]);
			spawnEntries.push_back(entry);
		}
	}
	if (in.contains("collectedDisplayTop")) collectedDisplayTop = Vector3FromJson(in["collectedDisplayTop"]);
	collectedDisplaySpacing = in.value("collectedDisplaySpacing", collectedDisplaySpacing);
}

REGISTER_SIMPLE_COMPONENT(GridItemSpawnComponent, "GridItemSpawn", "グリッドアイテムスポーン", "物理");
