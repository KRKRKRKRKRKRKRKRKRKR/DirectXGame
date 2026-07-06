#pragma once
#include "RenderComponentBase.h"

// Sprite3DとSprite2Dの両方に使う。UVTransformを追加で持ち、is3Dフラグに応じて
// Draw()内でDrawSprite3D/DrawSprite2Dのどちらを呼ぶか切り替える
class SpriteRenderComponent : public RenderComponentBase {
public:
	explicit SpriteRenderComponent(bool is3D) : is3D(is3D) {}

	void Draw(Renderer* renderer, const Transform& transform) const;

	// "{headerPrefix} UV Transform"という見出しの下に"{fieldPrefix} UV Offset/Rotation/Scale"を描画する。
	// 見出しとフィールドで別々のprefixを取るのは、既存UIが"Sprite3D UV Transform"（見出し）と
	// "3D UV Offset"（フィールド）のように異なる接頭辞を使っていたものをそのまま維持するため
	void DrawUVTransformImGui(const char* headerPrefix, const char* fieldPrefix);

	bool        is3D;
	UVTransform uvTransform;
};
