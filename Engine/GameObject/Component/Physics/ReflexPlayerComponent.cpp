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
#include <cmath>

namespace {
	constexpr Vector4 kWaypointMarkerColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄色い球
	constexpr Vector4 kWaypointLineColor   = { 1.0f, 0.9f, 0.2f, 1.0f };
	constexpr float   kWaypointMarkerRadius = 0.3f;
	constexpr Vector4 kObstacleMarginColor  = { 1.0f, 0.3f, 0.2f, 1.0f }; // 赤系：これより内側はクリックできない
	constexpr int      kWireCircleSegments  = 16;

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

void ReflexPlayerComponent::DrawPlanningVisualization(const Transform& transform, const UpdateContext& ctx) const {
	if (!ctx.renderer) return;

	Vector3 previous = transform.translation;
	for (const Vector3& point : waypoints_) {
		ctx.renderer->DrawLine(previous, point, kWaypointLineColor, ctx.view, ctx.proj);

		Transform markerTransform;
		markerTransform.translation = point;
		markerTransform.scale = { kWaypointMarkerRadius, kWaypointMarkerRadius, kWaypointMarkerRadius };
		ctx.renderer->DrawSphere(markerTransform, kWaypointMarkerColor);

		previous = point;
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
	bool clickedThisFrame = ctx.isGameView && leftPressed && !prevMouseLeftPressed_;
	prevMouseLeftPressed_ = leftPressed;

	if (phase_ == Phase::kPlanning) {
		// 計画フェーズ：クリックするたびに地点を追加する（最大kMaxWaypoints個）。プレイヤーはまだ動かない
		if (clickedThisFrame && waypoints_.size() < kMaxWaypoints) {
			Vector3 point;
			if (TryPickPoint(transform, ctx, point)) {
				// 直前の地点（無ければ現在のプレイヤー位置）からクリック地点までの経路上に
				// 障害物（CollisionLayer::kObstacle）があれば、その方向には進めないためクリックを無効化する
				Vector3 previous = waypoints_.empty() ? transform.translation : waypoints_.back();
				if (!IsPathBlocked(previous, point, ctx)) {
					waypoints_.push_back(point);
					if (waypoints_.size() >= kMaxWaypoints) {
						// 上限個数まで予約し終えたら自動的に実行フェーズへ移行する
						phase_ = Phase::kExecuting;
						currentWaypointIndex_ = 0;
						segmentStarted_ = false; // 次のUpdateでBeginSegmentする
						return;
					}
				}
			}
		}
		// 障害物マージンの可視化（赤いワイヤーフレーム）はデバッグ用の補助線であり、実際の
		// プレイ画面（Gameビュー、Gizmoなし）に映り込ませたくない。Sceneビュー（エディタ自由カメラ）
		// でだけ表示する
		if (!ctx.isGameView) {
			DrawObstacleMarginVisualization(ctx);
		}
		DrawPlanningVisualization(transform, ctx);
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
}

void ReflexPlayerComponent::DrawImGui(const char* namePrefix) {
	std::string speedLabel = std::string(namePrefix) + "移動速度";
	ImGui::DragFloat(speedLabel.c_str(), &moveSpeed, 0.1f, 0.0f, 20.0f);

	std::string marginLabel = std::string(namePrefix) + "障害物マージン";
	ImGui::DragFloat(marginLabel.c_str(), &obstacleMargin, 0.01f, 0.0f, 5.0f);

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
		: (phase_ == Phase::kExecuting) ? "実行フェーズ" : "準備フェーズ";
	ImGui::Text("%s", (std::string(namePrefix) + "フェーズ: " + phaseLabel).c_str());
	ImGui::Text("%s", (std::string(namePrefix) + "経路の地点数: "
		+ std::to_string(waypoints_.size()) + " / " + std::to_string(kMaxWaypoints)).c_str());
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
