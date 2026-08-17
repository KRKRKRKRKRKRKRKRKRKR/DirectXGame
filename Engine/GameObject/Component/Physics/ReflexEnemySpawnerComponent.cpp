#include "ReflexEnemySpawnerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"

void ReflexEnemySpawnerComponent::DrawImGui(const char* namePrefix) {
	// テンプレートのタグ選択（シーン内のReflexEnemyComponent::isTemplate=true一覧からのコンボ）は
	// このコンポーネント単体では描画できない（IComponent::DrawImGuiはシーン全体を参照できないため）。
	// SceneBase::DrawInspectorが選択中オブジェクトがこのコンポーネントを持つか判定し、
	// DrawImGui呼び出しの直後にコンボ・出現数・追加/削除ボタンを一括で描画する
	(void)namePrefix;
}

void ReflexEnemySpawnerComponent::ToJson(nlohmann::json& out) const {
	nlohmann::json entriesJson = nlohmann::json::array();
	for (const auto& entry : spawnEntries) {
		entriesJson.push_back({ {"tag", entry.tag}, {"count", entry.count} });
	}
	out["spawnEntries"] = entriesJson;
	out["respawnInterval"] = respawnInterval;
}

void ReflexEnemySpawnerComponent::FromJson(const nlohmann::json& in) {
	if (in.contains("spawnEntries") && in["spawnEntries"].is_array() && !in["spawnEntries"].empty()) {
		spawnEntries.clear();
		for (const auto& e : in["spawnEntries"]) {
			SpawnEntry entry;
			entry.tag = e.value("tag", entry.tag);
			entry.count = e.value("count", entry.count);
			spawnEntries.push_back(entry);
		}
	}
	respawnInterval = in.value("respawnInterval", respawnInterval);
}

REGISTER_SIMPLE_COMPONENT(ReflexEnemySpawnerComponent, "ReflexEnemySpawner", "REFLEX敵スポーン設定", "物理");
