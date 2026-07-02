#include "SphereRenderComponent.h"

void SphereRenderComponent::Draw(Renderer* renderer, const Transform& transform) const {
	renderer->DrawSphere(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
