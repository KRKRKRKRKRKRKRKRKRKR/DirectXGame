#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Easing.h"
#include <vector>

// 光反射アクションパズル「REFLEX」のプレイヤー操作コンポーネント（最小実装）。
// このゲームはX-Y平面上で進行する（Z座標は固定、カメラはZ軸方向遠くに置いて正面から見る）。
//
// 企画書2章「計画フェーズ→実行フェーズ」に対応する状態機：
// - 計画フェーズ（kPlanning）：左クリックするたびにwaypoints_へ地点を追加する（最大kMaxWaypoints個）。
//   プレイヤー位置→1番目→2番目…と連鎖する経路になる。各地点に球体マーカー、地点間に線を描画する。
//   プレイヤーはまだ動かない
// - 実行フェーズ（kExecuting）：waypoints_を先頭から順に自動で直進する。1区間の直進には
//   Easing::Type（https://easings.net/ja 準拠）を適用する。1つに到達したら次へ進み、
//   すべて終えても自動でフェーズは戻らない（Inspectorのボタンで手動切り替え）
// - 準備フェーズ（kPreparing）：実行フェーズが完了した直後に自動で入る、プレイヤー操作を
//   受け付けない待機フェーズ。PlayScene側がこの間に「直前の実行フェーズ中に倒した敵の数」だけ
//   1体ずつ間隔を空けて補充スポーンする演出を行い、全部出し終えたらFinishPreparing()を呼んで
//   計画フェーズへ戻す（このコンポーネント自身は敵のスポーンには関与しない、フェーズの
//   受け渡し役に徹する）
// コスト管理・鏡での反射は後続で追加する
class ReflexPlayerComponent : public IComponent {
public:
	enum class Phase { kPlanning, kExecuting, kPreparing };

	// 計画フェーズで予約できる地点の最大数（企画書2章のコスト4に対応する仮の上限）
	static constexpr size_t kMaxWaypoints = 4;

	// 1秒あたりの移動距離。区間の所要時間 = 区間の距離 / moveSpeed として使う
	// （直線移動の「平均速度」に相当。イージングが強くかかるほど体感速度にメリハリが付く）
	float moveSpeed = 5.0f;

	// 実行フェーズの直進に適用するイージングの種類（Inspectorのコンボで選択）
	Easing::Type easingType = Easing::Type::kLinear;

	// 障害物判定（IsPathBlocked）に持たせる安全マージン。クリック地点が障害物ぎりぎりだと、
	// 実際にそこへ到達した際にColliderSystemの押し戻しとプレイヤーの直進ロジックが競合して
	// その場で振動する（押し戻される→また進もうとする、の繰り返し）ため、判定の時点で
	// 障害物の見た目より少し広い範囲を「遮られている」とみなして回避する
	float obstacleMargin = 0.5f;

	void Update(float deltaTime, Transform& transform, const UpdateContext& ctx) override;
	void DrawImGui(const char* namePrefix) override;

	// 実行フェーズが最後の地点まで完了して準備フェーズ（kPreparing）に自動で移った直後の
	// 1回だけtrueを返し、呼び出すとフラグを消費する（ワンショット）。PlayScene側が
	// 「実行フェーズ完了の瞬間」を検知して、準備フェーズ中の敵補充スポーンを開始するために使う
	bool ConsumeExecutionFinished() {
		bool result = executionFinished_;
		executionFinished_ = false;
		return result;
	}

	// 現在のフェーズを返す。PlayScene側がkPreparing中かどうかを判定するために使う
	Phase GetPhase() const { return phase_; }

	// 準備フェーズ（kPreparing）から計画フェーズ（kPlanning）へ戻す。PlayScene側が
	// 「直前の実行フェーズで倒した敵の補充スポーンを全部出し終えた」タイミングで呼ぶ。
	// kPreparing以外の状態で呼ばれても何もしない（呼び出し側の判定ミスを黙って無視する）
	void FinishPreparing() {
		if (phase_ == Phase::kPreparing) phase_ = Phase::kPlanning;
	}

	// moveSpeed/easingType/obstacleMarginのみ保存対象（設定値）。waypoints_/phase_等は
	// 実行時の一時状態のため保存しない（GravityComponent::gravity等、他コンポーネントと同じ
	// 「設定値だけ保存する」方針を踏襲）
	void ToJson(nlohmann::json& out) const override {
		out["moveSpeed"] = moveSpeed;
		out["easingType"] = static_cast<int>(easingType);
		out["obstacleMargin"] = obstacleMargin;
	}
	void FromJson(const nlohmann::json& in) override {
		moveSpeed = in.value("moveSpeed", moveSpeed);
		easingType = static_cast<Easing::Type>(in.value("easingType", static_cast<int>(easingType)));
		obstacleMargin = in.value("obstacleMargin", obstacleMargin);
	}

private:
	bool prevMouseLeftPressed_ = false;

	Phase phase_ = Phase::kPlanning;

	std::vector<Vector3> waypoints_;  // 計画フェーズで予約した経路（プレイヤー位置は含まない）
	size_t currentWaypointIndex_ = 0; // 実行フェーズで現在向かっている地点のインデックス

	// 実行フェーズ中の現在区間の補間状態。segmentStart_→segmentEnd_をsegmentElapsed_/segmentDuration_
	// （0〜1にクランプ）にEasing::Applyを適用した割合でLerpする。
	// segmentStarted_は「今の区間についてBeginSegmentを呼び終えたか」を示す。DrawImGuiの
	// 「実行フェーズへ」ボタンはtransformを持たずBeginSegmentを直接呼べないため、
	// Update側で「実行フェーズだが区間が未開始」を検出して初回フレームに呼ぶために使う
	Vector3 segmentStart_{ 0.0f, 0.0f, 0.0f };
	Vector3 segmentEnd_{ 0.0f, 0.0f, 0.0f };
	float   segmentElapsed_ = 0.0f;
	float   segmentDuration_ = 0.0f;
	bool    segmentStarted_ = false;

	// 実行フェーズが完了して計画フェーズに自動で戻った瞬間にtrueへ立てる。ConsumeExecutionFinished()で
	// 読み取られると消費されてfalseに戻る（ワンショット）
	bool executionFinished_ = false;

	// currentWaypointIndex_が指す区間の開始・終了・所要時間をセットし、経過時間をリセットする
	void BeginSegment(const Vector3& from, const Vector3& to);

	// 左クリックした地点（プレイヤーと同じZ座標のX-Y平面上）を求める。
	// 成功したらtrueを返し、outPositionにワールド座標を書き込む
	bool TryPickPoint(const Transform& transform, const UpdateContext& ctx, Vector3& outPosition) const;

	// fromからtoまでの線分上に、CollisionLayer::kObstacleのColliderComponentBaseを持つ
	// GameObjectが存在するかどうか。存在すればtrue（＝その方向へは進めない）
	bool IsPathBlocked(const Vector3& from, const Vector3& to, const UpdateContext& ctx) const;

	// 計画フェーズ中の可視化：waypoints_の各地点に球体マーカー、地点間（プレイヤー位置含む）に線を描く
	void DrawPlanningVisualization(const Transform& transform, const UpdateContext& ctx) const;

	// 計画フェーズ中の可視化：ctx.sceneObjects内の全障害物（CollisionLayer::kObstacle）について、
	// IsPathBlockedが実際に判定に使っているマージン込みの形状（実サイズ+obstacleMargin）を
	// ワイヤーフレームで描く。クリック前に「ここまでは安全、ここからは弾かれる」領域を一目で分かるようにする
	void DrawObstacleMarginVisualization(const UpdateContext& ctx) const;
};
