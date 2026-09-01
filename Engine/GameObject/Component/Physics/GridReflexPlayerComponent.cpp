// 10DaysJam
#include "GridReflexPlayerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <string>

GridReflexPlayerComponent::GridReflexPlayerComponent() {
	// 基底のfieldRange（既定±8）が盤面より狭いと、盤面の外側寄りのマスへのクリックが
	// TryPickPoint（基底実装）の時点で弾かれてしまう。有効範囲の判定は本クラスのグリッド境界
	// チェック（gridWidth/gridHeight）に一本化したいので、基底側の制限は実質無効にしておく
	fieldRangeMinX = -1000.0f;
	fieldRangeMaxX = 1000.0f;
	fieldRangeMinY = -1000.0f;
	fieldRangeMaxY = 1000.0f;
}

bool GridReflexPlayerComponent::TryPickPoint(const Transform& transform, const UpdateContext& ctx, Vector3& outPosition) const {
	Vector3 rawPoint;
	if (!ReflexPlayerComponent::TryPickPoint(transform, ctx, rawPoint)) return false;

	int col, row;
	WorldToNearestGrid(rawPoint, col, row);
	if (col < 0 || col >= gridWidth || row < 0 || row >= gridHeight) return false;

	// 基準地点：直前の予約地点（無ければ現在地）。このコンポーネントは自分専用の位置状態を
	// 持たないため、基底のGetWaypointCount()/GetWaypoint()（経路予約の実体）をそのまま使う
	Vector3 refPos = (GetWaypointCount() > 0) ? GetWaypoint(GetWaypointCount() - 1) : transform.translation;
	int refCol, refRow;
	WorldToNearestGrid(refPos, refCol, refRow);

	int dCol = col - refCol;
	int dRow = row - refRow;

	// 同じ行/列上のクリックのみ有効（斜め・その場クリックは無効）
	bool sameRow = (dRow == 0 && dCol != 0);
	bool sameCol = (dCol == 0 && dRow != 0);
	if (!sameRow && !sameCol) return false;

	int distance = sameRow ? std::abs(dCol) : std::abs(dRow);
	if (distance < minJumpDistance || distance > maxJumpDistance) return false;

	outPosition = GridToWorld(col, row);
	outPosition.z = transform.translation.z;
	return true;
}

std::vector<std::pair<int, int>> GridReflexPlayerComponent::GetValidTargets(const Transform& transform) const {
	std::vector<std::pair<int, int>> targets;
	if (GetPhase() != Phase::kPlanning) return targets;
	if (GetWaypointCount() >= static_cast<size_t>((std::max)(maxWaypoints, 0))) return targets;

	Vector3 refPos = (GetWaypointCount() > 0) ? GetWaypoint(GetWaypointCount() - 1) : transform.translation;
	int refCol, refRow;
	WorldToNearestGrid(refPos, refCol, refRow);

	constexpr int kDirections[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
	for (const auto& dir : kDirections) {
		for (int dist = minJumpDistance; dist <= maxJumpDistance; ++dist) {
			int col = refCol + dir[0] * dist;
			int row = refRow + dir[1] * dist;
			if (col < 0 || col >= gridWidth || row < 0 || row >= gridHeight) continue;
			targets.push_back({ col, row });
		}
	}
	return targets;
}

Vector3 GridReflexPlayerComponent::GridToWorld(int col, int row) const {
	// マス(0,0)は常にワールド原点（固定アンカー）。以前はgridWidth/gridHeightに応じて盤面を
	// 中央寄せしていたが、その方式だと盤面（GridBoardComponent::columns/rows）とプレイヤー自身の
	// gridWidth/gridHeightが一致しない限り、同じ(col,row)でも計算結果が食い違ってしまい、
	// 盤面サイズを変えた瞬間にプレイヤーの移動先判定・ハイライト位置がズレるバグになっていた。
	// gridWidth/gridHeightに依存しない絶対座標にすることで、盤面側とプレイヤー側のサイズが
	// 一致していなくても常に同じ(col,row)が同じワールド座標を指すようにする（GridPuzzleScene::
	// RebuildTilesのタイル配置式と必ず同じ式にすること）。行は値が大きいほど下（Y-方向）へ
	// 進むようにし、見た目の「上から下へ数える」感覚に合わせる
	Vector3 result{ 0.0f, 0.0f, 0.0f };
	result.x = static_cast<float>(col) * cellSpacing;
	result.y = -static_cast<float>(row) * cellSpacing;
	return result;
}

void GridReflexPlayerComponent::WorldToNearestGrid(const Vector3& worldPos, int& outCol, int& outRow) const {
	// GridToWorldの逆変換。四捨五入で最寄りのマスへスナップする
	outCol = static_cast<int>(std::lround(worldPos.x / cellSpacing));
	outRow = static_cast<int>(std::lround(-worldPos.y / cellSpacing));
}

void GridReflexPlayerComponent::DrawImGui(const char* namePrefix) {
	ReflexPlayerComponent::DrawImGui(namePrefix);

	ImGui::Separator();
	ImGui::Text("%s", (std::string(namePrefix) + "-- グリッド制約(10DaysJam) --").c_str());

	std::string widthLabel = std::string(namePrefix) + "盤面の列数";
	std::string heightLabel = std::string(namePrefix) + "盤面の行数";
	std::string spacingLabel = std::string(namePrefix) + "マス間隔";
	std::string minDistLabel = std::string(namePrefix) + "最小ジャンプ距離";
	std::string maxDistLabel = std::string(namePrefix) + "最大ジャンプ距離";
	std::string highlightLabel = std::string(namePrefix) + "移動先ハイライトの色";
	ImGui::DragInt(widthLabel.c_str(), &gridWidth, 1.0f, 1, 50);
	ImGui::DragInt(heightLabel.c_str(), &gridHeight, 1.0f, 1, 50);
	ImGui::DragFloat(spacingLabel.c_str(), &cellSpacing, 0.05f, 0.1f, 10.0f);
	ImGui::DragInt(minDistLabel.c_str(), &minJumpDistance, 1.0f, 1, maxJumpDistance);
	ImGui::DragInt(maxDistLabel.c_str(), &maxJumpDistance, 1.0f, minJumpDistance, 10);
	ImGui::ColorEdit4(highlightLabel.c_str(), &highlightColor.x);
}

void GridReflexPlayerComponent::ToJson(nlohmann::json& out) const {
	ReflexPlayerComponent::ToJson(out);
	out["gridWidth"] = gridWidth;
	out["gridHeight"] = gridHeight;
	out["cellSpacing"] = cellSpacing;
	out["minJumpDistance"] = minJumpDistance;
	out["maxJumpDistance"] = maxJumpDistance;
	out["highlightColor"] = Vector4ToJson(highlightColor);
}

void GridReflexPlayerComponent::FromJson(const nlohmann::json& in) {
	ReflexPlayerComponent::FromJson(in);
	gridWidth = in.value("gridWidth", gridWidth);
	gridHeight = in.value("gridHeight", gridHeight);
	cellSpacing = in.value("cellSpacing", cellSpacing);
	minJumpDistance = in.value("minJumpDistance", minJumpDistance);
	maxJumpDistance = in.value("maxJumpDistance", maxJumpDistance);
	if (in.contains("highlightColor")) highlightColor = Vector4FromJson(in["highlightColor"]);
}

REGISTER_SIMPLE_COMPONENT(GridReflexPlayerComponent, "GridReflexPlayer", "グリッドREFLEXプレイヤー移動", "物理");
