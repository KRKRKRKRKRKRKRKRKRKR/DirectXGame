#pragma once
#include "IComponent.h"
#include "ComponentManager.h"
#include "Component/TransformComponent.h"
#include "../../Math/MathTypes.h"
#include "../../Externals/imgui/imgui.h"
#include <string>
#include <vector>

struct ComponentLoadContext;

// 描画オブジェクト1個を表す軽量コンテナ。Transformは必ず1つ持つTransformComponentとして
// 保持し（コンストラクタで自動アタッチ）、任意の追加機能（描画方法等）をIComponent派生クラスと
// してアタッチできる。GameObject自体はRendererを知らない（コンポーネント側がRendererを直接呼ぶ）。
// コンポーネントの保持・追加・検索・一括更新/描画自体はComponentManagerに委譲する
class GameObject {
public:
	GameObject() { transformComponent_ = components_.AddComponent<TransformComponent>(); }

	std::string name; // ImGui/ギズモのUI表示用の識別名

	// Unityのタグに相当する任意の識別文字列（空="Untagged"）。「シーン内で最初に見つかった
	// Xを機械的に使う」だけでは複数存在する場合に選べなかった箇所（メインカメラ・プレイヤー等）を、
	// SceneBase::FindObjectByTagで明示的に指定できるようにする
	std::string tag;

	// マウスピッキング用のBounding Sphere半径の基準値（scale=1のときの半径）。
	// Cube/Sphere/Triangleは「1辺/半径1.0の単位形状」を前提にscaleがそのまま見た目のサイズと
	// 一致するため1.0のままでよいが、Model系はメッシュ自体の実寸とscaleの対応が個体ごとに
	// 異なる（例: FBXModelはscale=0.01で実寸メッシュを縮小している）ため、Initialize()側で
	// 実際の見た目に合わせて上書きする。ピッキング半径は pickingRadiusHint * max(scale.x,y,z) で計算する
	float pickingRadiusHint = 1.0f;

	// trueにすると、GizmoController::UpdatePickingのレイキャスト対象から外れる（コンボボックス
	// からの選択は引き続き可能）。Floorのような極端に平たいオブジェクトは、scale最大成分を
	// 半径とするBounding Sphere近似では実際の見た目よりはるかに巨大な球になり、
	// どこをクリックしても最優先でヒットしてしまうため使う
	bool excludeFromPicking = false;

	// trueにすると、PlayScene::RebuildDerivedListsがgizmoTargets_（Gizmoコンボ選択・ピッキング
	// 候補の一覧）に含めない。Sprite2D（スクリーン空間UI）やBGM（3D位置を持たない）用
	bool excludeFromGizmoList = false;

	// trueにすると、SceneObjectStore::Save（SceneSerializer::Saveのfilter経由）がこのGameObjectを
	// scene.json/ui.jsonへ書き出さない。パーティクルのようにコード側が実行時に大量生成し、
	// 短い寿命で自動的に消えていく一時的なGameObjectに使う。セーブ操作のタイミングによっては
	// まだ消える前のインスタンスが混ざっていることがあり、それをそのまま保存してしまうと
	// 次回ロード時にParticleComponent（保存対象外）を持たない、見た目だけの静止オブジェクトとして
	// シーンに残ってしまう問題があった
	bool excludeFromSave = false;

	// TransformComponentが実際に持つTransformへの参照を返す（既存の.transformアクセスの代替）
	Transform& GetTransform() { return transformComponent_->transform; }

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args) { return components_.AddComponent<T>(std::forward<Args>(args)...); }

	template<typename T>
	T* GetComponent() { return components_.GetComponent<T>(); }

	// 型Tに一致する全コンポーネントを返す（TextRenderComponent等、1GameObjectに複数付けられる
	// 型向け。詳しくはComponentManager::GetComponents参照）
	template<typename T>
	std::vector<T*> GetComponents() { return components_.GetComponents<T>(); }

	// 型Tのコンポーネントを1個破棄する。TransformComponentは常にコンストラクタで
	// 自動アタッチされる必須コンポーネントのため、呼び出し側で対象から除外すること
	template<typename T>
	bool RemoveComponent() { return components_.RemoveComponent<T>(); }

	void Update(float deltaTime, const UpdateContext& ctx) { components_.Update(deltaTime, transformComponent_->transform, ctx); }

	// ColliderSystem::ResolveAndDrawが、Trigger相手と新しく重なった瞬間に呼ぶ。
	// 自分が持つ全コンポーネントへそのままIComponent::OnTriggerEnterとして伝える
	// （実際に何か反応するかどうかは各コンポーネント側のオーバーライド次第。例：HealthComponent）
	void OnTriggerEnter(GameObject& other) { components_.OnTriggerEnter(other); }

	// 自分が持つ全コンポーネントをUnity風（コンポーネントごとの見出し・並び替え・右クリック
	// メニュー付き）にImGui描画させる。GameObject自身はどんなコンポーネントが付いているか
	// 一切気にしない（Update()と同じ形）。TransformComponentもcomponents_の一員のため、
	// ここでScale/Rotation/Translationも自動描画される。名前の表示・編集はInspector側
	// （SceneBase::DrawInspector）が別途行う
	void DrawImGui() {
		components_.DrawImGui(*this);
	}

	// name・Transform・保持コンポーネント一式をJSONへ書き出す/読み込む。実装はComponentRegistry
	// （GameObjectを使う側）に依存するため、循環includeを避けてGameObject.cppに置く
	// （GameObject.hがこれまで.cppを持たなかった唯一の例外）
	void ToJson(nlohmann::json& out) const;
	void FromJson(const nlohmann::json& in, const ComponentLoadContext& ctx);

	// 親子関係。parent_/children_は非所有の生ポインタ（実体の所有権はSceneBase::objects_の
	// unique_ptrにある）。GetTransform()はあくまで親から見た相対値（ローカル）のまま変わらない
	GameObject* GetParent() const { return parent_; }
	const std::vector<GameObject*>& GetChildren() const { return children_; }

	// newParentをnullptrにするとルート（親なし）に戻す。newParentが自分自身、または自分の
	// 子孫（IsDescendantOfで判定）の場合は循環参照になるため何もしない
	void SetParent(GameObject* newParent);

	// Hierarchyのドラッグ&ドロップ並べ替え用。droppedをthisの子にした上で、children_内の
	// index番目（挿入後の位置、0=先頭）に来るよう並べ替える。droppedが既にthisの子である場合は
	// SetParent自体は何もしないが、children_内での位置移動だけは行われる。循環参照になる場合は
	// SetParentが無視するため何もしない
	void ReparentAt(GameObject* dropped, size_t index);

	// thisがobjの子孫（子・孫…）かどうか。SetParentの循環防止に使う
	bool IsDescendantOf(const GameObject* obj) const;

	// 親をたどって合成したワールド行列/ワールド相当Transform。「実際に画面上どこにあるか」を
	// 知りたい読み取り専用の呼び出し元（描画・ピッキング・ギズモの初期姿勢）はGetTransform()
	// ではなくこちらを使う
	Matrix4x4 GetWorldMatrix() const;
	Transform GetWorldTransform() const;

private:
	TransformComponent* transformComponent_ = nullptr;
	ComponentManager components_;
	GameObject* parent_ = nullptr;
	std::vector<GameObject*> children_;
};
