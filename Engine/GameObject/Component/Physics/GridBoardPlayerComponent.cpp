// 10DaysJam
#include "GridBoardPlayerComponent.h"
#include "GridBoardComponent.h"
#include "GridItemComponent.h"
#include "../../ComponentRegistry.h"
#include "../../GameObject.h"
#include "../../Systems/ScreenRay.h"
#include "../../../../Math/Collision.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include <algorithm>
#include <cmath>

namespace {
	// sceneObjectsからGridBoardComponentを持つ最初のGameObjectを探して返す（無ければnullptr）
	const GridBoardComponent* FindBoard(const std::vector<GameObject*>* sceneObjects) {
		if (!sceneObjects) return nullptr;
		for (GameObject* obj : *sceneObjects) {
			if (!obj) continue;
			if (auto* board = obj->GetComponent<GridBoardComponent>()) return board;
		}
		return nullptr;
	}
}

bool GridBoardPlayerComponent::TryPickCell(const Transform& transform, const UpdateContext& ctx, int& outCol, int& outRow) const {
	if (!ctx.renderer) return false;
	if (ImGui::GetIO().WantCaptureMouse) return false; // ImGuiパネル上のクリックは無視

	const GridBoardComponent* board = FindBoard(ctx.sceneObjects);
	if (!board) return false;

	Collision::Ray ray = ScreenRay::FromMouse(ctx.renderer, ctx.view, ctx.proj);

	// このゲームはX-Y平面上で進行するため、プレイヤーと同じZ座標のX-Y平面（法線Z+）を
	// クリック対象の面とみなす（ReflexPlayerComponent::TryPickPointと同じ考え方）
	Collision::Plane fieldPlane{ { 0.0f, 0.0f, 1.0f }, transform.translation.z };
	float t;
	Vector3 hitPoint;
	if (!Collision::RayPlane(ray, fieldPlane, t, hitPoint)) return false;

	int col, row;
	board->WorldToNearestGrid(hitPoint, col, row);
	if (col < 0 || col >= board->columns || row < 0 || row >= board->rows) return false;

	outCol = col;
	outRow = row;
	return true;
}

void GridBoardPlayerComponent::BeginSegment(const Vector3& from, const Vector3& to) {
	float distance = VectorMath::Length(to - from);

	segmentStart_ = from;
	segmentEnd_ = to;
	segmentElapsed_ = 0.0f;
	segmentStarted_ = true;
	segmentDuration_ = (moveSpeed > 0.0f) ? (distance / moveSpeed) : 0.0f;
}

std::vector<std::pair<int, int>> GridBoardPlayerComponent::GetValidTargets(const Transform& transform, const std::vector<GameObject*>* sceneObjects) const {
	std::vector<std::pair<int, int>> result;
	if (phase_ != Phase::kPlanning || currentCost_ <= 0) return result;

	const GridBoardComponent* board = FindBoard(sceneObjects);
	if (!board) return result;

	int originCol, originRow;
	if (waypoints_.empty()) {
		board->WorldToNearestGrid(transform.translation, originCol, originRow);
	} else {
		originCol = waypoints_.back().first;
		originRow = waypoints_.back().second;
	}

	// 横方向：originColを中心に左右へ、盤面端 or 残コストで届く範囲まで
	for (int col = originCol - currentCost_; col <= originCol + currentCost_; ++col) {
		if (col == originCol) continue;
		if (col < 0 || col >= board->columns) continue;
		result.push_back({ col, originRow });
	}
	// 縦方向：originRowを中心に上下へ、盤面端 or 残コストで届く範囲まで
	for (int row = originRow - currentCost_; row <= originRow + currentCost_; ++row) {
		if (row == originRow) continue;
		if (row < 0 || row >= board->rows) continue;
		result.push_back({ originCol, row });
	}

	return result;
}

void GridBoardPlayerComponent::ClearWaypoints() {
	// 予約時に即時消費した分をまとめて払い戻す（消費距離の合計 = マス数の合計）。
	// 発動済みのアイテム効果（attackPower_・コスト増減）は巻き戻さない
	currentCost_ = (std::min)(currentCost_ + static_cast<int>(waypoints_.size()), maxCost);
	waypoints_.clear();
}

