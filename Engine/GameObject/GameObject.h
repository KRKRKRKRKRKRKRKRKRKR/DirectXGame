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

	// マウスピッキング用のBounding Sphere半径の基準値（scale=1のときの半径）。
	// Cube/Sphere/Triangleは「1辺/半径1.0の単位形状」を前提にscaleがそのまま見た目のサイズと
	// 一致するため1.0のままでよいが、Model系はメッシュ自体の実寸とscaleの対応が個体ごとに
	// 異なる（例: FBXModelはscale=0.01で実寸メッシュを縮小している）ため、Initialize()側で
	// 実際の見た目に合わせて上書きする。ピッキング半径は pickingRadiusHint * max(scale.x,y,z) で計算する
	float pickingRadiusHint = 1.0f;

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
