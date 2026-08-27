#include "ReflexPlayerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../GameObject.h"
#include "../../Systems/ScreenRay.h"
#include "ColliderComponentBase.h"
#include "SphereColliderComponent.h"
#include "OBBColliderComponent.h"
#include "../../../../Math/Collision.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/EasingPreview.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/Pipeline/BlendMode.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr Vector4 kWaypointMarkerColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄色い球（Circle.objロード失敗時のフォールバック）
	constexpr float   kWaypointMarkerRadius = 0.3f;
	constexpr Vector4 kObstacleMarginColor  = { 1.0f, 0.3f, 0.2f, 1.0f }; // 赤系：これより内側はクリックできない
	constexpr Vector4 kFieldRangeColor      = { 0.2f, 0.6f, 1.0f, 1.0f }; // 水色：この内側だけクリックで経路予約できる
	constexpr int      kWireCircleSegments  = 16;

	// Circle.objマーカーのモデルパス（パルスの範囲・周期はInspectorで調整可能なmarkerPulseMinScale/
	// markerPulseMaxScale/markerPulseDurationメンバを使う）
	constexpr const char* kMarkerModelDirectory = "Resources/Model";
	constexpr const char* kMarkerModelFilename  = "Circle.obj";

	// 中心center、半径radiusの円を1枚、指定した2軸(axis0, axis1。0=x,1=y,2=z)平面上に
	// 線分で近似して描画する（SphereColliderComponent::DrawWireCircleと同じロジック）
	void DrawWireCircle(Renderer* renderer, const Vector3& center, float radius, int axis0, int axis1,
		const Vector4& color, const Matrix4x4& view, const Matrix4x4& proj) {
		float coords[3] = { center.x, center.y, center.z };
		auto pointAt = [&](float angle) {
			float c[3] = { coords[0], coords[1], coords[2] };
			c[axis0] += cosf(angle) * radius;
			c[axis1] += sinf(angle) * radius;
			return Vector3{ c[0], c[1], c[2] };
			};
		for (int i = 0; i < kWireCircleSegments; i++) {
			float a0 = (float)i / kWireCircleSegments * 2.0f * 3.14159265f;
			float a1 = (float)(i + 1) / kWireCircleSegments * 2.0f * 3.14159265f;
			renderer->DrawLine(pointAt(a0), pointAt(a1), color, view, proj);
		}
	}

	// マージン込みのOBB（obb.Sizeは既にマージン加算済みの想定）の12本の辺を描画する
	// （OBBColliderComponent::DrawWireframeと同じロジック）
	void DrawWireOBB(Renderer* renderer, const Collision::OBB& obb,
		const Vector4& color, const Matrix4x4& view, const Matrix4x4& proj) {
		auto toWorld = [&](float sx, float sy, float sz) {
			Vector3 local = { obb.Size.x * sx, obb.Size.y * sy, obb.Size.z * sz };
			Vector3 world = obb.center
				+ Vector3{ obb.Orientation[0].x * local.x, obb.Orientation[0].y * local.x, obb.Orientation[0].z * local.x }
				+ Vector3{ obb.Orientation[1].x * local.y, obb.Orientation[1].y * local.y, obb.Orientation[1].z * local.y }
				+ Vector3{ obb.Orientation[2].x * local.z, obb.Orientation[2].y * local.z, obb.Orientation[2].z * local.z };
			return world;
			};
		Vector3 p[8] = {
			toWorld(-1,-1,-1), toWorld(+1,-1,-1), toWorld(+1,+1,-1), toWorld(-1,+1,-1),
			toWorld(-1,-1,+1), toWorld(+1,-1,+1), toWorld(+1,+1,+1), toWorld(-1,+1,+1),
		};
		static constexpr int kEdges[12][2] = {
			{0,1}, {1,2}, {2,3}, {3,0},
			{4,5}, {5,6}, {6,7}, {7,4},
			{0,4}, {1,5}, {2,6}, {3,7},
		};
		for (auto& e : kEdges) renderer->DrawLine(p[e[0]], p[e[1]], color, view, proj);
	}
}

