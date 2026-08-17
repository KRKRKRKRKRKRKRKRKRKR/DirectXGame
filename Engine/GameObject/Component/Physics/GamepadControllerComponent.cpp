#include "GamepadControllerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../InputDevice/InputDevice.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>
#include <algorithm>

void GamepadControllerComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (!enabled) return;
	if (!Input::IsGamepadConnected(controllerIndex)) return;

	float stickX = Input::GetGamepadLeftStickX(controllerIndex);
	float stickY = Input::GetGamepadLeftStickY(controllerIndex);
	if (stickX != 0.0f || stickY != 0.0f) {
		transform.translation.x += stickX * moveSpeed * deltaTime;
		transform.translation.z += stickY * moveSpeed * deltaTime;
	}
}

void GamepadControllerComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "ゲームパッド操作を有効化";
	std::string speedLabel  = std::string(namePrefix) + "移動速度";
	std::string indexLabel  = std::string(namePrefix) + "コントローラ番号";
	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::SliderFloat(speedLabel.c_str(), &moveSpeed, 0.0f, 20.0f);
	if (ImGui::SliderInt(indexLabel.c_str(), &controllerIndex, 0, InputDevice::kGamepadCount - 1)) {
		controllerIndex = std::clamp(controllerIndex, 0, InputDevice::kGamepadCount - 1);
	}

	bool connected = Input::IsGamepadConnected(controllerIndex);
	ImGui::TextColored(connected ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
		connected ? "接続中" : "未接続");
}

REGISTER_SIMPLE_COMPONENT(GamepadControllerComponent, "GamepadController", "ゲームパッド操作", "物理");
