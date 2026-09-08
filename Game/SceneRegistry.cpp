#include "SceneRegistry.h"
#include "GenericScene.h"
#include <algorithm>
#include <filesystem>

bool SceneRegistry::IsRegistered(const std::string& name) {
	return Factories().find(name) != Factories().end();
}

bool SceneRegistry::RegisterGenericIfMissing(const std::string& name) {
	if (IsRegistered(name)) return false;
	Register(name, []() { return std::make_unique<GenericScene>(); });
	GenericFlags()[name] = true;
	return true;
}

bool SceneRegistry::IsGeneric(const std::string& name) {
	auto it = GenericFlags().find(name);
	return it != GenericFlags().end() && it->second;
}

void SceneRegistry::Unregister(const std::string& name) {
	Factories().erase(name);
	GenericFlags().erase(name);
	auto& names = Names();
	names.erase(std::remove(names.begin(), names.end(), name), names.end());
}

std::unordered_map<std::string, bool>& SceneRegistry::GenericFlags() {
	static std::unordered_map<std::string, bool> flags;
	return flags;
}

std::unordered_map<std::string, SceneRegistry::Factory>& SceneRegistry::Factories() {
	static std::unordered_map<std::string, Factory> factories;
	return factories;
}

std::vector<std::string>& SceneRegistry::Names() {
	static std::vector<std::string> names;
	return names;
}

void SceneRegistry::Register(const std::string& name, Factory factory) {
	Factories()[name] = std::move(factory);
	Names().push_back(name);
}

std::unique_ptr<IScene> SceneRegistry::Create(const std::string& name) {
	auto it = Factories().find(name);
	if (it == Factories().end()) return nullptr;
	return it->second();
}

const std::vector<std::string>& SceneRegistry::GetAllNames() {
	return Names();
}

void SceneRegistry::ScanResourcesForGenericScenes() {
	namespace fs = std::filesystem;
	std::error_code ec;
	if (!fs::exists("Resources", ec) || ec) return;

	for (auto& entry : fs::directory_iterator("Resources", ec)) {
		if (ec || !entry.is_directory()) continue;
		std::string name = entry.path().filename().string();
		if (IsRegistered(name)) continue; // REGISTER_SCENE済みの固定シーンはスキップ

		std::error_code existsEc;
		bool hasSceneJson = fs::exists(entry.path() / "scene.json", existsEc);
		bool hasUiJson = fs::exists(entry.path() / "ui.json", existsEc);
		if (hasSceneJson || hasUiJson) {
			RegisterGenericIfMissing(name);
		}
	}
}
