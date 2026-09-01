#include "SceneRegistry.h"
#include <algorithm>

std::unordered_map<std::string, SceneRegistry::Factory>& SceneRegistry::Factories() {
	static std::unordered_map<std::string, Factory> factories;
	return factories;
}

std::vector<std::string>& SceneRegistry::Names() {
	static std::vector<std::string> names;
	return names;
}

void SceneRegistry::Register(const std::string& name, Factory factory) {
	bool alreadyRegistered = Factories().find(name) != Factories().end();
	Factories()[name] = std::move(factory);
	if (!alreadyRegistered) {
		Names().push_back(name);
	}
}

void SceneRegistry::Unregister(const std::string& name) {
	Factories().erase(name);
	auto& names = Names();
	names.erase(std::remove(names.begin(), names.end(), name), names.end());
}

std::unique_ptr<IScene> SceneRegistry::Create(const std::string& name) {
	auto it = Factories().find(name);
	if (it == Factories().end()) return nullptr;
	return it->second();
}

const std::vector<std::string>& SceneRegistry::GetAllNames() {
	return Names();
}
