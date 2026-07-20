#include "SphereRenderComponent.h"
#include "../../ComponentRegistry.h"

void SphereRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	renderer->DrawSphere(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}

REGISTER_SIMPLE_COMPONENT(SphereRenderComponent, "SphereRender", "球描画", "形状");
