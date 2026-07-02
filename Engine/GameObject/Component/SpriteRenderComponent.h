#pragma once
#include "RenderComponentBase.h"

// Sprite3DとSprite2Dの両方に使う。UVTransformを追加で持ち、is3Dフラグに応じて
// Draw()内でDrawSprite3D/DrawSprite2Dのどちらを呼ぶか切り替える
class SpriteRenderComponent : public RenderComponentBase {
public:
	explicit SpriteRenderComponent(bool is3D) : is3D(is3D) {}

	void Draw(Renderer* renderer, const Transform& transform) const;

	bool        is3D;
	UVTransform uvTransform;
};
