// 10DaysJam
#include "GridBoardPlayerComponent.h"
#include "GridBoardComponent.h"
#include "GridItemComponent.h"
#include "GridWallComponent.h"
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

	// sceneObjectsから指定マス(col,row)にあるGridWallComponentを探して返す（無ければnullptr）。
	// 壁の数は数個〜十数個程度の想定のため、毎回シーン全体を線形探索しても実用上問題にならない
	const GridWallComponent* FindWallAt(const std::vector<GameObject*>* sceneObjects, int col, int row) {
		if (!sceneObjects) return nullptr;
		for (GameObject* obj : *sceneObjects) {
			if (!obj) continue;
			if (auto* wall = obj->GetComponent<GridWallComponent>()) {
				if (wall->col == col && wall->row == row) return wall;
			}
		}
		return nullptr;
	}

	// 1マスぶんの移動コスト。壁マスならその壁のpassCost、無ければ通常の1
	int CellMoveCost(const std::vector<GameObject*>* sceneObjects, int col, int row) {
		const GridWallComponent* wall = FindWallAt(sceneObjects, col, row);
		return wall ? wall->passCost : 1;
	}

	// (fromCol,fromRow)から(toCol,toRow)まで（同じ行/列上の直線移動）実際に通過する各マスの
	// コストを合計する。GridBoardPlayerComponent::GetReservedPathCellsと同じ「1マスずつ進みながら
	// 通過マスを数える」ロジックをコスト集計用に転用したもの
	int ComputePathCost(const std::vector<GameObject*>* sceneObjects, int fromCol, int fromRow, int toCol, int toRow) {
		int stepCol = (toCol > fromCol) - (toCol < fromCol);
		int stepRow = (toRow > fromRow) - (toRow < fromRow);
		int steps = (std::max)(std::abs(toCol - fromCol), std::abs(toRow - fromRow));

		int cost = 0;
		for (int s = 1; s <= steps; ++s) {
			cost += CellMoveCost(sceneObjects, fromCol + stepCol * s, fromRow + stepRow * s);
		}
		return cost;
	}
}

bool GridBoardPlayerComponent::TryPickCell(const Transform& transform, const UpdateContext& ctx, int& outCol, int& outRow) const {
	if (!ctx.renderer) return false;
	if (ImGui::GetIO().WantCaptureMouse) return false; // ImGuiパネル上のクリックは無視

	const GridBoardComponent* board = FindBoard(ctx.sceneObjects);
	if (!board) return false;

	Collision::Ray ray = ScreenRay::FromMouse(ctx.renderer, ctx.view, ctx.proj);

	// このゲームはX-Z平面（水平な地面）上で進行するため、プレイヤーと同じ高さ(Y座標)の
	// X-Z平面（法線Y+）をクリック対象の面とみなす（見下ろしカメラからのレイと地面の交点を求める）
	Collision::Plane fieldPlane{ { 0.0f, 1.0f, 0.0f }, transform.translation.y };
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

	// 4方向それぞれへ1マスずつ進みながら、通過するマスのコスト（壁マスはGridWallComponent::
	// passCost、それ以外は1）を積算していく。積算コストが残りコストを超えた時点でその方向は
	// 打ち切る（壁を挟むと、同じ残りコストでも届く距離が短くなる）
	auto walk = [&](int colStep, int rowStep) {
		int accumulated = 0;
		int col = originCol;
		int row = originRow;
		for (;;) {
			col += colStep;
			row += rowStep;
			if (col < 0 || col >= board->columns || row < 0 || row >= board->rows) break;
			accumulated += CellMoveCost(sceneObjects, col, row);
			if (accumulated > currentCost_) break;
			result.push_back({ col, row });
		}
	};
	walk(1, 0);
	walk(-1, 0);
	walk(0, 1);
	walk(0, -1);

	return result;
}

