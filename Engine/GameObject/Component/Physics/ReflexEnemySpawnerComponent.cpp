#include "ReflexEnemySpawnerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../../Math/Collision.h"
#include "../../../Graphics/Renderer/Renderer.h"

namespace {
	// ReflexPlayerComponent::kFieldRangeColor（水色、プレイヤーの移動可能範囲）と見分けられるよう、
	// 敵のスポーン範囲は緑系にする
	constexpr Vector4 kSpawnRangeColor = { 0.3f, 1.0f, 0.3f, 1.0f };

	// マージン込みのOBB（obb.Sizeは既にマージン加算済みの想定）の12本の辺を描画する
	// （ReflexPlayerComponent.cpp内のDrawWireOBB/OBBColliderComponent::DrawWireframeと同じロジック。
	// 描画コンポーネントをまたいで共有するヘルパーが無いため、ここに複製している）
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

void ReflexEnemySpawnerComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)deltaTime;
	(void)transform;
	// Sceneビュー（エディタ自由カメラ）表示中のみ表示する。ReflexPlayerComponent::
	// DrawFieldRangeVisualizationと同じ理由（デバッグ用の補助線を実プレイ画面に映り込ませたくない）
	if (ctx.isGameView) return;
	if (!ctx.renderer) return;

	// スポーン位置は常にZ=0固定で決定される（PlayScene::PickEnemySpawnPosition/
	// BuildShuffledSpawnGridがVector3{x, y, 0.0f}を使っているため）。可視化もZ=0平面に薄い直方体で描く
	Collision::OBB obb;
	obb.center = {
		(spawnRangeMinX + spawnRangeMaxX) * 0.5f,
		(spawnRangeMinY + spawnRangeMaxY) * 0.5f,
		0.0f };
	obb.Orientation[0] = { 1.0f, 0.0f, 0.0f };
	obb.Orientation[1] = { 0.0f, 1.0f, 0.0f };
	obb.Orientation[2] = { 0.0f, 0.0f, 1.0f };
	obb.Size = {
		(spawnRangeMaxX - spawnRangeMinX) * 0.5f,
		(spawnRangeMaxY - spawnRangeMinY) * 0.5f,
		0.05f };
	DrawWireOBB(ctx.renderer, obb, kSpawnRangeColor, ctx.view, ctx.proj);
}

void ReflexEnemySpawnerComponent::DrawImGui(const char* namePrefix) {
	// テンプレートのタグ選択（シーン内のReflexEnemyComponent::isTemplate=true一覧からのコンボ）は
	// このコンポーネント単体では描画できない（IComponent::DrawImGuiはシーン全体を参照できないため）。
	// PlayScene::DrawSceneSpecificInspectorExtensions（SceneBase::DrawSceneSpecificInspectorExtensions
	// フックのPlayScene側オーバーライド）が選択中オブジェクトがこのコンポーネントを持つか判定し、
	// DrawImGui呼び出しの直後にコンボ・出現数・追加/削除ボタンを一括で描画する
	(void)namePrefix;
}

void ReflexEnemySpawnerComponent::ToJson(nlohmann::json& out) const {
	nlohmann::json entriesJson = nlohmann::json::array();
	for (const auto& entry : spawnEntries) {
		entriesJson.push_back({ {"tag", entry.tag}, {"count", entry.count} });
	}
	out["spawnEntries"] = entriesJson;
	out["spawnIntervalMin"] = spawnIntervalMin;
	out["spawnIntervalMax"] = spawnIntervalMax;
	out["sizeScaleMin"] = sizeScaleMin;
	out["sizeScaleMax"] = sizeScaleMax;
	out["playerExclusionRadius"] = playerExclusionRadius;
	out["spawnRangeMinX"] = spawnRangeMinX;
	out["spawnRangeMaxX"] = spawnRangeMaxX;
	out["spawnRangeMinY"] = spawnRangeMinY;
	out["spawnRangeMaxY"] = spawnRangeMaxY;
	out["minSpawnDistance"] = minSpawnDistance;
	out["spawnGridCellSize"] = spawnGridCellSize;
	out["totalRounds"] = totalRounds;
}

void ReflexEnemySpawnerComponent::FromJson(const nlohmann::json& in) {
	if (in.contains("spawnEntries") && in["spawnEntries"].is_array() && !in["spawnEntries"].empty()) {
		spawnEntries.clear();
		for (const auto& e : in["spawnEntries"]) {
			SpawnEntry entry;
			entry.tag = e.value("tag", entry.tag);
			entry.count = e.value("count", entry.count);
			spawnEntries.push_back(entry);
		}
	}
	// respawnIntervalは旧キー名（spawnIntervalMin/Maxへリネームする前の保存データ）。
	// 存在すればmin/maxの両方に引き継ぐ（旧仕様は固定間隔だったため）
	if (in.contains("respawnInterval")) {
		float legacyInterval = in.value("respawnInterval", spawnIntervalMin);
		spawnIntervalMin = legacyInterval;
		spawnIntervalMax = legacyInterval;
	}
	spawnIntervalMin = in.value("spawnIntervalMin", spawnIntervalMin);
	spawnIntervalMax = in.value("spawnIntervalMax", spawnIntervalMax);
	sizeScaleMin = in.value("sizeScaleMin", sizeScaleMin);
	sizeScaleMax = in.value("sizeScaleMax", sizeScaleMax);
	playerExclusionRadius = in.value("playerExclusionRadius", playerExclusionRadius);
	// spawnRangeMin/Maxは旧キー名（X/Y共通の正方形だった頃の保存データ）。存在すれば
	// X/Y両方の初期値として引き継ぐ（旧仕様は正方形だったため）
	if (in.contains("spawnRangeMin")) {
		float legacyMin = in.value("spawnRangeMin", spawnRangeMinX);
		spawnRangeMinX = legacyMin;
		spawnRangeMinY = legacyMin;
	}
	if (in.contains("spawnRangeMax")) {
		float legacyMax = in.value("spawnRangeMax", spawnRangeMaxX);
		spawnRangeMaxX = legacyMax;
		spawnRangeMaxY = legacyMax;
	}
	spawnRangeMinX = in.value("spawnRangeMinX", spawnRangeMinX);
	spawnRangeMaxX = in.value("spawnRangeMaxX", spawnRangeMaxX);
	spawnRangeMinY = in.value("spawnRangeMinY", spawnRangeMinY);
	spawnRangeMaxY = in.value("spawnRangeMaxY", spawnRangeMaxY);
	minSpawnDistance = in.value("minSpawnDistance", minSpawnDistance);
	spawnGridCellSize = in.value("spawnGridCellSize", spawnGridCellSize);
	totalRounds = in.value("totalRounds", totalRounds);
}

REGISTER_SIMPLE_COMPONENT(ReflexEnemySpawnerComponent, "ReflexEnemySpawner", "REFLEX敵スポーン設定", "物理");
