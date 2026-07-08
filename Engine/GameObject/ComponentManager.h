#pragma once
#include "IComponent.h"
#include <functional>
#include <vector>
#include <memory>

// GameObjectが持つIComponent群の保持・追加・検索・一括更新/描画を担当する。
// コンポーネントの種類が増えてきたため、GameObject本体からこの責務を切り出した
// （ColliderSystem/GizmoControllerと同じ「役割ごとに専用クラスへ切り出す」パターン）
class ComponentManager {
public:
	template<typename T, typename... Args>
	T* AddComponent(Args&&... args) {
		auto component = std::make_unique<T>(std::forward<Args>(args)...);
		T* ptr = component.get();
		components_.push_back(std::move(component));
		return ptr;
	}

	template<typename T>
	T* GetComponent() {
		for (auto& c : components_) {
			if (T* ptr = dynamic_cast<T*>(c.get())) return ptr;
		}
		return nullptr;
	}

	void Update(float deltaTime, Transform& transform) {
		for (auto& c : components_) c->Update(deltaTime, transform);
	}

	void DrawImGui(const char* namePrefix) {
		for (auto& c : components_) c->DrawImGui(namePrefix);
	}

	// 保持している全コンポーネントへ生ポインタで触れるための走査（JSON保存等、GameObject側で
	// 型を気にせず中身を見たい場合に使う。所有権はComponentManagerに残したまま）
	void ForEach(const std::function<void(IComponent*)>& fn) const {
		for (auto& c : components_) fn(c.get());
	}

	// 保持コンポーネントを全て破棄する（JSONからの再構築前に呼ぶ）
	void Clear() { components_.clear(); }

private:
	std::vector<std::unique_ptr<IComponent>> components_;
};
