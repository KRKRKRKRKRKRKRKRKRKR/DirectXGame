#include "SpriteRenderComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void SpriteRenderComponent::Draw(Renderer* renderer, const Transform& transform, float deltaTime) const {
	(void)deltaTime;
	if (is3D) {
		renderer->DrawSprite3D(transform, color, textureHandle, lighting, uvTransform, blendMode, blendStrength, alphaTest, alphaThreshold);
	} else {
		renderer->DrawSprite2D(transform, color, textureHandle, lighting, uvTransform, blendMode, blendStrength, alphaTest, alphaThreshold);
	}
}

void SpriteRenderComponent::DrawImGui(const char* namePrefix) {
	RenderComponentBase::DrawImGui(namePrefix);

	std::string headerLabel = std::string(namePrefix) + "UV変換";
	ImGui::Text("%s", headerLabel.c_str());

	std::string offsetLabel   = std::string(namePrefix) + "UVオフセット";
	std::string rotationLabel = std::string(namePrefix) + "UV回転";
	std::string scaleLabel    = std::string(namePrefix) + "UVスケール";
	ImGui::DragFloat2(offsetLabel.c_str(), &uvTransform.offset.x, 0.01f, -10.0f, 10.0f);
	ImGui::DragFloat(rotationLabel.c_str(), &uvTransform.rotation, 0.01f, -3.14f, 3.14f);
	ImGui::DragFloat2(scaleLabel.c_str(), &uvTransform.scale.x, 0.01f, 0.01f, 10.0f);
}