bool ReflexPlayerComponent::TryPickPoint(const Transform& transform, const UpdateContext& ctx, Vector3& outPosition) const {
	if (!ctx.renderer) return false;
	if (ImGui::GetIO().WantCaptureMouse) return false; // ImGuiパネル上のクリックは無視

	Collision::Ray ray = ScreenRay::FromMouse(ctx.renderer, ctx.view, ctx.proj);

	// このゲームはX-Y平面上で進行するため、プレイヤーと同じZ座標のX-Y平面（法線Z+）を
	// クリック対象の面とみなす
	Collision::Plane fieldPlane{ { 0.0f, 0.0f, 1.0f }, transform.translation.z };
	float t;
	Vector3 hitPoint;
	if (!Collision::RayPlane(ray, fieldPlane, t, hitPoint)) return false;

	hitPoint.z = transform.translation.z; // Z座標は常に固定のまま保つ

	// フィールド範囲外のクリックは無効にする（壁の外側を直接クリックすると、経路上に障害物
	// （壁の線分）が無い限りそのまま外へ移動できてしまっていたため、障害物判定と同じ扱いで
	// ここで弾く）
	if (hitPoint.x < fieldRangeMinX || hitPoint.x > fieldRangeMaxX ||
		hitPoint.y < fieldRangeMinY || hitPoint.y > fieldRangeMaxY) {
		return false;
	}

	outPosition = hitPoint;
	return true;
}

bool ReflexPlayerComponent::IsPathBlocked(const Vector3& from, const Vector3& to, const UpdateContext& ctx) const {
	if (!ctx.sceneObjects) return false;

	Collision::Segment segment;
	segment.origin = from;
	segment.diff = to - from;

	for (GameObject* obj : *ctx.sceneObjects) {
		if (!obj) continue;
		auto* collider = obj->GetComponent<ColliderComponentBase>();
		if (!collider) continue;
		if (collider->layer != CollisionLayer::kObstacle) continue;

		Transform ownerTransform = obj->GetWorldTransform();
		if (auto* sphereCollider = obj->GetComponent<SphereColliderComponent>()) {
			// 実際の障害物半径にobstacleMarginを加えた「少し広い球」で判定し、
			// クリック地点が障害物ぎりぎりになってColliderSystemの押し戻しと競合するのを防ぐ
			Collision::Sphere sphere = sphereCollider->GetWorldSphere(ownerTransform);
			sphere.radius += obstacleMargin;
			if (Collision::SegmentSphere(segment, sphere)) return true;
		} else if (auto* obbCollider = obj->GetComponent<OBBColliderComponent>()) {
			// 同様にOBBのSizeへobstacleMarginを加えて一回り大きい直方体として判定する
			Collision::OBB obb = obbCollider->GetWorldOBB(ownerTransform);
			obb.Size = obb.Size + Vector3{ obstacleMargin, obstacleMargin, obstacleMargin };
			if (Collision::OBBSegment(obb, segment)) return true;
		}
	}
	return false;
}

void ReflexPlayerComponent::DrawObstacleMarginVisualization(const UpdateContext& ctx) const {
	if (!ctx.renderer || !ctx.sceneObjects) return;

	for (GameObject* obj : *ctx.sceneObjects) {
		if (!obj) continue;
		auto* collider = obj->GetComponent<ColliderComponentBase>();
		if (!collider) continue;
		if (collider->layer != CollisionLayer::kObstacle) continue;

		Transform ownerTransform = obj->GetWorldTransform();
		if (auto* sphereCollider = obj->GetComponent<SphereColliderComponent>()) {
			Collision::Sphere sphere = sphereCollider->GetWorldSphere(ownerTransform);
			sphere.radius += obstacleMargin;
			DrawWireCircle(ctx.renderer, sphere.center, sphere.radius, 0, 1, kObstacleMarginColor, ctx.view, ctx.proj);
			DrawWireCircle(ctx.renderer, sphere.center, sphere.radius, 1, 2, kObstacleMarginColor, ctx.view, ctx.proj);
			DrawWireCircle(ctx.renderer, sphere.center, sphere.radius, 0, 2, kObstacleMarginColor, ctx.view, ctx.proj);
		} else if (auto* obbCollider = obj->GetComponent<OBBColliderComponent>()) {
			Collision::OBB obb = obbCollider->GetWorldOBB(ownerTransform);
			obb.Size = obb.Size + Vector3{ obstacleMargin, obstacleMargin, obstacleMargin };
			DrawWireOBB(ctx.renderer, obb, kObstacleMarginColor, ctx.view, ctx.proj);
		}
	}
}

