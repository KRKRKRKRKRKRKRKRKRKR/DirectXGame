#include "GameObject.h"
#include "ComponentRegistry.h"
#include "ComponentLoadContext.h"
#include "../../Math/JsonUtil.h"
#include "../../Math/TransformMath.h"
#include "../../Math/MatrixMath.h"
#include "../../Externals/ImGuizmo/src/ImGuizmo.h"
#include <algorithm>

void GameObject::SetParent(GameObject* newParent) {
	if (newParent == this) return;
	if (newParent && newParent->IsDescendantOf(this)) return; // 循環参照防止
	if (parent_ == newParent) return;

	if (parent_) {
		auto& siblings = parent_->children_;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
	}
	parent_ = newParent;
	if (parent_) parent_->children_.push_back(this);
}

bool GameObject::IsDescendantOf(const GameObject* obj) const {
	for (const GameObject* p = parent_; p; p = p->parent_) {
		if (p == obj) return true;
	}
	return false;
}

Matrix4x4 GameObject::GetWorldMatrix() const {
	const Transform& t = transformComponent_->transform;
	Matrix4x4 local = TransformMath::MakeAffineMatrix(t.scale, t.rotation, t.translation);
	return parent_ ? local * parent_->GetWorldMatrix() : local;
}

Transform GameObject::GetWorldTransform() const {
	Matrix4x4 world = GetWorldMatrix();
	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&world._11, t, r, s);
	Transform result;
	result.translation = { t[0], t[1], t[2] };
	result.rotation = {
		DirectX::XMConvertToRadians(r[0]),
		DirectX::XMConvertToRadians(r[1]),
		DirectX::XMConvertToRadians(r[2]) }; // ImGuizmoは度数法、Transform.rotationはラジアン
	result.scale = { s[0], s[1], s[2] };
	return result;
}

void GameObject::ToJson(nlohmann::json& out) const {
	out["name"] = name;
	out["excludeFromGizmoList"] = excludeFromGizmoList;

	const Transform& t = transformComponent_->transform;
	out["transform"]["translation"] = Vector3ToJson(t.translation);
	out["transform"]["rotation"]    = Vector3ToJson(t.rotation);
	out["transform"]["scale"]       = Vector3ToJson(t.scale);
	out["transform"]["is2D"]        = transformComponent_->is2D;

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
	excludeFromGizmoList = in.value("excludeFromGizmoList", false);

	components_.Clear();
	transformComponent_ = components_.AddComponent<TransformComponent>();
	if (in.contains("transform")) {
		const auto& t = in["transform"];
		if (t.contains("translation")) transformComponent_->transform.translation = Vector3FromJson(t["translation"]);
		if (t.contains("rotation"))    transformComponent_->transform.rotation    = Vector3FromJson(t["rotation"]);
		if (t.contains("scale"))       transformComponent_->transform.scale       = Vector3FromJson(t["scale"]);
		transformComponent_->is2D = t.value("is2D", false);
	}

	if (in.contains("components")) {
		for (const auto& entry : in["components"]) {
			ComponentRegistry::Create(entry.value("type", std::string()), *this, ctx, entry["data"]);
		}
	}
}
