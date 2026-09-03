// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// パイプ接続パズル企画の壁スポーン設定を、Inspectorから調整できるようにするコンポーネント。
// GridItemSpawnComponentと同じ方針：「盤面に常に何個の壁を存在させ続けるか」という設定値の
// 置き場所のみを持ち、実際の生成・再配置ロジック（空きマスの抽選・GameObjectの生成・親子付け）は
// GridPuzzleScene側に置いたままにする（「シーン全体を走査してGameObjectを生成する」処理は
// 1つのGameObjectに属さないため、IComponentよりシーンクラス側の責務として扱う既存方針を踏襲する）。
// GridItemSpawnComponentと違い種別は1種類のみ（壁に「種類」の概念は無く、通過コストが一律の
// 単純な障害物マスのため、SpawnEntryのような一覧構造は持たない）。
//
// 生成された壁（GridWallComponent付きGameObject）は、このコンポーネントが付いたGameObject
// （スポナー）の子オブジェクトとしてぶら下がる（GridPuzzleScene::RespawnWallsIfNoneExistが
// SetParentする）。アイテムと違い、壁は一度配置したら盤面リセット（今回のスコープ外）や
// Inspectorの「リセット」ボタンを押すまでそのまま居座り続ける（プレイヤーが踏んでも消えない）
class GridWallSpawnComponent : public IComponent {
public:
	// 盤面上に同時に存在させ続ける壁の数。GridPuzzleScene::RespawnWallsIfNoneExist/
	// SpawnWallsFromConfigが（盤面上にtag==kGridWallTagが1つも無い時、またはリセットボタンが
	// 押された時）この個数ぶんランダムな空きマスへ壁を生成する
	int wallCount = 5;

	// 壁を通過する（＝経路の一部として通る）際に消費する移動コスト（通常のマスは距離1マスに
	// つきコスト1）。生成する壁（GridWallComponent::passCost）の初期値として使う。生成後は
	// 壁ごとにInspectorから個別に上書きできる（ここでの値はあくまで新規生成時の初期値）
	int passCost = 3;

	// 壁の見た目色。生成する壁（GridWallComponent::color）の初期値として使う
	Vector4 wallColor = { 0.35f, 0.32f, 0.4f, 1.0f };

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// DrawImGuiの「リセット」ボタンが押された瞬間だけtrueを返し、呼ぶとフラグを消費する
	// （GridItemSpawnComponent::ConsumeResetRequestedと同じワンショットの取り出し方）。
	// GridPuzzleScene::ResetWallsIfRequestedが毎フレームこれを見て、trueなら現在盤面にある壁を
	// 全部削除してからwallCount枚新しく配置し直す。実行時の一時要求のためToJson/FromJsonでは保存しない
	bool ConsumeResetRequested() {
		bool result = resetRequested_;
		resetRequested_ = false;
		return result;
	}

private:
	bool resetRequested_ = false;
};