void ReflexPlayerComponent::DrawFieldRangeVisualization(const Transform& transform, const UpdateContext& ctx) const {
	if (!ctx.renderer) return;

	// fieldRangeMin/Max（TryPickPointが経路予約を許可する範囲）を、Z方向に薄い直方体の
	// ワイヤーフレームとしてコライダーの可視化と同じ見た目で表示する。回転なし・軸並行の
	// 矩形なのでOrientationは単位行列のまま使う
	Collision::OBB obb;
	obb.center = {
		(fieldRangeMinX + fieldRangeMaxX) * 0.5f,
		(fieldRangeMinY + fieldRangeMaxY) * 0.5f,
		transform.translation.z };
	obb.Orientation[0] = { 1.0f, 0.0f, 0.0f };
	obb.Orientation[1] = { 0.0f, 1.0f, 0.0f };
	obb.Orientation[2] = { 0.0f, 0.0f, 1.0f };
	obb.Size = {
		(fieldRangeMaxX - fieldRangeMinX) * 0.5f,
		(fieldRangeMaxY - fieldRangeMinY) * 0.5f,
		0.05f };
	DrawWireOBB(ctx.renderer, obb, kFieldRangeColor, ctx.view, ctx.proj);
}

void ReflexPlayerComponent::DrawPlanningVisualization(const Transform& transform, const UpdateContext& ctx, float deltaTime, size_t startIndex) const {
	if (!ctx.renderer) return;
	if (startIndex >= waypoints_.size()) return; // 実行フェーズで全区間を通過済みの場合等

	// Circle.objの読み込みは初回呼び出し時に1度だけ試みる（成功・失敗を問わず以降は再試行しない。
	// LoadModelはGPUリソースを新規確保するため毎フレーム呼ぶわけにはいかない）
	// 注意：Renderer::LoadModelはファイルが見つからない等の失敗時にassert(false)で
	// 落ちる実装（Model_AssimpLoader.cpp）のため、Circle.objが存在する限りここで例外的に
	// フォールバックへ分岐することはない。circleModelLoaded_は将来LoadModelが失敗時に
	// 安全に倒れるよう改修された場合に備えて残す
	if (!tryLoadCircleModel_) {
		tryLoadCircleModel_ = true;
		circleModelHandle_ = ctx.renderer->LoadModel(kMarkerModelDirectory, kMarkerModelFilename);
		circleModelLoaded_ = true;
	}

	// 波紋アニメーション：基準時計markerPulseElapsed_を進める。duration<=0（Inspectorでの
	// 入力ミス等）は0除算になるため、その場合は波紋を1本・最大スケール固定で表示する
	float duration = (std::max)(markerPulseDuration, 0.0f);
	int waveCount = (std::max)(markerWaveCount, 1);
	if (duration > 0.0f) {
		markerPulseElapsed_ = std::fmod(markerPulseElapsed_ + deltaTime, duration);
	}

	// 破線が進行方向へ流れる演出のオフセットは、全waypoint区間で共通の値を1度だけ計算する
	// （区間ごとにDrawDashedLine内でlineScrollElapsed_を加算すると、区間数が多い＝経路が長い
	// ほど1フレームあたりの加算回数が増えてしまい、見かけのスクロール速度が距離に依存してしまう）
	float dashPeriod = (std::max)(lineDashLength, 0.01f) + (std::max)(lineGapLength, 0.01f);
	lineScrollElapsed_ += lineScrollSpeed * deltaTime;
	float scrollOffset = std::fmod(lineScrollElapsed_, dashPeriod);
	if (scrollOffset < 0.0f) scrollOffset += dashPeriod; // 負の速度指定でも安全なようにフルクランプ

	Vector3 previous = transform.translation;
	for (size_t i = startIndex; i < waypoints_.size(); i++) {
		const Vector3& point = waypoints_[i];
		DrawDashedLine(ctx.renderer, previous, point, lineColor, scrollOffset);

		// waveCount本の波紋を、それぞれduration/waveCountぶん位相をずらして同じ地点に重ねて描く。
		// 各波紋は片道（0→1）：小さい状態から広がりながら不透明度が下がって消え、次の周期でまた
		// 最小サイズから再発生する（水面の波紋のように複数が同時進行して見える）
		for (int wave = 0; wave < waveCount; wave++) {
			float t = 0.0f; // 0=発生直後（最小・不透明）、1=消える直前（最大・透明）
			if (duration > 0.0f) {
				float phaseOffset = duration * (static_cast<float>(wave) / static_cast<float>(waveCount));
				float waveElapsed = std::fmod(markerPulseElapsed_ + phaseOffset, duration);
				t = waveElapsed / duration;
			}
			float eased = Easing::Apply(Easing::Type::kInOutSine, t);
			float scale = markerPulseMinScale + (markerPulseMaxScale - markerPulseMinScale) * eased;
			float alpha = markerColor.w * (1.0f - eased);

			Transform markerTransform;
			markerTransform.translation = point;
			markerTransform.scale = { scale, scale, scale };

			if (circleModelLoaded_) {
				Vector4 fadedColor = { markerColor.x, markerColor.y, markerColor.z, alpha };
				ctx.renderer->DrawModel(circleModelHandle_, markerTransform, fadedColor,
					{}, true, BlendMode::kNormal);
			} else {
				Vector4 fadedColor = { kWaypointMarkerColor.x, kWaypointMarkerColor.y, kWaypointMarkerColor.z, alpha };
				ctx.renderer->DrawSphere(markerTransform, fadedColor, kTextureNone, true, BlendMode::kNormal);
			}
		}

		previous = point;
	}
}

