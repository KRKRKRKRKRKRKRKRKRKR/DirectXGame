#pragma once
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Easing.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include <vector>

// 光反射アクションパズル「REFLEX」のプレイヤー操作コンポーネント（最小実装）。
// このゲームはX-Y平面上で進行する（Z座標は固定、カメラはZ軸方向遠くに置いて正面から見る）。
//
// 企画書2章「計画フェーズ→実行フェーズ」に対応する状態機：
// - 計画フェーズ（kPlanning）：左クリックするたびにwaypoints_へ地点を追加する（最大maxWaypoints個、
//   Inspectorで調整可能）。プレイヤー位置→1番目→2番目…と連鎖する経路になる。各地点に球体マーカー、
//   地点間に線を描画する。プレイヤーはまだ動かない
// - 実行準備フェーズ（kReadyToExecute）：maxWaypoints個目を予約した直後に自動で入る、
//   readyToExecuteDelay秒だけ待つ短い遷移演出。この間はクリックを受け付けない（waypoints_が
//   既に上限のため計画フェーズと同じUIでもクリックしても地点を追加できないのは同じだが、
//   ワンテンポ置くことで「地点が確定した」ことを視覚的に分かりやすくする狙い）
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
	enum class Phase { kPlanning, kReadyToExecute, kExecuting, kPreparing };

	// 計画フェーズで予約できる地点の最大数（企画書2章のコスト4に対応する仮の上限）。
	// 以前はstatic constexprで固定4だったが、Inspectorから調整できるようにした
	int maxWaypoints = 4;

	// 1秒あたりの移動距離。区間の所要時間 = 区間の距離 / moveSpeed として使う
	// （直線移動の「平均速度」に相当。イージングが強くかかるほど体感速度にメリハリが付く）
	float moveSpeed = 5.0f;

	// 実行フェーズの直進に適用するイージングの種類（Inspectorのコンボで選択）
	Easing::Type easingType = Easing::Type::kLinear;

	// kMaxWaypoints個目を予約してから実行フェーズへ移るまでの待機時間（秒）。0にすると
	// 従来通り即座に実行フェーズへ移る
	float readyToExecuteDelay = 0.5f;

	// 障害物判定（IsPathBlocked）に持たせる安全マージン。クリック地点が障害物ぎりぎりだと、
	// 実際にそこへ到達した際にColliderSystemの押し戻しとプレイヤーの直進ロジックが競合して
	// その場で振動する（押し戻される→また進もうとする、の繰り返し）ため、判定の時点で
	// 障害物の見た目より少し広い範囲を「遮られている」とみなして回避する
	float obstacleMargin = 0.5f;

	// クリックで予約できる移動範囲（X/Yそれぞれの最小・最大）。TryPickPointがこの範囲外の
	// クリックを障害物と同じ扱いで無効化する（＝そこへは経路予約できない）。既定値はPlayScene::
	// kSpawnRangeMin/Max（敵のスポーン範囲）と同じ±8で、壁の内側に収まる想定
	float fieldRangeMinX = -8.0f;
	float fieldRangeMaxX = 8.0f;
	float fieldRangeMinY = -8.0f;
	float fieldRangeMaxY = 8.0f;

	// 計画フェーズの経路マーカー（Resources/Model/Circle.objを表示する）の色。Inspectorの
	// ColorEditで調整できる（既定は白）。モデルのロードに失敗した場合は使われず、
	// 代わりに従来の黄色い球体（DrawSphere、kWaypointMarkerColor）にフォールバックする
	Vector4 markerColor = { 1.0f, 1.0f, 1.0f, 1.0f };

	// マーカーの波紋アニメーション（水面の波紋のように、小さい状態から広がりながら透明になって
	// 消え、また小さい状態から再発生することを繰り返す）の最小/最大スケールと1本あたりの周期（秒）。
	// DrawPlanningVisualizationがこの範囲・周期でscaleを0→1（Easing::kInOutSine）、
	// アルファを1→0で変化させる
	float markerPulseMinScale = 0.15f;
	float markerPulseMaxScale = 0.3f;
	float markerPulseDuration = 1.0f;

	// 1つのマーカー地点に同時発生させる波紋の本数。markerPulseDurationをこの本数で均等に
	// 位相をずらして発生させる（波紋が重なって見えるようにするため）。1以上にクランプして使う
	int markerWaveCount = 3;

	// 経路の線（プレイヤー位置→地点1→地点2…）の色。Inspectorのコンボで選択できる（既定は黄色、
	// 従来のkWaypointLineColorと同じ値）
	Vector4 lineColor = { 1.0f, 0.9f, 0.2f, 1.0f };

	// 経路の線を破線にする際の、実線部分・空白部分の長さ（ワールド単位）。
	// DrawPlanningVisualizationが線分をこの長さで区切り、実線部分だけ描く。
	// どちらも0以下だと0除算・無限ループになるため、実際に使う際は下限でクランプする
	float lineDashLength = 0.3f;
	float lineGapLength = 0.2f;

	// 経路線の太さ（ワールド単位、Y/Z方向の辺の長さ）。D3D12の標準ラスタライザはDrawLine
	// （LINELISTプリミティブ）に線幅指定を持たせられないため、各実線部分を進行方向に伸ばした
	// 立方体（DrawCube）として描画することで太さを表現する
	float lineThickness = 0.1f;

	// 破線パターンが進行方向（プレイヤー位置→地点方向）へ流れる速さ（ワールド単位/秒）。
	// ベルトコンベアのように、ダッシュ・ギャップの区切り位置を時間経過で進める。0で流れを止める
	float lineScrollSpeed = 1.0f;

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

	// 計画フェーズで現在までに予約された経路地点の数。TutorialScene側が「プレイヤーが
	// 一度でもクリックしたか」（1個以上予約済みか）を判定し、操作説明テキストを消すために使う
	size_t GetWaypointCount() const { return waypoints_.size(); }

	// index番目に予約された経路地点のワールド座標を返す。indexが範囲外の場合は{0,0,0}を返す。
	// TutorialScene側が「特定の座標がクリックされたか」を判定するために、直近で追加された
	// waypointの座標を調べる用途で使う
	Vector3 GetWaypoint(size_t index) const {
		if (index >= waypoints_.size()) return { 0.0f, 0.0f, 0.0f };
		return waypoints_[index];
	}

	// 準備フェーズ（kPreparing）から計画フェーズ（kPlanning）へ戻す。PlayScene側が
	// 「直前の実行フェーズで倒した敵の補充スポーンを全部出し終えた」タイミングで呼ぶ。
	// kPreparing以外の状態で呼ばれても何もしない（呼び出し側の判定ミスを黙って無視する）
	void FinishPreparing() {
		if (phase_ == Phase::kPreparing) phase_ = Phase::kPlanning;
	}

	// 計画フェーズ（kPlanning）から準備フェーズ（kPreparing）へ強制的に入れる。PlayScene側が
	// 「Playシーンを開いた直後、敵をランダム間隔で時間差スポーンさせている間はプレイヤーに
	// 操作させたくない」ために呼ぶ（実行フェーズ完了時の自動遷移とは別の、明示的な開始トリガー）。
	// kPlanning以外の状態（既に計画外・実行中等）で呼ばれても何もしない
	void BeginPreparing() {
		if (phase_ == Phase::kPlanning) phase_ = Phase::kPreparing;
	}

	// moveSpeed/easingType/obstacleMarginのみ保存対象（設定値）。waypoints_/phase_等は
	// 実行時の一時状態のため保存しない（GravityComponent::gravity等、他コンポーネントと同じ
	// 「設定値だけ保存する」方針を踏襲）
	void ToJson(nlohmann::json& out) const override {
		out["maxWaypoints"] = maxWaypoints;
		out["moveSpeed"] = moveSpeed;
		out["easingType"] = static_cast<int>(easingType);
		out["obstacleMargin"] = obstacleMargin;
		out["markerColor"] = Vector4ToJson(markerColor);
		out["markerPulseMinScale"] = markerPulseMinScale;
		out["markerPulseMaxScale"] = markerPulseMaxScale;
		out["markerPulseDuration"] = markerPulseDuration;
		out["markerWaveCount"] = markerWaveCount;
		out["lineDashLength"] = lineDashLength;
		out["lineGapLength"] = lineGapLength;
		out["lineThickness"] = lineThickness;
		out["lineScrollSpeed"] = lineScrollSpeed;
		out["lineColor"] = Vector4ToJson(lineColor);
		out["readyToExecuteDelay"] = readyToExecuteDelay;
		out["fieldRangeMinX"] = fieldRangeMinX;
		out["fieldRangeMaxX"] = fieldRangeMaxX;
		out["fieldRangeMinY"] = fieldRangeMinY;
		out["fieldRangeMaxY"] = fieldRangeMaxY;
	}
	void FromJson(const nlohmann::json& in) override {
		maxWaypoints = in.value("maxWaypoints", maxWaypoints);
		moveSpeed = in.value("moveSpeed", moveSpeed);
		easingType = static_cast<Easing::Type>(in.value("easingType", static_cast<int>(easingType)));
		obstacleMargin = in.value("obstacleMargin", obstacleMargin);
		if (in.contains("markerColor")) markerColor = Vector4FromJson(in["markerColor"]);
		markerPulseMinScale = in.value("markerPulseMinScale", markerPulseMinScale);
		markerPulseMaxScale = in.value("markerPulseMaxScale", markerPulseMaxScale);
		markerPulseDuration = in.value("markerPulseDuration", markerPulseDuration);
		markerWaveCount = in.value("markerWaveCount", markerWaveCount);
		lineDashLength = in.value("lineDashLength", lineDashLength);
		lineGapLength = in.value("lineGapLength", lineGapLength);
		lineThickness = in.value("lineThickness", lineThickness);
		lineScrollSpeed = in.value("lineScrollSpeed", lineScrollSpeed);
		if (in.contains("lineColor")) lineColor = Vector4FromJson(in["lineColor"]);
		readyToExecuteDelay = in.value("readyToExecuteDelay", readyToExecuteDelay);
		fieldRangeMinX = in.value("fieldRangeMinX", fieldRangeMinX);
		fieldRangeMaxX = in.value("fieldRangeMaxX", fieldRangeMaxX);
		fieldRangeMinY = in.value("fieldRangeMinY", fieldRangeMinY);
		fieldRangeMaxY = in.value("fieldRangeMaxY", fieldRangeMaxY);
	}

