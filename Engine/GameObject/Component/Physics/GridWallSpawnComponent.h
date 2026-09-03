// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"

// パイプ接続パズル企画の壁スポーン設定を、Inspectorから調整できるようにするコンポーネント。
// GridItemSpawnComponentと同じ方針：「盤面に常に何個の壁ブロックを存在させ続けるか」という
// 設定値の置き場所のみを持ち、実際の生成・再配置ロジック（形状の抽選・空きマスの抽選・
// GameObjectの生成・親子付け）はGridPuzzleScene側に置いたままにする（「シーン全体を走査して
// GameObjectを生成する」処理は1つのGameObjectに属さないため、IComponentよりシーンクラス側の
// 責務として扱う既存方針を踏襲する）。
//
// 壁は単独の1マスではなく、テトリスのミノ（4マス連結のI/O/T/S/Z/J/L形、ランダムな向きで回転）
// 単位で配置される（GridPuzzleScene::SpawnWallsFromConfig参照）。pieceCountは「そのミノを
// いくつ配置するか」であって、生成されるGridWallComponent（1マス分）の総数はpieceCount×4になる。
//
// 生成された壁（GridWallComponent付きGameObject、1マスにつき1個）は、このコンポーネントが付いた
// GameObject（スポナー）の子オブジェクトとしてぶら下がる（GridPuzzleScene::RespawnWallsIfNoneExistが
// SetParentする）。アイテムと違い、壁は一度配置したら盤面リセット（今回のスコープ外）や
// Inspectorの「リセット」ボタンを押すまでそのまま居座り続ける（プレイヤーが踏んでも消えない）
class GridWallSpawnComponent : public IComponent {
public:
	// 盤面上に同時に存在させ続けるテトロミノ形の壁ブロックの個数（1個＝4マス連結）。
	// GridPuzzleScene::RespawnWallsIfNoneExist/SpawnWallsFromConfigが（盤面上にtag==kGridWallTagが
	// 1つも無い時、またはリセットボタンが押された時）この個数ぶん、ランダムな形状・向き・位置で
	// 壁ブロックを生成する（各ブロックが4マスの壁を生む）
	int pieceCount = 3;

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
	// 全部削除してからpieceCount個ぶん新しく配置し直す。実行時の一時要求のためToJson/FromJsonでは保存しない
	bool ConsumeResetRequested() {
		bool result = resetRequested_;
		resetRequested_ = false;
		return result;
	}

private:
	bool resetRequested_ = false;
};
