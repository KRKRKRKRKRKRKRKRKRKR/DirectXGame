#include "SceneSerializer.h"
#include "../../Externals/Json/json.hpp"
#include <fstream>

void SceneSerializer::Save(const std::string& path, const std::vector<std::unique_ptr<GameObject>>& objects) {
	nlohmann::json root;
	nlohmann::json objs = nlohmann::json::array();
	for (auto& obj : objects) {
		nlohmann::json j;
		obj->ToJson(j);
		objs.push_back(j);
	}
	root["objects"] = objs;

	std::ofstream file(path);
	file << root.dump(4);
}

bool SceneSerializer::Load(const std::string& path, std::vector<std::unique_ptr<GameObject>>& objects, const ComponentLoadContext& ctx) {
	std::ifstream file(path);
	if (!file) return false;

	nlohmann::json root = nlohmann::json::parse(file, nullptr, /*allow_exceptions=*/false);
	if (root.is_discarded() || !root.contains("objects")) return false;

	objects.clear();
	for (auto& objJson : root["objects"]) {
		auto obj = std::make_unique<GameObject>();
		obj->FromJson(objJson, ctx);
		objects.push_back(std::move(obj));
	}
	return true;
}
