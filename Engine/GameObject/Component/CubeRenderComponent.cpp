#include "CubeRenderComponent.h"

void CubeRenderComponent::Draw(Renderer* renderer, const Transform& transform) const {
	renderer->DrawCube(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
