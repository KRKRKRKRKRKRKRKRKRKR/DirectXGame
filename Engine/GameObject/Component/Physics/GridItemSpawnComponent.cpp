// 10DaysJam
#include "GridItemSpawnComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../../Math/VectorMath.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {
	constexpr const char* kTypeNames[] = { "赤", "緑", "青" };
	constexpr int kTypeCount = static_cast<int>(sizeof(kTypeNames) / sizeof(kTypeNames[0]));

	// Sceneビュー可視化の色・サイズ。ReflexPathVisualizerのkFieldRangeColor等と衝突しない
	// 独自の色にしておく（黄緑：積み重ねの先頭座標マーカー）
	constexpr Vector4 kMarkerColor = { 0.4f, 0.9f, 1.0f, 1.0f };
	constexpr float kMarkerRadius = 0.25f;
	constexpr int kMarkerCircleSegments = 16;
	constexpr float kArrowLength = 1.0f; // 積み重ね方向を示す矢印線の長さ

	// 中心center、半径radiusの円を1枚、指定した2軸(axis0, axis1。0=x,1=y,2=z)平面上に
	// 線分で近似して描画する（ReflexPathVisualizer::DrawWireCircleと同じロジック）
	void DrawWireCircle(Renderer* renderer, const Vector3& center, float radius, int axis0, int axis1,
		const Vector4& color, const Matrix4x4& view, const Matrix4x4& proj) {
		float coords[3] = { center.x, center.y, center.z };
		auto pointAt = [&](float angle) {
			float c[3] = { coords[0], coords[1], coords[2] };
			c[axis0] += cosf(angle) * radius;
			c[axis1] += sinf(angle) * radius;
			return Vector3{ c[0], c[1], c[2] };
			};
		for (int i = 0; i < kMarkerCircleSegments; i++) {
			float a0 = (float)i / kMarkerCircleSegments * 2.0f * 3.14159265f;
			float a1 = (float)(i + 1) / kMarkerCircleSegments * 2.0f * 3.14159265f;
			renderer->DrawLine(pointAt(a0), pointAt(a1), color, view, proj);
		}
	}
}

void GridItemSpawnComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)deltaTime;
	(void)transform;
	if (ctx.isGameView || !ctx.renderer) return; // Gameビュー（実プレイ画面）には描かない

	// collectedDisplayTopを中心に3方向の円でワイヤーフレーム球を近似する（コライダーの
	// DrawWireCircle3枚と同じ考え方）。実際の球体メッシュではなく線描画のみのため、
	// 常時軽量でGameビューへの映り込みも確実に防げる
	DrawWireCircle(ctx.renderer, collectedDisplayTop, kMarkerRadius, 0, 1, kMarkerColor, ctx.view, ctx.proj);
	DrawWireCircle(ctx.renderer, collectedDisplayTop, kMarkerRadius, 1, 2, kMarkerColor, ctx.view, ctx.proj);
	DrawWireCircle(ctx.renderer, collectedDisplayTop, kMarkerRadius, 0, 2, kMarkerColor, ctx.view, ctx.proj);

	// 積み重ねが進む向き（縦=Z-方向／横=X+方向）を短い矢印線で示す。GridPuzzleScene::
	// UpdateCollectedItemsDisplayの実際のオフセット計算（pos.z -= offset／pos.x += offset）と
	// 符号を一致させてある
	Vector3 direction = (collectedDisplayLayout == DisplayLayout::kHorizontal)
		? Vector3{ 1.0f, 0.0f, 0.0f }
		: Vector3{ 0.0f, 0.0f, -1.0f };
	Vector3 arrowEnd = collectedDisplayTop + direction * kArrowLength;
	ctx.renderer->DrawLine(collectedDisplayTop, arrowEnd, kMarkerColor, ctx.view, ctx.proj);
}