private:
	bool prevMouseLeftPressed_ = false;

	// このコンポーネントが生成されてから最初のUpdate呼び出しかどうか。他シーン（例：Title画面の
	// PLAYボタン）を左クリックした際、そのクリックがまだ離されていない状態でシーン遷移すると、
	// 新規生成されたReflexPlayerComponentのprevMouseLeftPressed_は初期値falseのため
	// 「今クリックされた瞬間」と誤判定してしまう（ボタンを押した座標がそのまま経路予約クリックに
	// 化けるバグの原因）。初回フレームだけはマウス状態をprevMouseLeftPressed_へ同期するのみに
	// とどめ、clickedThisFrame判定自体を行わないことでこれを防ぐ
	bool isFirstUpdate_ = true;

	Phase phase_ = Phase::kPlanning;

	std::vector<Vector3> waypoints_;  // 計画フェーズで予約した経路（プレイヤー位置は含まない）
	size_t currentWaypointIndex_ = 0; // 実行フェーズで現在向かっている地点のインデックス

	// kReadyToExecute中の経過時間。readyToExecuteDelayに達したら実行フェーズへ移る
	float readyToExecuteElapsed_ = 0.0f;

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

	// 経路マーカー用モデル（Resources/Model/Circle.obj）の遅延ロード状態。コンストラクタは
	// Rendererを受け取れない（IComponentは引数無しで生成される）ため、DrawPlanningVisualization
	// の初回呼び出し時に1度だけLoadModelする。tryLoadCircleModel_はロードを試みたかどうか
	// （成功・失敗を問わず1回だけ試す。失敗した場合は毎フレーム再試行せず、以降は
	// 黄色い球体にフォールバックし続ける）
	mutable bool tryLoadCircleModel_ = false;
	mutable bool circleModelLoaded_ = false;
	mutable Renderer::ModelHandle circleModelHandle_ = 0;

	// マーカーの波紋アニメーションの基準経過時間（0~markerPulseDurationを繰り返す時計）。
	// 各波紋（markerWaveCount本）はこの時計からmarkerPulseDuration/markerWaveCountぶんずつ
	// 位相をずらして自分の進行度を計算する
	mutable float markerPulseElapsed_ = 0.0f;

	// 破線パターンが進行方向へ流れる演出の経過距離（ワールド単位）。時間経過でlineScrollSpeed分
	// ずつ増え、DrawDashedLineがダッシュ・ギャップの開始位置オフセットとして使う
	mutable float lineScrollElapsed_ = 0.0f;

	// currentWaypointIndex_が指す区間の開始・終了・所要時間をセットし、経過時間をリセットする
	void BeginSegment(const Vector3& from, const Vector3& to);

	// 左クリックした地点（プレイヤーと同じZ座標のX-Y平面上）を求める。
	// 成功したらtrueを返し、outPositionにワールド座標を書き込む
	bool TryPickPoint(const Transform& transform, const UpdateContext& ctx, Vector3& outPosition) const;

	// fromからtoまでの線分上に、CollisionLayer::kObstacleのColliderComponentBaseを持つ
	// GameObjectが存在するかどうか。存在すればtrue（＝その方向へは進めない）
	bool IsPathBlocked(const Vector3& from, const Vector3& to, const UpdateContext& ctx) const;

	// 経路の可視化：waypoints_[startIndex]以降の各地点にCircle.objマーカー（ロード失敗時は球体）、
	// 区間（プレイヤーの現在位置→waypoints_[startIndex]→…）に破線を描く。deltaTimeはマーカーの
	// パルスアニメーション・破線のスクロール演出用。計画フェーズ（startIndex=0、全区間対象）と
	// 実行フェーズ（startIndex=currentWaypointIndex_、通過済み区間を除いた残りだけ対象。
	// 始点は毎フレームのtransform.translationそのものなので、移動につれて自動的に区間が縮む）の
	// 両方から呼ばれる
	void DrawPlanningVisualization(const Transform& transform, const UpdateContext& ctx, float deltaTime, size_t startIndex) const;

	// fromからtoまでの線分を、lineDashLength（実線）・lineGapLength（空白）の繰り返しで
	// 区切りながら実線部分だけ描く（破線描画）。D3D12の標準ラスタライザはDrawLine
	// （LINELISTプリミティブ）に太さを持たせられないため、各実線部分をlineThickness幅の
	// 薄いCube（DrawCube）として描画することで太さを表現する。scrollOffsetは
	// 「線が進行方向へ流れる」演出のパターン開始位置オフセット（呼び出し元が全区間で共通の
	// 値を1度だけ計算して渡す。区間ごとに計算すると区間数ぶんスクロールが速くなってしまうため）
	void DrawDashedLine(Renderer* renderer, const Vector3& from, const Vector3& to, const Vector4& color, float scrollOffset) const;

	// 計画フェーズ中の可視化：ctx.sceneObjects内の全障害物（CollisionLayer::kObstacle）について、
	// IsPathBlockedが実際に判定に使っているマージン込みの形状（実サイズ+obstacleMargin）を
	// ワイヤーフレームで描く。クリック前に「ここまでは安全、ここからは弾かれる」領域を一目で分かるようにする
	void DrawObstacleMarginVisualization(const UpdateContext& ctx) const;

	// 計画フェーズ中の可視化：fieldRangeMin/Max（TryPickPointが経路予約を許可する範囲）を、
	// Z方向に薄い直方体のワイヤーフレームで描く。障害物マージンと同じくコライダー風の見た目で、
	// この外側をクリックしても経路予約できないことを一目で分かるようにする
	void DrawFieldRangeVisualization(const Transform& transform, const UpdateContext& ctx) const;
};
