#include "ReflexPathVisualizer.h"
#include "../../GameObject.h"
#include "ColliderComponentBase.h"
#include "SphereColliderComponent.h"
#include "OBBColliderComponent.h"
#include "../../../../Math/Collision.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/PulseWave.h"
#include "../../../../Externals/imgui/imgui.h"
#include "../../../Graphics/Pipeline/BlendMode.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr Vector4 kWaypointMarkerColor = { 1.0f, 0.9f, 0.2f, 1.0f }; // 黄色い球（Circle.objロード失敗時のフォールバック）
	constexpr Vector4 kObstacleMarginColor = { 1.0f, 0.3f, 0.2f, 1.0f }; // 赤系：これより内側はクリックできない
	constexpr Vector4 kFieldRangeColor     = { 0.2f, 0.6f, 1.0f, 1.0f }; // 水色：この内側だけクリックで経路予約できる
	constexpr int     kWireCircleSegments  = 16;

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

void ReflexPathVisualizer::DrawPlanning(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
	const Vector3& playerPosition, const std::vector<Vector3>& waypoints, size_t startIndex, float deltaTime) {
	if (!renderer) return;
	if (startIndex >= waypoints.size()) return; // 実行フェーズで全区間を通過済みの場合等

	// Circle.objの読み込みは初回呼び出し時に1度だけ試みる（成功・失敗を問わず以降は再試行しない。
	// LoadModelはGPUリソースを新規確保するため毎フレーム呼ぶわけにはいかない）。
	// 注意：Renderer::LoadModelはファイルが見つからない等の失敗時にassert(false)で
	// 落ちる実装（Model_AssimpLoader.cpp）のため、Circle.objが存在する限りここで例外的に
	// フォールバックへ分岐することはない
	Renderer::ModelHandle circleModelHandle = circleModel_.Get(renderer);

	// 波紋アニメーション：基準時計markerPulseElapsed_を進める。duration<=0（Inspectorでの
	// 入力ミス等）は0除算になるため、その場合は波紋を1本・最大スケール固定で表示する
	// （SamplePulseWave内のガードと同じ扱い）
	float duration = (std::max)(markerPulseDuration, 0.0f);
	int waveCount = (std::max)(markerWaveCount, 1);
	if (duration > 0.0f) {
		markerPulseElapsed_ = std::fmod(markerPulseElapsed_ + deltaTime, duration);
	}

	PulseWaveParams pulseParams;
	pulseParams.minScale = markerPulseMinScale;
	pulseParams.maxScale = markerPulseMaxScale;
	pulseParams.duration = markerPulseDuration;
	pulseParams.waveCount = markerWaveCount;

	// 破線が進行方向へ流れる演出のオフセットは、全waypoint区間で共通の値を1度だけ計算する
	// （区間ごとにDrawDashedLine内でlineScrollElapsed_を加算すると、区間数が多い＝経路が長い
	// ほど1フレームあたりの加算回数が増えてしまい、見かけのスクロール速度が距離に依存してしまう）
	float dashPeriod = (std::max)(lineDashLength, 0.01f) + (std::max)(lineGapLength, 0.01f);
	lineScrollElapsed_ += lineScrollSpeed * deltaTime;
	float scrollOffset = std::fmod(lineScrollElapsed_, dashPeriod);
	if (scrollOffset < 0.0f) scrollOffset += dashPeriod; // 負の速度指定でも安全なようにフルクランプ

	Vector3 previous = playerPosition;
	for (size_t i = startIndex; i < waypoints.size(); i++) {
		const Vector3& point = waypoints[i];
		DrawDashedLine(renderer, view, proj, previous, point, lineColor, scrollOffset);

		// waveCount本の波紋を、それぞれduration/waveCountぶん位相をずらして同じ地点に重ねて描く。
		// 各波紋は片道（0→1）：小さい状態から広がりながら不透明度が下がって消え、次の周期でまた
		// 最小サイズから再発生する（水面の波紋のように複数が同時進行して見える）。
		// 計算式はClickHintMarkerComponentと共通のSamplePulseWave（Math/PulseWave.h）を使う
		for (int wave = 0; wave < waveCount; wave++) {
			PulseWaveSample sample = SamplePulseWave(pulseParams, markerPulseElapsed_, wave);
			float alpha = markerColor.w * sample.alphaMultiplier;

			Transform markerTransform;
			markerTransform.translation = point;
			markerTransform.scale = { sample.scale, sample.scale, sample.scale };

			if (circleModelHandle) {
				Vector4 fadedColor = { markerColor.x, markerColor.y, markerColor.z, alpha };
				renderer->DrawModel(circleModelHandle, markerTransform, fadedColor,
					{}, true, BlendMode::kNormal);
			} else {
				Vector4 fadedColor = { kWaypointMarkerColor.x, kWaypointMarkerColor.y, kWaypointMarkerColor.z, alpha };
				renderer->DrawSphere(markerTransform, fadedColor, kTextureNone, true, BlendMode::kNormal);
			}
		}

		previous = point;
	}
}

