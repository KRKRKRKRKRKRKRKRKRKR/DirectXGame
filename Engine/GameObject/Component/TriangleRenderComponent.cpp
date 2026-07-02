#include "TriangleRenderComponent.h"

void TriangleRenderComponent::Draw(Renderer* renderer, const Transform& transform) const {
	renderer->DrawTriangle(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
