#pragma once
#include "IComponent.h"
#include <functional>
#include <vector>
#include <memory>

class GameObject;

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

	// 型Tに一致する全コンポーネントを返す（TextRenderComponentのように1GameObjectに複数付けられる
	// 型向け。多くの呼び出し元は「主となる1個」だけで十分なためGetComponent<T>()のままでよいが、
	// 描画ループ（RenderMainPass/RenderMirrorPass）のようにRenderComponentBase派生を漏れなく
	// 描画したい場合はこちらを使う）
	template<typename T>
	std::vector<T*> GetComponents() {
		std::vector<T*> result;
		for (auto& c : components_) {
			if (T* ptr = dynamic_cast<T*>(c.get())) result.push_back(ptr);
		}
		return result;
	}

	// 型Tに一致する最初の1個を破棄する（Add Componentメニューからの取り消し用）。
	// 見つかった場合はtrueを返す。TextureSelectorComponent/MirrorComponent等、他コンポーネントへの
	// 生ポインタを持つものが依存先を指している状態で依存先だけを消すとダングリングポインタになるため、
	// 呼び出し側（SceneBase::DrawAddComponentMenu）が依存関係を見て削除順を制御する
	template<typename T>
	bool RemoveComponent() {
		for (auto it = components_.begin(); it != components_.end(); ++it) {
			if (dynamic_cast<T*>(it->get())) {
				components_.erase(it);
				return true;
			}
		}
		return false;
	}

	// index番目（At/Countと同じ添字）のコンポーネントを1個破棄する。TextRenderComponentのように
	// 同じ型が複数並びうる場合、型名だけを頼りにRemoveByTypeName（常に「最初の1個」を消す）を使うと
	// 右クリックで狙った方ではなく常に1個目が消えてしまうため、Inspectorの「コンポーネントを削除」
	// メニューはこちらのインデックス指定版を使う。index==0（Transform）は呼び出し側で除外すること
	bool RemoveAt(size_t index) {
		if (index >= components_.size()) return false;
		components_.erase(components_.begin() + static_cast<ptrdiff_t>(index));
		return true;
	}

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
		for (auto& c : components_) c->Update(deltaTime, transform, ctx);
	}

	// 保持している全コンポーネントへOnTriggerEnterを伝える（ColliderSystem::ResolveAndDraw経由。
	// Update/DrawImGuiと同じ「型を気にせず全部に呼ぶ」形）
	void OnTriggerEnter(GameObject& other) {
		for (auto& c : components_) c->OnTriggerEnter(other);
	}

	// 保持コンポーネント数、およびindex指定での型消去アクセス（Inspectorの並び替えUI等、
	// 型を気にせず順序だけ扱いたい呼び出し元向け）
	size_t Count() const { return components_.size(); }
	IComponent* At(size_t index) const { return components_[index].get(); }

	// fromIndex番目のコンポーネントをtoIndexの位置へ移動する（Inspectorでのドラッグ並び替え用）。
	// 範囲外や移動不要な場合は何もしない。実装はComponentManager.cpp
	// （GameObject.h/ComponentRegistry.hを必要とするDrawImGuiと同じ理由で.cppへ切り出す）
	void Move(size_t fromIndex, size_t toIndex);

	// owner（自分自身が属するGameObject）ごとにコンポーネント一覧をUnity風に描画する
	// （コンポーネントごとの見出し・アイコン代わりの色チップ・ドラッグ並び替え・右クリックの
	// Remove/Move Up/Move Downメニュー込み）。実装はComponentManager.cpp
	void DrawImGui(GameObject& owner);

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