void ReflexPlayerComponent::DrawDashedLine(Renderer* renderer, const Vector3& from, const Vector3& to, const Vector4& color, float scrollOffset) const {
	float totalLength = VectorMath::Length(to - from);
	if (totalLength <= 0.0f) return;

	// dash/gapが0以下（Inspectorでの入力ミス等）だと無限ループになるため下限でクランプする
	float dash = (std::max)(lineDashLength, 0.01f);
	float gap = (std::max)(lineGapLength, 0.01f);
	float thickness = (std::max)(lineThickness, 0.01f);
	float period = dash + gap;
	Vector3 direction = (to - from) * (1.0f / totalLength); // VectorMath::Normalizeと等価だが長さを再計算せず済む

	// このゲームはX-Y平面上（Z座標固定）で進行するため、線分の向きはZ軸周りの回転角だけで表現できる。
	// DrawCubeの既定形状はローカルX軸方向に伸びる立方体を想定しているため、Cubeのscale.xを
	// セグメント長さ、scale.y/zをthicknessにして、Z軸回転で線分の向きへ合わせる
	float rotationZ = std::atan2f(direction.y, direction.x);

	// パターンの基準点はto（waypoint、実行フェーズ中も動かない不変の点）側に置き、そこから
	// from方向へ向かって並べる。実行フェーズ中はfromがtoへ近づくにつれtotalLengthが縮むが、
	// ダッシュ・ギャップの絶対的な間隔（dash/gap）はワールド座標上で固定されたまま、to側から
	// 見た並びを保ったままfrom側の端が自然に短く切り詰められるだけになる（fromを基準にすると
	// 毎フレームtotalLengthが変わるたびにパターンの位相がfromへ貼り付き直し、区間が縮むほど
	// ダッシュの間隔が詰まって見えてしまう問題があった）
	float cursor = scrollOffset - period;
	while (cursor < totalLength) {
		float dashStartFromTo = (std::max)(cursor, 0.0f);
		float dashEndFromTo = (std::min)(cursor + dash, totalLength);
		if (dashEndFromTo > dashStartFromTo) {
			// dashStartFromTo/dashEndFromTo は「toからfrom方向への距離」。実際の位置はtoから
			// -direction（=to→from方向）へ進めて求める
			Vector3 segmentStart = to - direction * dashStartFromTo;
			Vector3 segmentEnd = to - direction * dashEndFromTo;
			float segmentLength = dashEndFromTo - dashStartFromTo;

			Transform segmentTransform;
			segmentTransform.translation = (segmentStart + segmentEnd) * 0.5f;
			segmentTransform.rotation = { 0.0f, 0.0f, rotationZ };
			segmentTransform.scale = { segmentLength, thickness, thickness };
			renderer->DrawCube(segmentTransform, color, kTextureNone, false);
		}

		cursor += period;
	}
}