std::vector<std::pair<int, int>> GridBoardPlayerComponent::GetReservedPathCells(const Transform& transform, const std::vector<GameObject*>* sceneObjects) const {
	std::vector<std::pair<int, int>> cells;
	if (waypoints_.empty()) return cells;

	// 実行フェーズ中は既に通過し終えた区間（currentWaypointIndex_より前）を対象から除外し、
	// 現在向かっている区間以降だけを計算する。計画フェーズ中はcurrentWaypointIndex_が
	// 常に0のため、実質全区間が対象になる（＝計画フェーズ・実行フェーズを問わず同じロジックで
	// 「これから進む残りのマス」を返せる）。
	// 現在地（transform.translation）は、実行フェーズ中はイージング補間中の座標だが、
	// 移動は常に現在向かっている区間の直線（同じ行 or 同じ列）上を動くため、その直線上の
	// 最寄りマスへは正しく丸められる。既に通過済みの区間（別の直線上にある）を含めてしまうと、
	// この現在地との位置関係が軸に沿わなくなり、斜め方向にマスを拾ってしまうバグになるため、
	// 対象を現在の区間以降だけに絞ることが重要
	if (currentWaypointIndex_ >= waypoints_.size()) return cells;

	const GridBoardComponent* board = FindBoard(sceneObjects);
	int curCol = 0, curRow = 0;
	if (board) {
		board->WorldToNearestGrid(transform.translation, curCol, curRow);
	}

	// 現在地→waypoints_[currentWaypointIndex_]→…と区間ごとに、1マスずつ進みながら通過マスを
	// 全て積んでいく（移動は常に同じ行/列上の直線のため、列と行のどちらか一方だけが
	// 1マスずつ変化する）
	for (size_t i = currentWaypointIndex_; i < waypoints_.size(); ++i) {
		const auto& waypoint = waypoints_[i];
		int stepCol = (waypoint.first > curCol) - (waypoint.first < curCol);
		int stepRow = (waypoint.second > curRow) - (waypoint.second < curRow);
		int steps = (std::max)(std::abs(waypoint.first - curCol), std::abs(waypoint.second - curRow));

		for (int s = 1; s <= steps; ++s) {
			cells.push_back({ curCol + stepCol * s, curRow + stepRow * s });
		}

		curCol = waypoint.first;
		curRow = waypoint.second;
	}

	return cells;
}

void GridBoardPlayerComponent::ClearWaypoints() {
	// 予約時に即時消費した分をまとめて払い戻す。waypointCosts_に各予約が実際に消費した
	// コスト（壁マスを含む区間ほど大きい）を控えてあるため、その合計を払い戻す
	// （waypoints_.size()＝クリック回数はコストの合計とは限らないため使わない）。
	// 発動済みのアイテム効果（attackPower_・コスト増減）は巻き戻さない
	int refund = 0;
	for (int cost : waypointCosts_) refund += cost;
	currentCost_ = (std::min)(currentCost_ + refund, maxCost);
	waypoints_.clear();
	waypointCosts_.clear();
}

void GridBoardPlayerComponent::ApplyItemEffect(GridItemComponent::Type type) {
	std::uniform_int_distribution<int> coinFlip(0, 1);

	switch (type) {
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
					// 経路上に壁マスがあれば、その区間の消費コストは距離（マス数）そのままではなく
					// 壁のpassCostぶん上乗せされる（ComputePathCost参照）
					int pathCost = ComputePathCost(ctx.sceneObjects, prevCol, prevRow, col, row);
					if (pathCost <= currentCost_) {
						waypoints_.push_back({ col, row });
						waypointCosts_.push_back(pathCost);
						currentCost_ -= pathCost;

						// アイテム発動はColliderSystemのOnTriggerEnter経由（GridItemComponent側）で
						// 行うため、ここではマス座標を見た判定は行わない

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
		transform.translation.z = segmentEnd_.z;
		++currentWaypointIndex_;
		if (currentWaypointIndex_ >= waypoints_.size()) {
			// 実行完了：次ターンへ。コスト・攻撃力は満タン/0へリセットする
			phase_ = Phase::kPlanning;
			waypoints_.clear();
			waypointCosts_.clear();
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
		transform.translation.z = lerped.z;
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
