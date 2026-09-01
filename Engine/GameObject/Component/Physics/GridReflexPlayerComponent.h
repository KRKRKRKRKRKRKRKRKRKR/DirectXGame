// 10DaysJam
#pragma once
#include "ReflexPlayerComponent.h"
#include <utility>
#include <vector>

// パイプ接続パズル企画の「プレイヤー移動」用コンポーネント。REFLEXのReflexPlayerComponent
// （マウスクリックで経路を予約→最大maxWaypoints個たまったら実行フェーズへ→イージングで直進、
// というフェーズ機構一式）をそのまま継承して使う。ReflexPlayerComponent自体（既存REFLEXゲームが
// 使用中）は改造せず、クリック位置の妥当性判定（TryPickPoint、protected virtualにしてある）
// だけをオーバーライドし、「最寄りのマスへスナップした上で、直前の予約地点（無ければ現在地）と
// 同じ行/列上・minJumpDistance〜maxJumpDistanceマス先」を満たさないクリックを無効化する。
// フェーズ管理・イージング移動・経路マーカー/破線の描画（ReflexPathVisualizer）・障害物判定
// （IsPathBlocked、コライダーが無ければ何もしない）は基底クラスのロジックをそのまま使う
class GridReflexPlayerComponent : public ReflexPlayerComponent {
public:
	GridReflexPlayerComponent();

	// プレイヤーが移動できる範囲（列数×行数、マス(0,0)〜(gridWidth-1,gridHeight-1)）。
	// GridPuzzleScene::GridBoardComponentの見た目（タイルの列数・行数）とは意図的に自動同期しない
	// （互いに独立してInspectorから調整できればよい）。GridToWorld自体はこの値に依存しない絶対
	// 座標（マス(0,0)が常にワールド原点）のため、盤面側とこの値が一致していなくても、タイルの
	// 見た目とプレイヤーの移動先判定がズレることはない
	int gridWidth = 5;
	int gridHeight = 15;

	// マス目の中心間隔（ワールド単位）。GridPuzzleScene側のタイル配置と必ず同じ値にする
	// （ズレるとタイルの見た目とプレイヤーの移動先判定がかみ合わなくなる）
	float cellSpacing = 1.5f;

	// 1回のジャンプで有効な移動距離（マス数）の範囲。企画書の「1〜4マス先へジャンプ」通り、
	// これより近い/遠いクリックは無効な移動として扱う（同じマスへのクリック＝距離0も無効）
	int minJumpDistance = 1;
	int maxJumpDistance = 4;

	// GridPuzzleScene::UpdateTileHighlightsが「次にクリックできるマス」を塗る色。
	// このコンポーネント自身は描画しない（Scene側がGetValidTargets()と合わせて参照するだけの値）
	Vector4 highlightColor = { 0.55f, 0.95f, 0.35f, 1.0f };

	void DrawImGui(const char* namePrefix) override;
	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	// 計画フェーズ中のみ、「直前の予約地点（無ければ現在地）から見て次にクリックできるマス座標」
	// の一覧を返す（同じ行/列上、距離minJumpDistance〜maxJumpDistanceマス、盤面内の組み合わせ
	// 全て）。GridPuzzleScene側がこの一覧に含まれるタイルをハイライト表示するために使う。
	// このコンポーネントは自分のワールド座標を持たないため、呼び出し側がオーナーの現在Transformを
	// 渡す（計画フェーズ以外、または既に予約数が上限の場合は空を返す＝ハイライト無し）
	std::vector<std::pair<int, int>> GetValidTargets(const Transform& transform) const;

protected:
	// 基底クラスのTryPickPoint（マウスレイ→地面Planeとの交点、fieldRange制限）を呼んだ後、
	// 結果を最寄りのマスへスナップし、「直前の予約地点と同じ行/列上・minJumpDistance〜
	// maxJumpDistanceマス先」「盤面内」を満たさない場合は無効なクリックとして扱う
	bool TryPickPoint(const Transform& transform, const UpdateContext& ctx, Vector3& outPosition) const override;

private:
	// (col,row)をワールド座標(x,y)へ変換する（zは常に0を返す、呼び出し側はx/yだけ使う）
	Vector3 GridToWorld(int col, int row) const;

	// ワールド座標から最寄りのマス座標(col,row)を求める（GridToWorldの逆変換）
	void WorldToNearestGrid(const Vector3& worldPos, int& outCol, int& outRow) const;
};
