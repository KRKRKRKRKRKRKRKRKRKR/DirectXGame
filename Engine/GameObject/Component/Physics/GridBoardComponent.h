// 10DaysJam
#pragma once
#include "../../IComponent.h"

// パイプ接続パズル企画のプロトタイプ盤面（GridPuzzleScene）のサイズ・色設定だけを持つ、
// ごく小さなコンポーネント。GridPuzzleScene::HandleSceneTransitionInputが毎フレームcolumns/rowsを
// 読み、直前にタイルを組み立てた時点の値と食い違っていたらタイルを作り直す（＝Inspectorで
// この値を変えるだけで盤面の大きさがその場で変わる）。tileColorA/Bはタイルの市松模様の色で、
// タイル自身（excludeFromSave=trueで保存対象外、毎回作り直される）ではなくこちらに持たせることで、
// 色を変えてシーンを保存すれば次回起動時も反映されるようにしている。
// 意図的にGridReflexPlayerComponent::gridWidth/gridHeightとは同期させない（プレイヤーの
// 移動範囲と盤面の見た目は互いに独立して調整できるようにする）。壁配置等、盤面固有のデータを
// 今後増やす場合の置き場としてもこのコンポーネントを使う想定
class GridBoardComponent : public IComponent {
public:
	int columns = 5;
	int rows = 15;

	// タイルの市松模様に使う2色
	Vector4 tileColorA = { 0.80f, 0.82f, 0.86f, 1.0f };
	Vector4 tileColorB = { 0.55f, 0.58f, 0.64f, 1.0f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;
};
