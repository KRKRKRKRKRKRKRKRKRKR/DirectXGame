// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// パイプ接続パズル企画のプロトタイプ盤面（GridPuzzleScene）のサイズ・マス間隔・色設定を持つ、
// 盤面仕様の唯一のデータソース。GridPuzzleScene::HandleSceneTransitionInputが毎フレーム
// columns/rows/cellSpacingを読み、直前にタイルを組み立てた時点の値と食い違っていたらタイルを
// 作り直す（＝Inspectorでこの値を変えるだけで盤面の大きさがその場で変わる）。tileColorA/Bは
// タイルの市松模様の色で、タイル自身（excludeFromSave=trueで保存対象外、毎回作り直される）
// ではなくこちらに持たせることで、色を変えてシーンを保存すれば次回起動時も反映されるようにしている。
//
// 以前のプレイヤー移動コンポーネント（旧GridReflexPlayerComponent、削除済み）は自分専用の
// gridWidth/gridHeight/cellSpacingを別に持っていたが、これがタイル側（GridPuzzleScene::
// RebuildTiles）の数値と食い違うと、見た目と移動判定がズレるバグの原因になっていた。
// 現在はこのコンポーネントを盤面仕様の唯一の真実源とし、プレイヤー移動用の専用コンポーネントを
// 実装する際もUpdateContext::sceneObjects経由でこれを検索して参照すること（数値を複製しない）。
// 壁配置等、盤面固有のデータを今後増やす場合の置き場としてもこのコンポーネントを使う想定
class GridBoardComponent : public IComponent {
public:
	int columns = 5;
	int rows = 15;

	// マス目の中心間隔（ワールド単位）。タイル生成（GridPuzzleScene::RebuildTiles）と
	// プレイヤーの移動判定（専用コンポーネント実装後）の両方がこの1つの値だけを参照する想定
	float cellSpacing = 1.5f;

	// タイルの市松模様に使う2色
	Vector4 tileColorA = { 0.80f, 0.82f, 0.86f, 1.0f };
	Vector4 tileColorB = { 0.55f, 0.58f, 0.64f, 1.0f };

	// マス(0,0)が常にワールド原点になる固定アンカー方式の座標変換。行は値が大きいほど下
	// （Y-方向）へ進む（見た目の「上から下へ数える」感覚に合わせる）。タイル生成・プレイヤー
	// 移動判定のどちらも必ずこの関数経由で計算し、式を手書きで複製しない
	Vector3 GridToWorld(int col, int row) const {
		return { static_cast<float>(col) * cellSpacing, -static_cast<float>(row) * cellSpacing, 0.0f };
	}

	// GridToWorldの逆変換。四捨五入で最寄りのマスへスナップする
	void WorldToNearestGrid(const Vector3& worldPos, int& outCol, int& outRow) const;

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
