#pragma once
#include "../IComponent.h"
#include "../../../Math/MathTypes.h"

class Renderer;

// GameObjectに付与する点光源（Point Light）。位置は自分では持たず、
// オーナーのTransform.translationをそのまま使う（Gizmoで動かせば光の位置が変わる）。
// SceneLightへの反映は毎フレームSyncToRenderer()を明示的に呼ぶ必要がある
class PointLightComponent : public IComponent {
public:
	bool    enabled = false;
	Vector3 color = { 1.0f, 1.0f, 1.0f };
	float   intensity = 1.0f;
	float   radius = 5.0f;
	float   decay = 1.0f;

	void SyncToRenderer(Renderer* renderer, const Transform& transform) const;

	// enabled時のみtransform.translationに色付き球を描画する（デバッグ可視化）
	void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const;

	void DrawImGui(const char* namePrefix) override;
};