void GridItemSpawnComponent::DrawImGui(const char* namePrefix) {
	std::string headerLabel = std::string(namePrefix) + "アイテム種類一覧";
	ImGui::Text("%s", headerLabel.c_str());

	int removeIndex = -1;
	for (size_t i = 0; i < spawnEntries.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		SpawnEntry& entry = spawnEntries[i];
		int typeIndex = static_cast<int>(entry.type);
		std::string typeLabel = std::string(namePrefix) + "種別";
		if (ImGui::BeginCombo(typeLabel.c_str(), kTypeNames[typeIndex])) {
			for (int t = 0; t < kTypeCount; t++) {
				bool selected = (t == typeIndex);
				if (ImGui::Selectable(kTypeNames[t], selected)) entry.type = static_cast<GridItemComponent::Type>(t);
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		std::string countLabel = std::string(namePrefix) + "個数";
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragInt(countLabel.c_str(), &entry.count, 1.0f, 1, 20);
		entry.count = (std::max)(entry.count, 1);

		ImGui::SameLine();
		std::string removeLabel = std::string(namePrefix) + "削除";
		if (ImGui::Button(removeLabel.c_str())) removeIndex = static_cast<int>(i);

		std::string colorLabel = std::string(namePrefix) + "生成時の色";
		ImGui::ColorEdit4(colorLabel.c_str(), &entry.color.x);

		ImGui::PopID();
	}
	if (removeIndex >= 0) spawnEntries.erase(spawnEntries.begin() + removeIndex);

	std::string addLabel = std::string(namePrefix) + "種類を追加";
	if (ImGui::Button(addLabel.c_str())) spawnEntries.push_back(SpawnEntry{});

	std::string clusterRadiusLabel = std::string(namePrefix) + "群生半径(マス、同じ種別内でまとめる距離)";
	ImGui::DragInt(clusterRadiusLabel.c_str(), &clusterRadius, 1, 0, 20);
	clusterRadius = (std::max)(clusterRadius, 0);

	ImGui::Separator();
	std::string displayTopLabel = std::string(namePrefix) + "取得済み表示の一番上の座標";
	std::string displaySpacingLabel = std::string(namePrefix) + "取得済み表示の間隔";
	ImGui::DragFloat3(displayTopLabel.c_str(), &collectedDisplayTop.x, 0.05f);
	ImGui::DragFloat(displaySpacingLabel.c_str(), &collectedDisplaySpacing, 0.05f, 0.05f, 10.0f);

	std::string layoutLabel = std::string(namePrefix)
		+ (collectedDisplayLayout == DisplayLayout::kVertical ? "並び方向: 縦(Z方向)" : "並び方向: 横(X方向)");
	if (ImGui::Button(layoutLabel.c_str())) {
		collectedDisplayLayout = (collectedDisplayLayout == DisplayLayout::kVertical)
			? DisplayLayout::kHorizontal : DisplayLayout::kVertical;
	}

	ImGui::Separator();
	std::string resetLabel = std::string(namePrefix) + "リセット(全アイテムを削除して再配置)";
	if (ImGui::Button(resetLabel.c_str())) resetRequested_ = true;
}

void GridItemSpawnComponent::ToJson(nlohmann::json& out) const {
	nlohmann::json entries = nlohmann::json::array();
	for (const auto& entry : spawnEntries) {
		entries.push_back({
			{ "type", static_cast<int>(entry.type) },
			{ "count", entry.count },
			{ "color", Vector4ToJson(entry.color) },
			});
	}
	out["spawnEntries"] = entries;
	out["collectedDisplayTop"] = Vector3ToJson(collectedDisplayTop);
	out["collectedDisplaySpacing"] = collectedDisplaySpacing;
	out["collectedDisplayLayout"] = static_cast<int>(collectedDisplayLayout);
	out["clusterRadius"] = clusterRadius;
}

void GridItemSpawnComponent::FromJson(const nlohmann::json& in) {
	if (in.contains("spawnEntries")) {
		spawnEntries.clear();
		for (const auto& entryJson : in["spawnEntries"]) {
			SpawnEntry entry;
			entry.type = static_cast<GridItemComponent::Type>(entryJson.value("type", 0));
			entry.count = entryJson.value("count", 1);
			if (entryJson.contains("color")) entry.color = Vector4FromJson(entryJson["color"]);
			spawnEntries.push_back(entry);
		}
	}
	if (in.contains("collectedDisplayTop")) collectedDisplayTop = Vector3FromJson(in["collectedDisplayTop"]);
	collectedDisplaySpacing = in.value("collectedDisplaySpacing", collectedDisplaySpacing);
	collectedDisplayLayout = static_cast<DisplayLayout>(in.value("collectedDisplayLayout", static_cast<int>(collectedDisplayLayout)));
	clusterRadius = in.value("clusterRadius", clusterRadius);
}

REGISTER_SIMPLE_COMPONENT(GridItemSpawnComponent, "GridItemSpawn", "グリッドアイテムスポーン", "物理");
