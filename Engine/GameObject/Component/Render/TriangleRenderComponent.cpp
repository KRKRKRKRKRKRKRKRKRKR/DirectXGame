#include "TriangleRenderComponent.h"
#include "../../ComponentRegistry.h"

void TriangleRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	renderer->DrawTriangle(transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}

REGISTER_SIMPLE_COMPONENT(TriangleRenderComponent, "TriangleRender", "三角形描画", "形状");
