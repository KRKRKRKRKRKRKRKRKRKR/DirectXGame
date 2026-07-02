#pragma once
#include "IComponent.h"
#include "../../Math/MathTypes.h"
#include <vector>
#include <memory>
#include <string>

// 描画オブジェクト1個を表す軽量コンテナ。Transformを直接所有し、
// 任意の追加機能（描画方法等）をIComponent派生クラスとしてアタッチできる。
// GameObject自体はRendererを知らない（コンポーネント側がRendererを直接呼ぶ）
class GameObject {
public:
	Transform   transform;
	std::string name; // ImGui/ギズモのUI表示用の識別名

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

	void Update(float deltaTime) {
		for (auto& c : components_) c->Update(deltaTime);
	}

private:
	std::vector<std::unique_ptr<IComponent>> components_;
};