void GridBoardPlayerComponent::TriggerItemsAlongPath(int fromCol, int fromRow, int toCol, int toRow, const std::vector<GameObject*>* sceneObjects) {
	if (!sceneObjects) return;

	int stepCol = (toCol > fromCol) ? 1 : (toCol < fromCol) ? -1 : 0;
	int stepRow = (toRow > fromRow) ? 1 : (toRow < fromRow) ? -1 : 0;
	int steps = (stepCol != 0) ? std::abs(toCol - fromCol) : std::abs(toRow - fromRow);

	std::uniform_int_distribution<int> coinFlip(0, 1);

	for (int i = 1; i <= steps; i++) {
		int col = fromCol + stepCol * i;
		int row = fromRow + stepRow * i;

		for (GameObject* obj : *sceneObjects) {
			if (!obj) continue;
			auto* item = obj->GetComponent<GridItemComponent>();
			if (!item) continue;
			if (item->col != col || item->row != row) continue;

			switch (item->type) {
			case GridItemComponent::Type::kAttackPower:
				attackPower_ += 1;
				break;
			case GridItemComponent::Type::kCostFixed:
				currentCost_ += 2;
				break;
			case GridItemComponent::Type::kCostRisky:
				currentCost_ += (coinFlip(rng_) == 0) ? 4 : -4;
				currentCost_ = (std::max)(currentCost_, 1);
				break;
			}
			triggeredItems_.push_back(obj);
			break; // 同じマスに複数アイテムは想定しない
		}
	}
}

void GridBoardPlayerComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	bool leftPressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool clickedThisFrame = !isFirstUpdate_ && ctx.isGameView && leftPressed && !prevMouseLeftPressed_;
	prevMouseLeftPressed_ = leftPressed;
	isFirstUpdate_ = false;

	const GridBoardComponent* board = FindBoard(ctx.sceneObjects);

	if (phase_ == Phase::kPlanning) {
		if (clickedThisFrame && board && currentCost_ > 0) {
			int col, row;
			if (TryPickCell(transform, ctx, col, row)) {
				int prevCol, prevRow;
				if (waypoints_.empty()) {
					int c, r;
					board->WorldToNearestGrid(transform.translation, c, r);
					prevCol = c; prevRow = r;
				} else {
					prevCol = waypoints_.back().first;
					prevRow = waypoints_.back().second;
				}

				// 同じ行/列上のみ許可（斜め・任意方向は不可）
				bool sameRow = (row == prevRow && col != prevCol);
				bool sameCol = (col == prevCol && row != prevRow);
				if (sameRow || sameCol) {
					int distance = sameRow ? std::abs(col - prevCol) : std::abs(row - prevRow);
					if (distance <= currentCost_) {
						waypoints_.push_back({ col, row });
						currentCost_ -= distance;

						// 通過マスにアイテムがあれば即時発動する（攻撃力加算・コスト増減）。
						// currentCost_が0以下になっていても、アイテムでコストが回復する場合が
						// あるため、発動判定は必ずここで行ってから下のコスト切れチェックに進む
						TriggerItemsAlongPath(prevCol, prevRow, col, row, ctx.sceneObjects);

						// コストを使い切ったら、それ以上予約できないため自動的に実行フェーズへ移る
						if (currentCost_ <= 0) {
							phase_ = Phase::kExecuting;
							currentWaypointIndex_ = 0;
							segmentStarted_ = false;
							return;
						}
					}
				}
			}
		}

		return;
	}

	// 実行フェーズ：waypoints_を先頭から順に、区間ごとにイージング補間しながら直進する
	if (!board || currentWaypointIndex_ >= waypoints_.size()) {
		// 盤面が見つからない、または予約が空のまま実行フェーズに来た場合は即座に計画フェーズへ戻す
		phase_ = Phase::kPlanning;
		currentWaypointIndex_ = 0;
		currentCost_ = maxCost;
		attackPower_ = 0;
		return;
	}

	if (!segmentStarted_) {
		Vector3 targetWorld = board->GridToWorld(waypoints_[currentWaypointIndex_].first, waypoints_[currentWaypointIndex_].second);
		BeginSegment(transform.translation, targetWorld);
	}

	segmentElapsed_ += deltaTime;
	float t = (segmentDuration_ > 0.0f) ? (segmentElapsed_ / segmentDuration_) : 1.0f;

	if (t >= 1.0f) {
		transform.translation.x = segmentEnd_.x;
		transform.translation.y = segmentEnd_.y;
		++currentWaypointIndex_;
		if (currentWaypointIndex_ >= waypoints_.size()) {
			// 実行完了：次ターンへ。コスト・攻撃力は満タン/0へリセットする
			phase_ = Phase::kPlanning;
			waypoints_.clear();
			currentWaypointIndex_ = 0;
			segmentStarted_ = false;
			currentCost_ = maxCost;
			attackPower_ = 0;
		} else {
			Vector3 nextTargetWorld = board->GridToWorld(waypoints_[currentWaypointIndex_].first, waypoints_[currentWaypointIndex_].second);
			BeginSegment(segmentEnd_, nextTargetWorld);
		}
	} else {
		float easedT = Easing::Apply(easingType, t);
		Vector3 lerped = segmentStart_ + (segmentEnd_ - segmentStart_) * easedT;
		transform.translation.x = lerped.x;
		transform.translation.y = lerped.y;
	}
}

