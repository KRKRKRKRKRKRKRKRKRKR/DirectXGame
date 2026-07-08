#include "SphereRenderComponent.h"

void SphereRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	renderer->DrawSphere(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
