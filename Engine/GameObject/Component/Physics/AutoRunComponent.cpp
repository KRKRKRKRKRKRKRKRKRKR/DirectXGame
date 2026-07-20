#include "AutoRunComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../InputDevice/InputDevice.h"
#include "../../../../Externals/imgui/imgui.h"

void AutoRunComponent::Update(float deltaTime, Transform& transform) {
	if (!enabled) return;

	// 常時前進：入力に関係なく毎フレームZ+方向へ進む
	transform.translation.z += forwardSpeed * deltaTime;

	// 左右操作のみ：A/D（矢印キーも可）でX方向にだけ動かす。斜め移動が無いので正規化は不要
	if (Input::IsPressed(DIK_A) || Input::IsPressed(DIK_LEFT))  transform.translation.x -= turnSpeed * deltaTime;
	if (Input::IsPressed(DIK_D) || Input::IsPressed(DIK_RIGHT)) transform.translation.x += turnSpeed * deltaTime;
}

void AutoRunComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel  = std::string(namePrefix) + "自動走行を有効化";
	std::string forwardLabel = std::string(namePrefix) + "前進速度";
	std::string turnLabel    = std::string(namePrefix) + "左右移動速度";
	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::SliderFloat(forwardLabel.c_str(), &forwardSpeed, 0.0f, 30.0f);
	ImGui::SliderFloat(turnLabel.c_str(), &turnSpeed, 0.0f, 20.0f);
}

REGISTER_SIMPLE_COMPONENT(AutoRunComponent, "AutoRun", "自動走行", "物理");
