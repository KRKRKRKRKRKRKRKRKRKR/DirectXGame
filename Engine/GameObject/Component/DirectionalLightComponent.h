#pragma once
#include "../IComponent.h"
#include "../../../Math/MathTypes.h"

class Renderer;

// GameObjectに付与する平行光源（Directional Light）。位置・向きは自分では持たず、
// オーナーのTransform.rotationから方向を導出する（Gizmoで回転させれば光の向きが変わる）。
// SceneLightへの反映は毎フレームSyncToRenderer()を明示的に呼ぶ必要がある
// （Draw()と同じ理由でIComponent::Updateの仮想関数には乗せていない）
class DirectionalLightComponent : public IComponent {
public:
	bool    enabled = true;
	Vector3 color = { 1.0f, 1.0f, 1.0f };
	float   ambient = 0.2f;
	float   halfLambertPower = 2.0f;

	// transform.rotationをTransformMath::EulerRadiansToDirectionで方向ベクトルに変換し、
	// Renderer::GetLight()のSetter経由でSceneLightへ反映する（Upload()を確実に通すため）
	void SyncToRenderer(Renderer* renderer, const Transform& transform) const;

	// transform.translationに黄色い球、方向へラインを描画する（デバッグ可視化）
	void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const;

	void DrawImGui(const char* namePrefix) override;
};