void ReflexPlayerComponent::BeginSegment(const Vector3& from, const Vector3& to) {
	// distanceは代入前に計算する：呼び出し元がBeginSegment(segmentEnd_, ...)のように
	// メンバ変数への参照をfromに渡すことがあるため、先にsegmentEnd_=toを代入してしまうと
	// from（=segmentEnd_の別名）が書き換わり、to-fromが常に0になってしまう
	float distance = VectorMath::Length(to - from);

	segmentStart_ = from;
	segmentEnd_ = to;
	segmentElapsed_ = 0.0f;
	segmentStarted_ = true;

	// moveSpeedは「1秒あたりの移動距離」の意味を保つため、所要時間=距離/速度とする。
	// 距離0（同じ地点を続けてクリックした場合等）やmoveSpeed<=0での0除算を避け、即座に完了させる
	segmentDuration_ = (moveSpeed > 0.0f) ? (distance / moveSpeed) : 0.0f;
}

void ReflexPlayerComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	// Sceneビュー表示中（ctx.isGameView==false）はGizmoController::UpdatePicking/UpdatePicking2Dが
	// 同じ左クリックでオブジェクト選択・矩形選択を行っているため、Gameビュー中のみクリックを受け付ける
	bool leftPressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	// 初回フレームは他シーンから引き継いだ「押されっぱなし」のマウス状態を誤ってクリックとして
	// 拾わないよう、prevMouseLeftPressed_への同期だけ行いclickedThisFrameは常にfalseにする
	bool clickedThisFrame = !isFirstUpdate_ && ctx.isGameView && leftPressed && !prevMouseLeftPressed_;
	prevMouseLeftPressed_ = leftPressed;
	isFirstUpdate_ = false;

	if (phase_ == Phase::kPlanning) {
		// 計画フェーズ：クリックするたびに地点を追加する（最大maxWaypoints個）。プレイヤーはまだ動かない
		size_t maxWaypointsClamped = static_cast<size_t>((std::max)(maxWaypoints, 0));
		if (clickedThisFrame && waypoints_.size() < maxWaypointsClamped) {
			Vector3 point;
			if (TryPickPoint(transform, ctx, point)) {
				// 直前の地点（無ければ現在のプレイヤー位置）からクリック地点までの経路上に
				// 障害物（CollisionLayer::kObstacle）があれば、その方向には進めないためクリックを無効化する
				Vector3 previous = waypoints_.empty() ? transform.translation : waypoints_.back();
				if (!IsPathBlocked(previous, point, ctx)) {
					waypoints_.push_back(point);
					if (waypoints_.size() >= maxWaypointsClamped) {
						// 上限個数まで予約し終えたら、即座に実行フェーズへ移らずreadyToExecuteDelay秒
						// 待つ短い遷移フェーズへ入る（地点が確定したことを視覚的に分かりやすくする）
						phase_ = Phase::kReadyToExecute;
						readyToExecuteElapsed_ = 0.0f;
						return;
					}
				}
			}
		}
		// 障害物マージン・移動可能範囲の可視化（ワイヤーフレーム）はデバッグ用の補助線であり、
		// 実際のプレイ画面（Gameビュー、Gizmoなし）に映り込ませたくない。Sceneビュー
		// （エディタ自由カメラ）でだけ表示する
		if (!ctx.isGameView) {
			DrawObstacleMarginVisualization(ctx);
			DrawFieldRangeVisualization(transform, ctx);
		}
		DrawPlanningVisualization(transform, ctx, deltaTime, 0);
		return;
	}

	// 実行準備フェーズ：maxWaypoints個目を予約した直後の短い待機。クリックは受け付けない
	// （waypoints_は既に上限のためTryPickPointを呼んでも計画フェーズと同じく地点は増えないが、
	// 障害物マージンの可視化等は行わず経路の見た目だけ表示を継続する）。readyToExecuteDelayが
	// 0以下の場合はdeltaTime分の加算だけで即座に条件を満たし、実質的に従来通り即時遷移する
	if (phase_ == Phase::kReadyToExecute) {
		DrawPlanningVisualization(transform, ctx, deltaTime, 0);
		readyToExecuteElapsed_ += deltaTime;
		if (readyToExecuteElapsed_ >= readyToExecuteDelay) {
			phase_ = Phase::kExecuting;
			currentWaypointIndex_ = 0;
			segmentStarted_ = false; // 次のUpdateでBeginSegmentする
		}
		return;
	}

	// 準備フェーズ：PlayScene側が敵の補充スポーンを終えてFinishPreparing()を呼ぶまで、
	// プレイヤーはここで待機するだけ（クリックも移動も受け付けない）
	if (phase_ == Phase::kPreparing) return;

	// 実行フェーズ：waypoints_を先頭から順に、区間ごとにイージング補間しながら直進する
	if (currentWaypointIndex_ >= waypoints_.size()) return;

	if (!segmentStarted_) {
		// DrawImGuiの「実行フェーズへ」ボタン経由でkExecutingに入った場合、ボタン側は
		// transformを持たずBeginSegmentを呼べないため、ここで初回フレームに呼ぶ
		BeginSegment(transform.translation, waypoints_[currentWaypointIndex_]);
	}

	segmentElapsed_ += deltaTime;
	float t = (segmentDuration_ > 0.0f) ? (segmentElapsed_ / segmentDuration_) : 1.0f;

	if (t >= 1.0f) {
		// 区間の終点に到達：クランプせずぴったり終点へスナップしてから次へ進む
		transform.translation.x = segmentEnd_.x;
		transform.translation.y = segmentEnd_.y;
		++currentWaypointIndex_;
		if (currentWaypointIndex_ >= waypoints_.size()) {
			// 最後の地点に到達した：自動的に準備フェーズへ移り、PlayScene側の敵補充スポーンを待つ。
			// 計画フェーズへ戻すのはFinishPreparing()（PlayScene側が補充完了後に呼ぶ）の役目
			phase_ = Phase::kPreparing;
			waypoints_.clear();
			currentWaypointIndex_ = 0;
			segmentStarted_ = false;
			executionFinished_ = true; // 外部（PlayScene等）が実行フェーズ完了の瞬間を検知できるようにする
		} else {
			BeginSegment(segmentEnd_, waypoints_[currentWaypointIndex_]);
		}
	} else {
		float easedT = Easing::Apply(easingType, t);
		Vector3 lerped = segmentStart_ + (segmentEnd_ - segmentStart_) * easedT;
		transform.translation.x = lerped.x;
		transform.translation.y = lerped.y;
	}

	// 実行フェーズ中も計画フェーズと同じ見た目のマーカー・破線を表示し続ける。ただし
	// currentWaypointIndex_より前（プレイヤーが既に通過した区間）は描かず、現在の区間は
	// 始点をtransform.translation（今フレームの実際の位置）にすることで、移動につれて
	// リアルタイムに短くなっていくように見せる
	DrawPlanningVisualization(transform, ctx, deltaTime, currentWaypointIndex_);
}