void ReflexPathVisualizer::DrawDashedLine(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
	const Vector3& from, const Vector3& to, const Vector4& color, float scrollOffset) const {
	(void)view;
	(void)proj;
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

void ReflexPathVisualizer::DrawObstacleMargin(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
	const std::vector<GameObject*>& sceneObjects, float obstacleMargin) const {
	if (!renderer) return;

	for (GameObject* obj : sceneObjects) {
		if (!obj) continue;
		auto* collider = obj->GetComponent<ColliderComponentBase>();
		if (!collider) continue;
		if (collider->layer != CollisionLayer::kObstacle) continue;

		Transform ownerTransform = obj->GetWorldTransform();
		if (auto* sphereCollider = obj->GetComponent<SphereColliderComponent>()) {
			Collision::Sphere sphere = sphereCollider->GetWorldSphere(ownerTransform);
			sphere.radius += obstacleMargin;
			DrawWireCircle(renderer, sphere.center, sphere.radius, 0, 1, kObstacleMarginColor, view, proj);
			DrawWireCircle(renderer, sphere.center, sphere.radius, 1, 2, kObstacleMarginColor, view, proj);
			DrawWireCircle(renderer, sphere.center, sphere.radius, 0, 2, kObstacleMarginColor, view, proj);
		} else if (auto* obbCollider = obj->GetComponent<OBBColliderComponent>()) {
			Collision::OBB obb = obbCollider->GetWorldOBB(ownerTransform);
			obb.Size = obb.Size + Vector3{ obstacleMargin, obstacleMargin, obstacleMargin };
			DrawWireOBB(renderer, obb, kObstacleMarginColor, view, proj);
		}
	}
}

void ReflexPathVisualizer::DrawFieldRange(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
	float centerZ, float fieldRangeMinX, float fieldRangeMaxX,
	float fieldRangeMinY, float fieldRangeMaxY) const {
	if (!renderer) return;

	// fieldRangeMin/Max（TryPickPointが経路予約を許可する範囲）を、Z方向に薄い直方体の
	// ワイヤーフレームとしてコライダーの可視化と同じ見た目で表示する。回転なし・軸並行の
	// 矩形なのでOrientationは単位行列のまま使う
	Collision::OBB obb;
	obb.center = {
		(fieldRangeMinX + fieldRangeMaxX) * 0.5f,
		(fieldRangeMinY + fieldRangeMaxY) * 0.5f,
		centerZ };
	obb.Orientation[0] = { 1.0f, 0.0f, 0.0f };
	obb.Orientation[1] = { 0.0f, 1.0f, 0.0f };
	obb.Orientation[2] = { 0.0f, 0.0f, 1.0f };
	obb.Size = {
		(fieldRangeMaxX - fieldRangeMinX) * 0.5f,
		(fieldRangeMaxY - fieldRangeMinY) * 0.5f,
		0.05f };
	DrawWireOBB(renderer, obb, kFieldRangeColor, view, proj);
}

void ReflexPathVisualizer::DrawImGui(const char* namePrefix) {
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
}

void ReflexPathVisualizer::ToJson(nlohmann::json& out) const {
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
}

void ReflexPathVisualizer::FromJson(const nlohmann::json& in) {
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
}
