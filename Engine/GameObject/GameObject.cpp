#include "GameObject.h"
#include "ComponentRegistry.h"
#include "ComponentLoadContext.h"
#include "../../Math/JsonUtil.h"

void GameObject::ToJson(nlohmann::json& out) const {
	out["name"] = name;

	const Transform& t = transformComponent_->transform;
	out["transform"]["translation"] = Vector3ToJson(t.translation);
	out["transform"]["rotation"]    = Vector3ToJson(t.rotation);
	out["transform"]["scale"]       = Vector3ToJson(t.scale);

	nlohmann::json comps = nlohmann::json::array();
	components_.ForEach([&](IComponent* c) {
		if (dynamic_cast<TransformComponent*>(c)) return; // Transformは上で別扱い済み
		std::string typeName = ComponentRegistry::GetTypeName(typeid(*c));
		if (typeName.empty()) return; // 未登録＝保存対象外
		nlohmann::json data;
		c->ToJson(data);
		comps.push_back({ {"type", typeName}, {"data", data} });
	});
	out["components"] = comps;
}

void GameObject::FromJson(const nlohmann::json& in, const ComponentLoadContext& ctx) {
	name = in.value("name", name);

	components_.Clear();
	transformComponent_ = components_.AddComponent<TransformComponent>();
	if (in.contains("transform")) {
		const auto& t = in["transform"];
		if (t.contains("translation")) transformComponent_->transform.translation = Vector3FromJson(t["translation"]);
		if (t.contains("rotation"))    transformComponent_->transform.rotation    = Vector3FromJson(t["rotation"]);
		if (t.contains("scale"))       transformComponent_->transform.scale       = Vector3FromJson(t["scale"]);
	}

	if (in.contains("components")) {
		for (const auto& entry : in["components"]) {
			ComponentRegistry::Create(entry.value("type", std::string()), *this, ctx, entry["data"]);
		}
	}
}
