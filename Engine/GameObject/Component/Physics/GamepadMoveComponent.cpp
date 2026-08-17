#include "GamepadMoveComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../InputDevice/InputDevice.h"
#include "../../../../Math/TransformMath.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include <numbers>
#include <string>
#include <algorithm>

void GamepadMoveComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (!enabled) return;
	if (!Input::IsGamepadConnected(controllerIndex)) return;

	float stickX = Input::GetGamepadLeftStickX(controllerIndex);
	float stickY = Input::GetGamepadLeftStickY(controllerIndex);
	if (stickX == 0.0f && stickY == 0.0f) return;

	// 自身のyaw回転から前方向・右方向を求め、スティック入力をその基準で合成する
	// （右方向はyawに+90度した向きの基準ベクトルと同じ）
	float yaw = transform.rotation.y;
	Vector3 forward = TransformMath::EulerRadiansToDirection({ 0.0f, yaw, 0.0f });
	Vector3 right = TransformMath::EulerRadiansToDirection({ 0.0f, yaw + std::numbers::pi_v<float> / 2.0f, 0.0f });

	Vector3 move = forward * stickY + right * stickX;
	move.y = 0.0f;
	float lenSq = move.x * move.x + move.z * move.z;
	if (lenSq > 1e-6f) {
		move = VectorMath::Normalize(move);
		transform.translation.x += move.x * moveSpeed * deltaTime;
		transform.translation.z += move.z * moveSpeed * deltaTime;
	}
}

void GamepadMoveComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "ゲームパッド移動を有効化";
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

REGISTER_SIMPLE_COMPONENT(GamepadMoveComponent, "GamepadMove", "ゲームパッド移動(視点基準)", "物理");
