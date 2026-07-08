#include "TriangleRenderComponent.h"

void TriangleRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	renderer->DrawTriangle(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
