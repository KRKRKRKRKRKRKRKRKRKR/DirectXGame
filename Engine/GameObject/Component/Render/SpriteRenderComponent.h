#pragma once
#include "RenderComponentBase.h"
#include "../../../../Math/JsonUtil.h"

// Sprite3DとSprite2Dの両方に使う。UVTransformを追加で持ち、is3Dフラグに応じて
// Draw()内でDrawSprite3D/DrawSprite2Dのどちらを呼ぶか切り替える
class SpriteRenderComponent : public RenderComponentBase {
public:
	explicit SpriteRenderComponent(bool is3D) : is3D(is3D) {}

	void Draw(Renderer* renderer, const Transform& transform, float deltaTime) const override;

	// 共通項目（RenderComponentBase::DrawImGui）に加えて、UV Transform（Offset/Rotation/Scale）も描画する
	void DrawImGui(const char* namePrefix) override;

	bool        is3D;
	UVTransform uvTransform;

	// is3Dはコンストラクタ引数のため、復元はComponentRegistryのcreatorがJSONから読んで
	// AddComponent<SpriteRenderComponent>(is3D)を呼び直す形になる（FromJsonでは変更しない）
	void ToJson(nlohmann::json& out) const override {
		RenderComponentBase::ToJson(out);
		out["is3D"] = is3D;
		out["uvOffset"] = Vector2ToJson(uvTransform.offset);
		out["uvRotation"] = uvTransform.rotation;
		out["uvScale"] = Vector2ToJson(uvTransform.scale);
	}
	void FromJson(const nlohmann::json& in) override {
		RenderComponentBase::FromJson(in);
		if (in.contains("uvOffset")) uvTransform.offset = Vector2FromJson(in["uvOffset"]);
		uvTransform.rotation = in.value("uvRotation", uvTransform.rotation);
		if (in.contains("uvScale")) uvTransform.scale = Vector2FromJson(in["uvScale"]);
	}
};
