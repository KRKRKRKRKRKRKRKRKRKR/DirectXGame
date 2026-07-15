#include "ComponentRegistry.h"

std::unordered_map<std::string, ComponentRegistry::CreatorFunc>& ComponentRegistry::Creators() {
	static std::unordered_map<std::string, CreatorFunc> creators;
	return creators;
}

std::unordered_map<std::type_index, std::string>& ComponentRegistry::TypeNames() {
	static std::unordered_map<std::type_index, std::string> typeNames;
	return typeNames;
}

std::vector<std::string>& ComponentRegistry::SimpleTypeNames() {
	static std::vector<std::string> simpleTypeNames;
	return simpleTypeNames;
}

std::unordered_map<std::string, ComponentRegistry::RemoverFunc>& ComponentRegistry::Removers() {
	static std::unordered_map<std::string, RemoverFunc> removers;
	return removers;
}

std::unordered_map<std::string, std::string>& ComponentRegistry::DisplayNames() {
	static std::unordered_map<std::string, std::string> displayNames;
	return displayNames;
}

std::string ComponentRegistry::GetDisplayName(const std::string& typeName) {
	auto it = DisplayNames().find(typeName);
	return (it != DisplayNames().end()) ? it->second : typeName;
}

void ComponentRegistry::Create(const std::string& typeName, GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
	auto it = Creators().find(typeName);
	if (it == Creators().end()) return; // 未登録の型名は無視する
	it->second(obj, ctx, data);
}

bool ComponentRegistry::RemoveByTypeName(const std::string& typeName, GameObject& obj) {
	auto it = Removers().find(typeName);
	if (it == Removers().end()) return false;
	return it->second(obj);
}

std::string ComponentRegistry::GetTypeName(const std::type_info& type) {
	auto it = TypeNames().find(std::type_index(type));
	if (it == TypeNames().end()) return "";
	return it->second;
}
