#include "CubeRenderComponent.h"

void CubeRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	renderer->DrawCube(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