void ReflexPlayerComponent::DrawImGui(const char* namePrefix) {
	std::string maxWaypointsLabel = std::string(namePrefix) + "最大経路予約数";
	ImGui::DragInt(maxWaypointsLabel.c_str(), &maxWaypoints, 1, 1, 20);

	std::string speedLabel = std::string(namePrefix) + "移動速度";
	ImGui::DragFloat(speedLabel.c_str(), &moveSpeed, 0.1f, 0.0f, 20.0f);

	std::string marginLabel = std::string(namePrefix) + "障害物マージン";
	ImGui::DragFloat(marginLabel.c_str(), &obstacleMargin, 0.01f, 0.0f, 5.0f);

	std::string fieldRangeMinXLabel = std::string(namePrefix) + "移動可能範囲 X最小";
	std::string fieldRangeMaxXLabel = std::string(namePrefix) + "移動可能範囲 X最大";
	std::string fieldRangeMinYLabel = std::string(namePrefix) + "移動可能範囲 Y最小";
	std::string fieldRangeMaxYLabel = std::string(namePrefix) + "移動可能範囲 Y最大";
	ImGui::DragFloat(fieldRangeMinXLabel.c_str(), &fieldRangeMinX, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat(fieldRangeMaxXLabel.c_str(), &fieldRangeMaxX, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat(fieldRangeMinYLabel.c_str(), &fieldRangeMinY, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat(fieldRangeMaxYLabel.c_str(), &fieldRangeMaxY, 0.1f, -100.0f, 100.0f);

	std::string readyDelayLabel = std::string(namePrefix) + "実行フェーズへの遷移待機時間(秒)";
	ImGui::DragFloat(readyDelayLabel.c_str(), &readyToExecuteDelay, 0.01f, 0.0f, 10.0f);

	std::string markerColorLabel = std::string(namePrefix) + "経路マーカーの色";
	ImGui::ColorEdit4(markerColorLabel.c_str(), &markerColor.x);

	std::string lineColorLabel = std::string(namePrefix) + "経路線の色";
	ImGui::ColorEdit4(lineColorLabel.c_str(), &lineColor.x);

	std::string markerMinScaleLabel = std::string(namePrefix) + "マーカーの最小スケール";
	std::string markerMaxScaleLabel = std::string(namePrefix) + "マーカーの最大スケール";
	std::string markerDurationLabel = std::string(namePrefix) + "マーカーのパルス周期(秒)";
	ImGui::DragFloat(markerMinScaleLabel.c_str(), &markerPulseMinScale, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(markerMaxScaleLabel.c_str(), &markerPulseMaxScale, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat(markerDurationLabel.c_str(), &markerPulseDuration, 0.01f, 0.0f, 10.0f);

	std::string markerWaveCountLabel = std::string(namePrefix) + "波紋の本数";
	ImGui::DragInt(markerWaveCountLabel.c_str(), &markerWaveCount, 1, 1, 10);

	std::string dashLengthLabel = std::string(namePrefix) + "経路線の実線長さ";
	std::string gapLengthLabel = std::string(namePrefix) + "経路線の間隔長さ";
	ImGui::DragFloat(dashLengthLabel.c_str(), &lineDashLength, 0.01f, 0.01f, 10.0f);
	ImGui::DragFloat(gapLengthLabel.c_str(), &lineGapLength, 0.01f, 0.01f, 10.0f);

	std::string thicknessLabel = std::string(namePrefix) + "経路線の太さ";
	ImGui::DragFloat(thicknessLabel.c_str(), &lineThickness, 0.01f, 0.01f, 5.0f);

	std::string scrollSpeedLabel = std::string(namePrefix) + "経路線の流れる速さ";
	ImGui::DragFloat(scrollSpeedLabel.c_str(), &lineScrollSpeed, 0.01f, -10.0f, 10.0f);

	// 実行フェーズの直進に適用するイージング（https://easings.net/ja 準拠）をコンボで選択する
	std::string easingLabel = std::string(namePrefix) + "イージング";
	int easingIndex = static_cast<int>(easingType);
	const char* const* easingNames = Easing::GetTypeNames();
	if (ImGui::BeginCombo(easingLabel.c_str(), easingNames[easingIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == easingIndex);
			if (ImGui::Selectable(easingNames[i], selected)) easingType = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
			// カーソルを合わせている項目のイージング動作を、球体が左から右へ動くアニメーションで
			// プレビューする（ツールチップ内に常時再生し続ける小さなトラックを描く）
			EasingPreview::ShowOnHover(static_cast<Easing::Type>(i));
		}
		ImGui::EndCombo();
	}

	const char* phaseLabel = (phase_ == Phase::kPlanning) ? "計画フェーズ"
		: (phase_ == Phase::kReadyToExecute) ? "実行準備フェーズ"
		: (phase_ == Phase::kExecuting) ? "実行フェーズ" : "準備フェーズ";
	ImGui::Text("%s", (std::string(namePrefix) + "フェーズ: " + phaseLabel).c_str());
	ImGui::Text("%s", (std::string(namePrefix) + "経路の地点数: "
		+ std::to_string(waypoints_.size()) + " / " + std::to_string(maxWaypoints)).c_str());
	if (phase_ == Phase::kExecuting) {
		ImGui::Text("%s", (std::string(namePrefix) + "進行状況: "
			+ std::to_string((std::min)(currentWaypointIndex_, waypoints_.size())) + " / " + std::to_string(waypoints_.size())).c_str());
	}
	if (phase_ == Phase::kPlanning) {
		std::string clearLabel = std::string(namePrefix) + "経路をクリア";
		if (ImGui::Button(clearLabel.c_str())) {
			waypoints_.clear();
		}
		ImGui::SameLine();
		std::string toExecuteLabel = std::string(namePrefix) + "実行フェーズへ";
		if (ImGui::Button(toExecuteLabel.c_str())) {
			phase_ = Phase::kExecuting;
			currentWaypointIndex_ = 0;
		}
	} else {
		std::string toPlanningLabel = std::string(namePrefix) + "計画フェーズへ";
		if (ImGui::Button(toPlanningLabel.c_str())) {
			phase_ = Phase::kPlanning;
			waypoints_.clear();
			currentWaypointIndex_ = 0;
			segmentStarted_ = false;
		}
	}
}

REGISTER_SIMPLE_COMPONENT(ReflexPlayerComponent, "ReflexPlayer", "REFLEXプレイヤー操作", "物理");
