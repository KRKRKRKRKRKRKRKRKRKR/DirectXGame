#pragma once
#include "../../IComponent.h"
#include "CollisionLayer.h"
#include <vector>

class GameObject;

// Sphere/OBBColliderComponentに共通する「入力データ」の置き場。
// RenderComponentBase（描画コンポーネントの共通プロパティ基底）と同じ考え方の
// 当たり判定版。形状（半径/halfSize等）は各派生クラス固有のままここには置かない
class ColliderComponentBase : public IComponent {
public:
	// このColliderがどの種類のオブジェクトか（自分が何者か）を表す所属レイヤー
	CollisionLayer layer = CollisionLayer::kDefault;

	// true: Trigger（重なりを検知するだけで押し戻しはしない。アイテム取得等の用途）
	// false: Solid（両方Solidで重なった場合、Game::ResolveAndDrawColliderGizmos()が
	//         Collision::*Penetrationで求めた重なり量ぶん、双方のtranslationを半分ずつ
	//         押し戻して分離する。障害物同士の衝突等の用途）
	bool isTrigger = false;

	// このコライダーが衝突判定の対象として選んでいるレイヤーの一覧（trueなら判定候補にする）。
	// 添字はCollisionLayerの値をそのまま使う。デフォルトは全レイヤーとtrue
	// （何も設定しなければ従来通り全部と当たる。除外したいものだけ個別にfalseにする運用）
	bool collidesWith[static_cast<size_t>(CollisionLayer::kCount)] = { true, true, true, true, true };

	bool CollidesWithLayer(CollisionLayer other) const {
		return collidesWith[static_cast<size_t>(other)];
	}

	// true: 押し戻しで動かない（地面や壁など、常に固定しておきたいオブジェクト用）。
	// 両方Solidで重なった場合、isStaticでない側だけが100%押し戻される
	// （両方isStaticなら押し戻し自体をスキップする）
	bool isStatic = false;

	// Trigger判定で「前フレームに重なっていた相手」一覧。ColliderSystem::ResolveAndDrawが
	// 侵入の瞬間（前フレームは無かった相手）だけOnTriggerEnterを呼ぶために使う実行時の
	// 一時状態（GravityComponent::velocityYと同じ扱いで、保存はしない）
	std::vector<GameObject*> triggerOverlapsLastFrame_;

	// "{namePrefix} Collider Layer"コンボ・"{namePrefix} Collider Is Trigger"チェックボックス・
	// "{namePrefix} Collider Is Static"チェックボックス・"{namePrefix} Collides With"の
	// レイヤー別チェックボックス一覧を描画する
	void DrawImGui(const char* namePrefix) override;

	// layer/isTrigger/isStatic/collidesWithを書き出す/読み込む。派生クラスは自分固有の
	// フィールド（offset/radius等）を足す前にColliderComponentBase::ToJson(out)を呼ぶ
	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// Gizmoでのオフセット/サイズ編集用。ワールド座標のtranslation(=中心)とscale(=見た目のサイズ)を
	// 返す（rotationは使わないため既定値のまま）。ドラッグ後の結果をApplyGizmoEditTransformで
	// 自分のoffset/radius(またはhalfSize)へ書き戻す
	virtual Transform GetGizmoEditTransform(const Transform& ownerTransform) const = 0;
	virtual void ApplyGizmoEditTransform(const Transform& ownerTransform, const Transform& edited) = 0;
};

// 2つのコライダーが判定対象かどうか。お互いが相手の所属レイヤーを自分の衝突対象に
// 含めている場合にのみtrue（片方だけが含めていても衝突しない＝両者の合意が必要）
inline bool ShouldLayersCollide(const ColliderComponentBase& a, const ColliderComponentBase& b) {
	return a.CollidesWithLayer(b.layer) && b.CollidesWithLayer(a.layer);
}
