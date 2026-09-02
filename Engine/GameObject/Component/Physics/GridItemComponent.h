// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// パイプ接続パズル企画の盤面アイテム。GridBoardPlayerComponentが経路予約時（クリックした瞬間）に
// 通過マスへこのコンポーネントを持つGameObjectがあれば効果を発動する（発動・削除の実処理は
// GridBoardPlayerComponent/GridPuzzleScene側が行う。このコンポーネント自身は種別・配置マス座標・
// 見た目色を持つだけのデータコンポーネント）。
//
// 3種類（企画書確定仕様）：
// - kAttackPower：そのターン限りの攻撃力+1
// - kCostFixed：移動コスト+2固定
// - kCostRisky：踏んだ瞬間に50%の確率で移動コスト±4（-4を引いてもコストは最低1で下げ止まる）
class GridItemComponent : public IComponent {
public:
	enum class Type { kAttackPower, kCostFixed, kCostRisky };

	Type type = Type::kAttackPower;

	// 配置マス座標（GridBoardComponent::GridToWorldでワールド座標に変換する）。
	// GameObjectのTransformとは別に持つ理由：GridBoardPlayerComponentが経路予約時に
	// 「このマスにアイテムがあるか」を列/行の整数比較だけで判定できるようにするため
	// （ワールド座標の浮動小数点比較を避ける）
	int col = 0;
	int row = 0;

	// 見た目色。GridPuzzleScene::EnsureInitialObjectsExistが配置時にtype別の既定色
	// （攻撃力=赤、コスト固定=青、コストリスキー=紫）を初期値として渡すが、Inspectorから
	// 自由に上書きできる。実際の描画は兄弟のCubeRenderComponent::colorが担うため、
	// GridPuzzleSceneがこの値をCubeRenderComponent::colorへ毎フレーム同期する
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
