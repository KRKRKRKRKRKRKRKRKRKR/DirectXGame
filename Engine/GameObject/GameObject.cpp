#include "GameObject.h"
#include "ComponentRegistry.h"
#include "ComponentLoadContext.h"
#include "../../Math/JsonUtil.h"
#include "../../Math/TransformMath.h"
#include "../../Math/MatrixMath.h"
#include "../../Externals/ImGuizmo/src/ImGuizmo.h"
#include <algorithm>
#include <unordered_set>

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

void GameObject::ReparentAt(GameObject* dropped, size_t index) {
	dropped->SetParent(this); // 循環参照チェック・旧親からの除去・children_末尾への追加はここで完結
	if (dropped->GetParent() != this) return; // 循環参照等でSetParentが無視した場合は何もしない

	auto it = std::find(children_.begin(), children_.end(), dropped);
	if (it == children_.end()) return;
	size_t from = static_cast<size_t>(it - children_.begin());

	size_t to = index;
	if (to > from) to--; // fromを消すと、それより後ろの目標indexは1つ前へ詰まるため補正
	size_t maxTo = children_.size() - 1; // eraseの前に、eraseした後の有効な最大挿入位置を求めておく
	if (to > maxTo) to = maxTo;
	if (from == to) return;

	GameObject* moved = children_[from];
	children_.erase(children_.begin() + from);
	children_.insert(children_.begin() + to, moved);
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
	out["tag"] = tag;
	out["excludeFromGizmoList"] = excludeFromGizmoList;
	out["excludeFromPicking"] = excludeFromPicking;

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
	tag = in.value("tag", tag);
	excludeFromGizmoList = in.value("excludeFromGizmoList", false);
	excludeFromPicking = in.value("excludeFromPicking", false);

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
		// TextureSelector/Mirrorは兄弟のRenderComponentBase系/CubeRenderComponentへの生ポインタを
		// コンストラクタで即座にdereferenceするため、依存先より先に生成されるとnullptrになりクラッシュする。
		// Inspectorのドラッグ&ドロップ並び替え（ComponentManager::Move）で保存順が入れ替わりうるため、
		// 依存する側の型は常に後回しにして2パスで生成する
		static const std::unordered_set<std::string> kDeferredTypes = { "TextureSelector", "Mirror" };
		for (const auto& entry : in["components"]) {
			std::string type = entry.value("type", std::string());
			if (kDeferredTypes.count(type)) continue;
			ComponentRegistry::Create(type, *this, ctx, entry["data"]);
		}
		for (const auto& entry : in["components"]) {
			std::string type = entry.value("type", std::string());
			if (!kDeferredTypes.count(type)) continue;
			ComponentRegistry::Create(type, *this, ctx, entry["data"]);
		}
	}
}
