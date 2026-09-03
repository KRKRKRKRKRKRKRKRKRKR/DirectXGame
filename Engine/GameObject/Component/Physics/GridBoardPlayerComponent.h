// 10DaysJam
#pragma once
#include "../../IComponent.h"
#include "GridItemComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Easing.h"
#include "../../../../Math/JsonUtil.h"
#include <random>
#include <utility>
#include <vector>

class GameObject;

// パイプ接続パズル企画の「プレイヤー移動」用コンポーネント。ReflexPlayerComponent（REFLEXゲーム用）
// には一切依存しない独立実装。GridBoardComponent（盤面サイズ・マス間隔の唯一のデータソース）を
// UpdateContext::sceneObjects経由で参照し、盤面上のマスをクリックして移動する。
//
// フェーズは「計画（kPlanning）→実行（kExecuting）」の2段階。計画フェーズ中、直前の予約地点
// （無ければ現在地）と同じ行/列上のマスをクリックすると、そのマスまでの距離（マス数）ぶん
// currentCost_をその場で即時消費して経路に予約する。残りコストを超える距離のクリックは無効。
// currentCost_を使い切った（0になった）瞬間、それ以上予約できないため自動的に実行フェーズへ移る
// （Inspectorの「実行フェーズへ」ボタンを押さなくてもよい）。実行フェーズでは予約した経路を
// 先頭から順にイージング移動し、すべて終えたら計画フェーズへ戻り、同時にcurrentCost_を
// maxCostへリセットする（次ターンの開始）。
//
// アイテム（GridItemComponent）：発動判定はマス座標比較ではなく、ColliderSystemによる
// OBBCollider同士の当たり判定（isTrigger=true）で行う。プレイヤー・アイテム双方のGameObjectに
// OBBColliderComponentが付与されている前提で、実際に重なった瞬間にGridItemComponent::
// OnTriggerEnterが呼ばれ、そこからこのコンポーネントのApplyItemEffect()を呼んで効果を適用する
// （kAttackPower：attackPower_+1、kCostFixed：currentCost_+2、kCostRisky：50%でcurrentCost_±4、
// 下限1でクランプ）。発動したアイテムのGameObject削除はGridItemComponent::triggeredフラグ経由で
// GridPuzzleScene側が行う（このコンポーネントはアイテムGameObjectの削除には一切関与しない）。
// 「経路をクリア」しても発動済みの効果（attackPower_・コスト増減）は巻き戻さない
// （アイテムは拾ったままにする）。
//
// 経路の可視化は波紋マーカー・破線（ReflexPathVisualizer、REFLEX本編と共有の描画部品）を
// 使わない。GridPuzzleScene::UpdateTileHighlightsがGetReservedWaypoints()を参照して、
// 予約済みマス自体をタイルの色塗りで表現する方式にしている（このコンポーネント自身は
// 経路の描画を一切行わない）。
//
// 壁マスのコスト追加消費・アイテム・ダメージ・盤面リセットは今回のスコープ外
// （docs/ComponentPlanTemplate.mdの計画書参照。実装は次段階で追加する）
class GridBoardPlayerComponent : public IComponent {
public:
	enum class Phase { kPlanning, kExecuting };

	// 1ターンあたりの移動コスト上限。ターン開始（実行フェーズ完了）のたびにcurrentCost_へ
	// この値が補充される
	int maxCost = 10;

	// 1秒あたりの移動距離。区間の所要時間 = 区間の距離 / moveSpeed
	float moveSpeed = 5.0f;

	// 実行フェーズの直進に適用するイージングの種類
	Easing::Type easingType = Easing::Type::kLinear;

	// GridPuzzleScene::UpdateTileHighlightsが「次にクリックできるマス」を塗る色。
	// このコンポーネント自身は描画しない（Scene側がGetValidTargets()と合わせて参照するだけの値）
	Vector4 highlightColor = { 0.55f, 0.95f, 0.35f, 1.0f };

