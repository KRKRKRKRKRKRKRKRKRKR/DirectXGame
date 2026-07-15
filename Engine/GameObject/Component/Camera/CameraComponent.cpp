#include "CameraComponent.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

void CameraComponent::DrawImGui(const char* namePrefix) {
	std::string fovLabel  = std::string(namePrefix) + "画角(FOV)";
	std::string nearLabel = std::string(namePrefix) + "ニアクリップ";
	std::string farLabel  = std::string(namePrefix) + "ファークリップ";

	ImGui::SliderFloat(fovLabel.c_str(), &fov, 1.0f, 179.0f);
	ImGui::DragFloat(nearLabel.c_str(), &nearClip, 0.01f, 0.001f, farClip - 0.01f);
	ImGui::DragFloat(farLabel.c_str(), &farClip, 1.0f, nearClip + 0.01f, 10000.0f);
}
