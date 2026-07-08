#pragma once
#include "../IComponent.h"
#include "../../../Math/MathTypes.h"

class Renderer;

// GameObjectに付与するスポットライト（Spot Light）。位置・向きは自分では持たず、
// オーナーのTransform.translation/rotationから導出する（Gizmoで動かす・回転させれば
// スポットの位置・向きが変わる）。SceneLightへの反映は毎フレームSyncToRenderer()を
// 明示的に呼ぶ必要がある
class SpotLightComponent : public IComponent {
public:
	bool    enabled = false;
	Vector3 color = { 1.0f, 1.0f, 1.0f };
	float   intensity = 1.0f;
	float   distance = 7.0f;
	float   decay = 2.0f;
	float   cosAngle = 0.8f;        // 外側コーンのcos（この角度より外は完全に暗い）
	float   cosFalloffStart = 0.9f; // 内側コーンのcos（この角度より内は完全に明るい）

	void SyncToRenderer(Renderer* renderer, const Transform& transform) const;

	// enabled時のみtransform.translationに色付き球、方向へラインを描画する（デバッグ可視化）
	void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const;

	void DrawImGui(const char* namePrefix) override;
};
