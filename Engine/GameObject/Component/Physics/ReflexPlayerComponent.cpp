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
#include <algorithm>
#include <cmath>

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
		if (!ctx.isGameView && ctx.sceneObjects) {
			visualizer_.DrawObstacleMargin(ctx.renderer, ctx.view, ctx.proj, *ctx.sceneObjects, obstacleMargin);
			visualizer_.DrawFieldRange(ctx.renderer, ctx.view, ctx.proj, transform.translation.z,
				fieldRangeMinX, fieldRangeMaxX, fieldRangeMinY, fieldRangeMaxY);
		}
		visualizer_.DrawPlanning(ctx.renderer, ctx.view, ctx.proj, transform.translation, waypoints_, 0, deltaTime);
		return;
	}

	// 実行準備フェーズ：maxWaypoints個目を予約した直後の短い待機。クリックは受け付けない
	// （waypoints_は既に上限のためTryPickPointを呼んでも計画フェーズと同じく地点は増えないが、
	// 障害物マージンの可視化等は行わず経路の見た目だけ表示を継続する）。readyToExecuteDelayが
	// 0以下の場合はdeltaTime分の加算だけで即座に条件を満たし、実質的に従来通り即時遷移する
	if (phase_ == Phase::kReadyToExecute) {
		visualizer_.DrawPlanning(ctx.renderer, ctx.view, ctx.proj, transform.translation, waypoints_, 0, deltaTime);
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
	visualizer_.DrawPlanning(ctx.renderer, ctx.view, ctx.proj, transform.translation, waypoints_, currentWaypointIndex_, deltaTime);
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

	// マーカー・経路線の見た目パラメータはReflexPathVisualizer側のUIに委譲する
	visualizer_.DrawImGui(namePrefix);

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
