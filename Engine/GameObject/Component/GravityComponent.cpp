#include "GravityComponent.h"
#include "../../../Externals/imgui/imgui.h"
#include <string>

void GravityComponent::Update(float deltaTime, Transform& transform) {
	if (!enabled) return;
	velocityY -= gravity * deltaTime;
	if (velocityY < -maxFallSpeed) velocityY = -maxFallSpeed; // 終端速度でクランプ
	transform.translation.y += velocityY * deltaTime;
}

void GravityComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel  = std::string(namePrefix) + " Enable Gravity";
	std::string gravityLabel = std::string(namePrefix) + " Gravity";
	std::string maxFallSpeedLabel = std::string(namePrefix) + " Max Fall Speed";
	std::string velocityLabel = std::string(namePrefix) + " Velocity Y";
	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::SliderFloat(gravityLabel.c_str(), &gravity, 0.0f, 30.0f);
	ImGui::SliderFloat(maxFallSpeedLabel.c_str(), &maxFallSpeed, 1.0f, 50.0f);

	// 現在の落下速度はデバッグ表示のみ（直接編集はさせない）
	ImGui::BeginDisabled();
	ImGui::DragFloat(velocityLabel.c_str(), &velocityY);
	ImGui::EndDisabled();
}