	// GridPuzzleScene::UpdateTileHighlightsが「予約済みの経路マス」を塗る色。highlightColorとは
	// 別の色にして、次に選べるマスと既に予約済みのマスを見分けられるようにする
	Vector4 reservedColor = { 1.0f, 0.85f, 0.2f, 1.0f };

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override;
	void FromJson(const nlohmann::json& in) override;

	Phase GetPhase() const { return phase_; }
	int GetCurrentCost() const { return currentCost_; }

	// このターン中に発動したkAttackPowerアイテムの合計値。ターン開始（実行フェーズ完了）の
	// たびに0へリセットされる。GridPuzzleScene側がダメージ計算に使う想定（ダメージ計算自体は
	// 今回のスコープ外で、値を読めるようにするだけ）
	int GetAttackPower() const { return attackPower_; }

	// 計画フェーズ中のみ、「直前の予約地点（無ければ現在地）と同じ行/列上」にあり、かつ
	// 残りコストで到達可能な盤面端までの全マスの一覧を返す（縦横、盤面外は含まない）。
	// GridPuzzleScene側がこの一覧に含まれるタイルをハイライト表示するために使う。このコンポーネントは
	// 自分のワールド座標を持たないため、呼び出し側がオーナーの現在Transformを渡す
	// （計画フェーズ以外の場合は空を返す＝ハイライト無し）。sceneObjectsはGridBoardComponentを
	// 検索するためのシーン内GameObject一覧（UpdateContext::sceneObjectsをそのまま渡す想定）。
	// 盤面が見つからない場合は空を返す
	std::vector<std::pair<int, int>> GetValidTargets(const Transform& transform, const std::vector<GameObject*>* sceneObjects) const;

	// 現在予約済みの経路（列,行）一覧をそのまま返す。GridPuzzleScene::UpdateTileHighlightsが
	// 予約済みマスをreservedColorで塗るために使う（実行フェーズ中は「まだ通過していない」
	// 残りの区間のみを返す想定はしていない。全区間を返し続けるが、実行フェーズ中は
	// GetValidTargets()が空になるため見分けが付く）
	const std::vector<std::pair<int, int>>& GetReservedWaypoints() const { return waypoints_; }

	// GridItemComponent::OnTriggerEnterから呼ばれる。渡されたアイテムの種別に応じて効果を
	// 即時適用する（kAttackPower：attackPower_+1、kCostFixed：currentCost_+2、kCostRisky：
	// 50%でcurrentCost_±4、下限1でクランプ）。呼び出し元（GridItemComponent）が自分自身の
	// triggeredフラグを立てて削除待ちにするため、ここではアイテムGameObjectの削除には関与しない
	void ApplyItemEffect(GridItemComponent::Type type);

private:
	Phase phase_ = Phase::kPlanning;
	int currentCost_ = maxCost;

	bool prevMouseLeftPressed_ = false;
	bool isFirstUpdate_ = true;

	std::vector<std::pair<int, int>> waypoints_; // 予約した経路（列,行）。プレイヤーの現在地は含まない
	size_t currentWaypointIndex_ = 0;

	Vector3 segmentStart_{ 0.0f, 0.0f, 0.0f };
	Vector3 segmentEnd_{ 0.0f, 0.0f, 0.0f };
	float   segmentElapsed_ = 0.0f;
	float   segmentDuration_ = 0.0f;
	bool    segmentStarted_ = false;

	int attackPower_ = 0; // このターン中に発動したkAttackPowerアイテムの合計（実行時状態、非保存）
	std::mt19937 rng_{ std::random_device{}() }; // kCostRiskyの±4抽選用

	// 左クリック位置をレイキャストし、盤面上の最寄りマス（列,行）にスナップする。
	// 成功したらtrueを返す。盤面（GridBoardComponent）が見つからない場合は常にfalse
	bool TryPickCell(const Transform& transform, const UpdateContext& ctx, int& outCol, int& outRow) const;

	void BeginSegment(const Vector3& from, const Vector3& to);

	// waypoints_を全部消し、消費済みコストを予約前の状態へ戻す（DrawImGuiの「経路をクリア」用）
	void ClearWaypoints();
};
