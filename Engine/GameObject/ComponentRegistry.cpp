#include "ComponentRegistry.h"

std::unordered_map<std::string, ComponentRegistry::CreatorFunc>& ComponentRegistry::Creators() {
	static std::unordered_map<std::string, CreatorFunc> creators;
	return creators;
}

std::unordered_map<std::type_index, std::string>& ComponentRegistry::TypeNames() {
	static std::unordered_map<std::type_index, std::string> typeNames;
	return typeNames;
}

void ComponentRegistry::Create(const std::string& typeName, GameObject& obj, const ComponentLoadContext& ctx, const nlohmann::json& data) {
	auto it = Creators().find(typeName);
	if (it == Creators().end()) return; // 未登録の型名は無視する
	it->second(obj, ctx, data);
}

std::string ComponentRegistry::GetTypeName(const std::type_info& type) {
	auto it = TypeNames().find(std::type_index(type));
	if (it == TypeNames().end()) return "";
	return it->second;
}
