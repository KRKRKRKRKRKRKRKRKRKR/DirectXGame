#pragma once
#include "../../IComponent.h"
#include "ILightComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/JsonUtil.h"

class Renderer;

// GameObjectに付与する点光源（Point Light）。位置は自分では持たず、
// オーナーのTransform.translationをそのまま使う（Gizmoで動かせば光の位置が変わる）。
// SceneLightへの反映は毎フレームSyncToRenderer()を明示的に呼ぶ必要がある
class PointLightComponent : public IComponent, public ILightComponent {
public:
	bool    enabled = false;
	Vector3 color = { 1.0f, 1.0f, 1.0f };
	float   intensity = 1.0f;
	float   radius = 5.0f;
	float   decay = 1.0f;

	void SyncToRenderer(Renderer* renderer, const Transform& transform) const override;

	// enabled時のみtransform.translationに色付き球を描画する（デバッグ可視化）
	void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const override;

	void DrawImGui(const char* namePrefix) override;

	// 位置(translation)はTransformComponent側で保存されるためここでは扱わない
	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["color"] = Vector3ToJson(color);
		out["intensity"] = intensity;
		out["radius"] = radius;
		out["decay"] = decay;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		if (in.contains("color")) color = Vector3FromJson(in["color"]);
		intensity = in.value("intensity", intensity);
		radius = in.value("radius", radius);
		decay = in.value("decay", decay);
	}
};
