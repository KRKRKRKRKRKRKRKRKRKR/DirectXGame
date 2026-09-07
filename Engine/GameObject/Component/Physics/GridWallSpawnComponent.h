// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// パイプ接続パズル企画の壁スポーン設定を、Inspectorから調整できるようにするコンポーネント。
// GridItemSpawnComponentと同じ方針：設定値の置き場所のみを持ち、実際の生成・再配置ロジック
// （GameObjectの生成・親子付け）はGridPuzzleScene側に置いたままにする（「シーン全体を走査して
// GameObjectを生成する」処理は1つのGameObjectに属さないため、IComponentよりシーンクラス側の
// 責務として扱う既存方針を踏襲する）。
//
// 壁は7x7盤面専用の手動デザインした複数の固定パターン（基本形、各10マス前後）を持ち、
// それぞれを90度回転4通り×左右反転2通り（GridPuzzleScene::FixedWallPatternVariants、
// 基本形1つあたり最大8種類）に展開した中からランダムに1つを選んで配置する。
//
// 生成された壁（GridWallComponent付きGameObject、1マスにつき1個）は、このコンポーネントが付いた
// GameObject（スポナー）の子オブジェクトとしてぶら下がる（GridPuzzleScene::RespawnWallsIfNoneExistが
// SetParentする）。アイテムと違い、壁は一度配置したら盤面リセット（今回のスコープ外）や
// Inspectorの「リセット」ボタンを押すまでそのまま居座り続ける（プレイヤーが踏んでも消えない）
class GridWallSpawnComponent : public IComponent {
public:
	// 過去のランダムテトロミノ生成方式で使っていた設定値の名残。壁は現在GridPuzzleScene::
	// FixedWallPatternVariants（7x7盤面専用の手動デザイン複数パターンを回転・反転した
	// バリエーションからランダム選択）を配置するため参照されない。JSON後方互換のためフィールドは残す
	int pieceCount = 3;

	// 壁を通過する（＝経路の一部として通る）際に消費する移動コスト（通常のマスは距離1マスに
	// つきコスト1）。生成する壁（GridWallComponent::passCost）の初期値として使う。生成後は
	// 壁ごとにInspectorから個別に上書きできる（ここでの値はあくまで新規生成時の初期値）。
	// impassable=trueの間はこの値は参照されない（GridWallComponent::passCostと同じ扱い）。
	// 既定3では簡単すぎたため5に上げた（行動可能マス20は変えず、壁越えの負担を増やす方向で調整）
	int passCost = 5;

	// trueにすると、今後SpawnWallsFromConfigで新しく生成される壁がimpassable=true
	// （超えられない壁）になる。「切り替え」ボタンを押すと、現在盤面上にある既存の全壁の
	// impassableもこの値へ一括で合わせる（GridPuzzleScene::ApplyImpassableToggleIfRequested）
	bool impassable = false;

	// 壁の見た目色。生成する壁（GridWallComponent::color）の初期値として使う
	Vector4 wallColor = { 0.35f, 0.32f, 0.4f, 1.0f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// DrawImGuiの「リセット」ボタンが押された瞬間だけtrueを返し、呼ぶとフラグを消費する
	// （GridItemSpawnComponent::ConsumeResetRequestedと同じワンショットの取り出し方）。
	// GridPuzzleScene::ResetWallsIfRequestedが毎フレームこれを見て、trueなら現在盤面にある壁を
	// 全部削除してからpieceCount個ぶん新しく配置し直す。実行時の一時要求のためToJson/FromJsonでは保存しない
	bool ConsumeResetRequested() {
		bool result = resetRequested_;
		resetRequested_ = false;
		return result;
	}

	// DrawImGuiの「超えられる／超えられないを切り替え」ボタンが押された瞬間だけtrueを返し、
	// 呼ぶとフラグを消費する。GridPuzzleScene::ApplyImpassableToggleIfRequestedが毎フレーム
	// これを見て、trueなら現在盤面上にある全GridWallComponent::impassableをこのimpassableの
	// 値へ一括で合わせる（削除・再生成はしない、フラグの書き換えのみ）
	bool ConsumeImpassableToggleRequested() {
		bool result = impassableToggleRequested_;
		impassableToggleRequested_ = false;
		return result;
	}

private:
	bool resetRequested_ = false;
	bool impassableToggleRequested_ = false;
};