void GridBoardPlayerComponent::DrawImGui(const char* namePrefix) {
	std::string maxCostLabel = std::string(namePrefix) + "移動可能マス コスト";
	if (ImGui::DragInt(maxCostLabel.c_str(), &maxCost, 1, 1, 999)) {
		currentCost_ = (std::min)(currentCost_, maxCost);
	}

	std::string speedLabel = std::string(namePrefix) + "移動速度";
	ImGui::DragFloat(speedLabel.c_str(), &moveSpeed, 0.1f, 0.0f, 20.0f);

	std::string easingLabel = std::string(namePrefix) + "イージング";
	int easingIndex = static_cast<int>(easingType);
	const char* const* easingNames = Easing::GetTypeNames();
	if (ImGui::BeginCombo(easingLabel.c_str(), easingNames[easingIndex])) {
		for (int i = 0; i < static_cast<int>(Easing::Type::kCount); i++) {
			bool selected = (i == easingIndex);
			if (ImGui::Selectable(easingNames[i], selected)) easingType = static_cast<Easing::Type>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	std::string highlightColorLabel = std::string(namePrefix) + "移動可能マスのハイライト色";
	ImGui::ColorEdit4(highlightColorLabel.c_str(), &highlightColor.x);

	std::string reservedColorLabel = std::string(namePrefix) + "予約済みマスの色";
	ImGui::ColorEdit4(reservedColorLabel.c_str(), &reservedColor.x);

	const char* phaseLabel = (phase_ == Phase::kPlanning) ? "計画フェーズ" : "実行フェーズ";
	ImGui::Text("%s", (std::string(namePrefix) + "フェーズ: " + phaseLabel).c_str());
	ImGui::Text("%s", (std::string(namePrefix) + "残りコスト: "
		+ std::to_string(currentCost_) + " / " + std::to_string(maxCost)).c_str());
	ImGui::Text("%s", (std::string(namePrefix) + "このターンの攻撃力: " + std::to_string(attackPower_)).c_str());

	if (phase_ == Phase::kPlanning) {
		std::string clearLabel = std::string(namePrefix) + "経路をクリア";
		if (ImGui::Button(clearLabel.c_str())) {
			ClearWaypoints();
		}
		ImGui::SameLine();
		std::string toExecuteLabel = std::string(namePrefix) + "実行フェーズへ";
		if (ImGui::Button(toExecuteLabel.c_str()) && !waypoints_.empty()) {
			phase_ = Phase::kExecuting;
			currentWaypointIndex_ = 0;
			segmentStarted_ = false;
		}
	}
}

void GridBoardPlayerComponent::ToJson(nlohmann::json& out) const {
	out["maxCost"] = maxCost;
	out["moveSpeed"] = moveSpeed;
	out["easingType"] = static_cast<int>(easingType);
	out["highlightColor"] = Vector4ToJson(highlightColor);
	out["reservedColor"] = Vector4ToJson(reservedColor);
}

void GridBoardPlayerComponent::FromJson(const nlohmann::json& in) {
	maxCost = in.value("maxCost", maxCost);
	moveSpeed = in.value("moveSpeed", moveSpeed);
	easingType = static_cast<Easing::Type>(in.value("easingType", static_cast<int>(easingType)));
	if (in.contains("highlightColor")) highlightColor = Vector4FromJson(in["highlightColor"]);
	if (in.contains("reservedColor")) reservedColor = Vector4FromJson(in["reservedColor"]);
	currentCost_ = maxCost;
}

REGISTER_SIMPLE_COMPONENT(GridBoardPlayerComponent, "GridBoardPlayer", "グリッドボードプレイヤー移動", "物理");
