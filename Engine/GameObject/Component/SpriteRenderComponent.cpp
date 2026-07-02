#include "SpriteRenderComponent.h"

void SpriteRenderComponent::Draw(Renderer* renderer, const Transform& transform) const {
	if (is3D) {
		renderer->DrawSprite3D(transform, color, textureHandle, lighting, uvTransform, blendMode, blendStrength, alphaTest, alphaThreshold);
	} else {
		renderer->DrawSprite2D(transform, color, textureHandle, lighting, uvTransform, blendMode, blendStrength, alphaTest, alphaThreshold);
	}
}
