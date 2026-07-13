#include "SceneSerializer.h"
#include "../../Externals/Json/json.hpp"
#include "../Utils/Logger.h"
#include <fstream>
#include <format>

void SceneSerializer::Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects) {
	Save(path, objects, [](GameObject&) { return true; });
}

void SceneSerializer::Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects,
	const std::function<bool(GameObject&)>& filter) {
	nlohmann::json root;
	nlohmann::json objs = nlohmann::json::array();
	for (auto& obj : objects) {
		if (!filter(*obj)) continue;
		nlohmann::json j;
		obj->ToJson(j);
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
	for (auto& objJson : root["objects"]) {
		auto obj = std::make_unique<GameObject>();
		obj->FromJson(objJson, ctx);
		objects.push_back(std::move(obj));
	}
	return true;
}
