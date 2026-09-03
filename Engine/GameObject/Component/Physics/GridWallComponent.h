// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// パイプ接続パズル企画の盤面壁。GridItemComponentと違い、プレイヤーの通行を完全に塞ぐ
// 障害物（Collider+isTrigger方式）ではなく、「コストを多く払えば通り抜けられる」マスの
// 目印にすぎない、純粋なデータ+見た目コンポーネント。実際の判定はGridBoardPlayerComponentが
// GridBoardPlayerComponent::Update/GetValidTargets内で、移動先までの経路上にある各マスの
// col/rowをこのコンポーネントと直接比較して行う（GridItemComponentのようなColliderの
// OnTriggerEnterは使わない。壁は「踏んだ瞬間に何か起きる」のではなく「そこを通るのに
// 何コスト必要か」という経路計算の入力値のため）。
class GridWallComponent : public IComponent {
public:
	// 配置マス座標（GridBoardComponent::GridToWorldでワールド座標に変換する）。GameObjectの
	// Transformとは別に持つ理由はGridItemComponent::col/rowと同じ（GridPuzzleScene::SyncWallsが
	// 毎フレームTransform.translationへ反映する）
	int col = 0;
	int row = 0;

	// このマスを通過する（＝経路の一部として通る）際に消費する移動コスト。通常のマスは
	// 距離1マスにつきコスト1だが、壁マスはこの値がその1マス分のコストとして使われる
	// （GridBoardPlayerComponent::Update・GetValidTargetsが経路上の各マスでこの値を参照する）
	int passCost = 3;

	// 見た目色。GridWallSpawnComponent::wallColorが生成時の初期値だが、生成後はInspectorから
	// 個別に上書きできる。実際の描画は兄弟のCubeRenderComponent::colorが担うため、
	// GridPuzzleScene::SyncWallsがこの値をCubeRenderComponent::colorへ毎フレーム同期する
	Vector4 color = { 0.35f, 0.32f, 0.4f, 1.0f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
