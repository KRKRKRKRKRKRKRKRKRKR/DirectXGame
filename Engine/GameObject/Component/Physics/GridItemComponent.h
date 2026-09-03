// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

class GameObject;

// パイプ接続パズル企画の盤面アイテム。プレイヤーGameObject（GridBoardPlayerComponent+
// OBBColliderComponent(isTrigger=true)）と、このコンポーネントを持つGameObject自身の
// OBBColliderComponent(isTrigger=true)がColliderSystem::ResolveAndDraw経由で重なった瞬間、
// OnTriggerEnterが呼ばれて効果を発動する（実際の効果適用は相手のGridBoardPlayerComponent::
// ApplyItemEffectに委譲する。発動・削除の実処理はGridBoardPlayerComponent/GridPuzzleScene側が
// 行う。このコンポーネント自身は種別・配置マス座標・見た目色を持つデータコンポーネント）。
//
// 3種類（企画書確定仕様）：
// - kAttackPower：そのターン限りの攻撃力+1
// - kCostFixed：移動コスト+2固定
// - kCostRisky：踏んだ瞬間に50%の確率で移動コスト±4（-4を引いてもコストは最低1で下げ止まる）
class GridItemComponent : public IComponent {
public:
	enum class Type { kAttackPower, kCostFixed, kCostRisky };

	Type type = Type::kAttackPower;

	// 配置マス座標（GridBoardComponent::GridToWorldでワールド座標に変換する）。GameObjectの
	// Transformとは別に持つ理由：Inspectorから盤面のマス単位で配置を指定できるようにするため
	// （GridPuzzleScene::SyncItemsが毎フレームTransform.translationへ反映する）
	int col = 0;
	int row = 0;

	// 見た目色。GridPuzzleScene::EnsureInitialObjectsExistが配置時にtype別の既定色
	// （攻撃力=赤、コスト固定=青、コストリスキー=紫）を初期値として渡すが、Inspectorから
	// 自由に上書きできる。実際の描画は兄弟のCubeRenderComponent::colorが担うため、
	// GridPuzzleSceneがこの値をCubeRenderComponent::colorへ毎フレーム同期する
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	void OnTriggerEnter(GameObject& other) override;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// プレイヤーとの重なりを検知して効果を発動した瞬間trueになる（実行時の一時状態、非保存）。
	// GridItemComponent自身はシーンのオブジェクト所有権を持たず自分のGameObjectを削除できないため、
	// GridPuzzleScene::ProcessTriggeredItemsが毎フレームこのフラグを見て、trueなGameObjectを
	// まとめてDeleteObjectsする（削除自体はScene側の責務にする、ReflexEnemyComponent::
	// pendingDestroyと同じパターン）
	bool triggered = false;
};
