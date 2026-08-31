#pragma once
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Easing.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../Graphics/Renderer/LazyModelHandle.h"
#include "../../IComponent.h"
#include <vector>

// ReflexPlayerComponentの経路可視化（波紋マーカー・破線・障害物マージン/フィールド範囲の
// デバッグワイヤーフレーム）を切り出した描画専用ヘルパー。ReflexPlayerComponentは
// 「状態遷移・入力処理・移動ロジック・当たり判定」に、こちらは「見た目」に責務を分ける。
// IComponentではない（GameObjectには付けず、ReflexPlayerComponentがメンバとして1個持ち、
// Update内から呼び出す）。waypoints_（プレイヤーの経路予約）はReflexPlayerComponent側の
// 状態のままにし、描画に必要な分だけ引数で受け渡す設計にすることで、無理な相互依存を避ける
class ReflexPathVisualizer {
public:
	// ---- 見た目パラメータ（Inspectorで調整可能） ----
	Vector4 markerColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	float markerPulseMinScale = 0.15f;
	float markerPulseMaxScale = 0.3f;
	float markerPulseDuration = 1.0f;
	int markerWaveCount = 3;

	Vector4 lineColor = { 1.0f, 0.9f, 0.2f, 1.0f };
	float lineDashLength = 0.3f;
	float lineGapLength = 0.2f;
	float lineThickness = 0.1f;
	float lineScrollSpeed = 1.0f;

	// 経路の可視化：waypoints[startIndex]以降の各地点にCircle.objマーカー（ロード失敗時は球体）、
	// 区間（playerPosition→waypoints[startIndex]→…）に破線を描く
	void DrawPlanning(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
		const Vector3& playerPosition, const std::vector<Vector3>& waypoints, size_t startIndex, float deltaTime);

	// obstacleMargin込みの障害物形状（Sphere/OBB）のワイヤーフレームを描く。sceneObjectsから
	// CollisionLayer::kObstacleのColliderを持つものだけ対象にする
	void DrawObstacleMargin(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
		const std::vector<GameObject*>& sceneObjects, float obstacleMargin) const;

	// fieldRangeMin/Maxの範囲を、Z方向に薄い直方体のワイヤーフレームで描く
	void DrawFieldRange(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
		float centerZ, float fieldRangeMinX, float fieldRangeMaxX,
		float fieldRangeMinY, float fieldRangeMaxY) const;

	// "{namePrefix} 経路マーカーの色"等、見た目パラメータ一式のInspector UIを描画する
	void DrawImGui(const char* namePrefix);

	void ToJson(nlohmann::json& out) const;
	void FromJson(const nlohmann::json& in);

private:
	void DrawDashedLine(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj,
		const Vector3& from, const Vector3& to, const Vector4& color, float scrollOffset) const;

	// 経路マーカー用モデル（Resources/Model/Circle.obj）の遅延ロード状態
	LazyModelHandle circleModel_{ "Resources/Model", "Circle.obj" };

	// マーカーの波紋アニメーションの基準経過時間
	mutable float markerPulseElapsed_ = 0.0f;

	// 破線パターンが進行方向へ流れる演出の経過距離
	mutable float lineScrollElapsed_ = 0.0f;
};
