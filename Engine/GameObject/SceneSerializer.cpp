#include "SceneSerializer.h"
#include "../../Externals/Json/json.hpp"
#include "../Utils/Logger.h"
#include <fstream>
#include <format>
#include <unordered_map>

void SceneSerializer::Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects) {
	Save(path, objects, [](GameObject&) { return true; });
}

void SceneSerializer::Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects,
	const std::function<bool(GameObject&)>& filter) {
	// フィルタを通過したGameObject群だけを対象に、この配列内でのインデックス対応表を先に作る。
	// 親がこの対応表に載っていない（is2Dが違う等でui.json/scene.jsonの別ファイル行きになった）
	// 場合はparentIndex=-1として保存し、Load後はルート扱いになる
	std::vector<GameObject*> filtered;
	for (auto& obj : objects) {
		if (filter(*obj)) filtered.push_back(obj.get());
	}
	std::unordered_map<GameObject*, int> indexOf;
	for (size_t i = 0; i < filtered.size(); i++) indexOf[filtered[i]] = static_cast<int>(i);

	nlohmann::json root;
	nlohmann::json objs = nlohmann::json::array();
	for (GameObject* obj : filtered) {
		nlohmann::json j;
		obj->ToJson(j);
		auto it = indexOf.find(obj->GetParent());
		j["parentIndex"] = (it != indexOf.end()) ? it->second : -1;
		objs.push_back(j);
	}
	root["objects"] = objs;

	std::ofstream file(path);
	file << root.dump(4);
}

bool SceneSerializer::Load(const std::string& path, std::vector<std::unique_ptr<GameObject>>& objects,
	const ComponentLoadContext& ctx, bool clearExisting) {
	std::ifstream file(path);
	// ファイルが無いのは「まだ一度もSaveしていない」という正常系（初回起動時等）でもあるため、
	// ここではログを出さずfalseを返すだけにする
	if (!file) return false;

	nlohmann::json root = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
	if (root.is_discarded()) {
		Logger::Log(std::format("SceneSerializer::Load: failed to parse JSON '{}' (file is corrupted or not valid JSON)\n", path));
		return false;
	}
	if (!root.contains("objects")) {
		Logger::Log(std::format("SceneSerializer::Load: '{}' has no \"objects\" key\n", path));
		return false;
	}

	if (clearExisting) objects.clear();
	const auto& objsJson = root["objects"];
	size_t base = objects.size();
	for (const auto& objJson : objsJson) {
		auto obj = std::make_unique<GameObject>();
		obj->FromJson(objJson, ctx);
		objects.push_back(std::move(obj));
	}

	// 2周目：parentIndexを解決して親子関係を復元する（同じファイル内で完結する親子関係のみ対応。
	// Save側でparentIndex=-1にされたもの＝別ファイル行きの親を持っていたものはルートのままになる）
	for (size_t i = 0; i < objsJson.size(); i++) {
		int parentIndex = objsJson[i].value("parentIndex", -1);
		if (parentIndex < 0) continue;
		size_t parentPos = base + static_cast<size_t>(parentIndex);
		if (parentPos < objects.size()) {
			objects[base + i]->SetParent(objects[parentPos].get());
		}
	}
	return true;
}
