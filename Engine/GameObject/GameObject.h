#pragma once
#include "IComponent.h"
#include "ComponentManager.h"
#include "Component/TransformComponent.h"
#include "../../Math/MathTypes.h"
#include "../../Externals/imgui/imgui.h"
#include <string>

struct ComponentLoadContext;

// 描画オブジェクト1個を表す軽量コンテナ。Transformは必ず1つ持つTransformComponentとして
// 保持し（コンストラクタで自動アタッチ）、任意の追加機能（描画方法等）をIComponent派生クラスと
// してアタッチできる。GameObject自体はRendererを知らない（コンポーネント側がRendererを直接呼ぶ）。
// コンポーネントの保持・追加・検索・一括更新/描画自体はComponentManagerに委譲する
class GameObject {
public:
	GameObject() { transformComponent_ = components_.AddComponent<TransformComponent>(); }

	std::string name; // ImGui/ギズモのUI表示用の識別名

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

	// TransformComponentが実際に持つTransformへの参照を返す（既存の.transformアクセスの代替）
	Transform& GetTransform() { return transformComponent_->transform; }

	template<typename T, typename... Args>
	T* AddComponent(Args&&... args) { return components_.AddComponent<T>(std::forward<Args>(args)...); }

	template<typename T>
	T* GetComponent() { return components_.GetComponent<T>(); }

	void Update(float deltaTime) { components_.Update(deltaTime, transformComponent_->transform); }

	// 自分の名前を見出しとして表示した後、自分が持つ全コンポーネントに、自分のnameを渡して
	// ImGuiを描画させる。GameObject自身はどんなコンポーネントが付いているか一切気にしない
	// （Update()と同じ形）。TransformComponentもcomponents_の一員のため、ここでScale/Rotation/
	// Translationも自動描画される。呼び出し側は名前表示を毎回書かずに済む
	void DrawImGui() {
		ImGui::Text("%s", name.c_str());
		components_.DrawImGui(name.c_str());
	}

	// name・Transform・保持コンポーネント一式をJSONへ書き出す/読み込む。実装はComponentRegistry
	// （GameObjectを使う側）に依存するため、循環includeを避けてGameObject.cppに置く
	// （GameObject.hがこれまで.cppを持たなかった唯一の例外）
	void ToJson(nlohmann::json& out) const;
	void FromJson(const nlohmann::json& in, const ComponentLoadContext& ctx);

private:
	TransformComponent* transformComponent_ = nullptr;
	ComponentManager components_;
};
