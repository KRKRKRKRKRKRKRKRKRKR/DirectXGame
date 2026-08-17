#include "GamepadCameraLookComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../InputDevice/InputDevice.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>
#include <algorithm>
#include <numbers>
#include <cmath>

namespace {
// yawをスティックを倒し続けたまま無限に加算し続けると、いずれfloatの有効桁数を超えて
// sin/cosの計算誤差が大きくなり、回転が突然反転して見える不具合が起きる。
// 毎フレーム[-pi, pi]へラップし直すことで値が際限なく増え続けないようにする
float WrapAngle(float angle) {
	constexpr float kTwoPi = std::numbers::pi_v<float> * 2.0f;
	angle = std::fmod(angle + std::numbers::pi_v<float>, kTwoPi);
	if (angle < 0.0f) angle += kTwoPi;
	return angle - std::numbers::pi_v<float>;
}
}

void GamepadCameraLookComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (!enabled) return;
	if (!Input::IsGamepadConnected(controllerIndex)) return;

	float stickX = Input::GetGamepadRightStickX(controllerIndex);
	float stickY = Input::GetGamepadRightStickY(controllerIndex);
	if (stickX == 0.0f && stickY == 0.0f) return;

	transform.rotation.y = WrapAngle(transform.rotation.y + stickX * sensitivity * deltaTime);
	float pitchDelta = stickY * sensitivity * deltaTime;
	transform.rotation.x += invertY ? -pitchDelta : pitchDelta;
	transform.rotation.x = std::clamp(transform.rotation.x, minPitch, maxPitch);
}

void GamepadCameraLookComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "ゲームパッド視点操作を有効化";
	std::string sensLabel   = std::string(namePrefix) + "感度";
	std::string invertLabel = std::string(namePrefix) + "上下反転";
	std::string pitchLabel  = std::string(namePrefix) + "ピッチ制限(rad)";
	std::string indexLabel  = std::string(namePrefix) + "コントローラ番号";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::SliderFloat(sensLabel.c_str(), &sensitivity, 0.1f, 10.0f);
	ImGui::Checkbox(invertLabel.c_str(), &invertY);
	ImGui::DragFloatRange2(pitchLabel.c_str(), &minPitch, &maxPitch, 0.01f, -1.5708f, 1.5708f);
	if (ImGui::SliderInt(indexLabel.c_str(), &controllerIndex, 0, InputDevice::kGamepadCount - 1)) {
		controllerIndex = std::clamp(controllerIndex, 0, InputDevice::kGamepadCount - 1);
	}

	bool connected = Input::IsGamepadConnected(controllerIndex);
	ImGui::TextColored(connected ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
		connected ? "接続中" : "未接続");
}

REGISTER_SIMPLE_COMPONENT(GamepadCameraLookComponent, "GamepadCameraLook", "ゲームパッド視点操作", "カメラ");
