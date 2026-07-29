#pragma once
#include "../../IComponent.h"
#include "ILightComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/JsonUtil.h"

class Renderer;

// GameObjectに付与するスポットライト（Spot Light）。位置・向きは自分では持たず、
// オーナーのTransform.translation/rotationから導出する（Gizmoで動かす・回転させれば
// スポットの位置・向きが変わる）。SceneLightへの反映は毎フレームSyncToRenderer()を
// 明示的に呼ぶ必要がある
class SpotLightComponent : public IComponent, public ILightComponent {
public:
	bool    enabled = false;
	Vector3 color = { 1.0f, 1.0f, 1.0f };
	float   intensity = 1.0f;
	float   distance = 7.0f;
	float   decay = 2.0f;
	float   cosAngle = 0.8f;        // 外側コーンのcos（この角度より外は完全に暗い）
	float   cosFalloffStart = 0.9f; // 内側コーンのcos（この角度より内は完全に明るい）

	LightType GetLightType() const override { return LightType::kSpot; }

	// slotIndex: SceneLight::LightData::spotLights配列の何番目に書き込むか
	void SyncToRenderer(Renderer* renderer, const Transform& transform, uint32_t slotIndex) const override;

	// enabled時のみtransform.translationに色付き球、方向へラインを描画する（デバッグ可視化）
	void DrawGizmoVisualization(Renderer* renderer, const Transform& transform, const Matrix4x4& view, const Matrix4x4& proj) const override;

	void DrawImGui(const char* namePrefix) override;

	// 位置・向き(translation/rotation)はTransformComponent側で保存されるためここでは扱わない
	void ToJson(nlohmann::json& out) const override {
		out["enabled"] = enabled;
		out["color"] = Vector3ToJson(color);
		out["intensity"] = intensity;
		out["distance"] = distance;
		out["decay"] = decay;
		out["cosAngle"] = cosAngle;
		out["cosFalloffStart"] = cosFalloffStart;
	}
	void FromJson(const nlohmann::json& in) override {
		enabled = in.value("enabled", enabled);
		if (in.contains("color")) color = Vector3FromJson(in["color"]);
		intensity = in.value("intensity", intensity);
		distance = in.value("distance", distance);
		decay = in.value("decay", decay);
		cosAngle = in.value("cosAngle", cosAngle);
		cosFalloffStart = in.value("cosFalloffStart", cosFalloffStart);
	}

private:
	// PointLightComponentと同じ理由（DrawImGuiで上限超過を警告表示するためのキャッシュ）
	mutable uint32_t lastSlotIndex_ = 0;
};
