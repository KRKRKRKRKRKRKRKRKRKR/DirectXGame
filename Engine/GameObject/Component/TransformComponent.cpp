#include "TransformComponent.h"
#include "../../../Externals/imgui/imgui.h"
#include <string>

void TransformComponent::DrawImGui(const char* namePrefix) {
	if (is2D) {
		std::string posLabel      = std::string(namePrefix) + "位置 (px)";
		std::string sizeLabel     = std::string(namePrefix) + "サイズ (px)";
		std::string rotationLabel = std::string(namePrefix) + "回転";
		ImGui::DragFloat2(posLabel.c_str(), &transform.translation.x, translationSpeed, translationMin, translationMax);
		ImGui::DragFloat2(sizeLabel.c_str(), &transform.scale.x, scaleSpeed, scaleMin, scaleMax);
		ImGui::DragFloat(rotationLabel.c_str(), &transform.rotation.z, rotationSpeed, -3.14f, 3.14f);
		return;
	}

	std::string scaleLabel       = std::string(namePrefix) + "スケール";
	std::string rotationLabel    = std::string(namePrefix) + "回転";
	std::string translationLabel = std::string(namePrefix) + "位置";
	ImGui::DragFloat3(scaleLabel.c_str(), &transform.scale.x, scaleSpeed, scaleMin, scaleMax);
	ImGui::DragFloat3(rotationLabel.c_str(), &transform.rotation.x, rotationSpeed, -3.14f, 3.14f);
	ImGui::DragFloat3(translationLabel.c_str(), &transform.translation.x, translationSpeed, translationMin, translationMax);
}
