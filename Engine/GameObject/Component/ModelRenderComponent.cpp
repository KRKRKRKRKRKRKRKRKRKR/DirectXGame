#include "ModelRenderComponent.h"

void ModelRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	if (hasAnimation) {
		renderer->UpdateModelAnimation(modelHandle, deltaTime);
	}
	renderer->DrawModel(modelHandle, transform, color, textureHandle, lighting, blendMode, blendStrength, alphaTest, alphaThreshold);
}
