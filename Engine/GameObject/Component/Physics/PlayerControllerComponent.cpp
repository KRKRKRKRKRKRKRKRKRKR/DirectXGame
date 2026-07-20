#include "PlayerControllerComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../InputDevice/InputDevice.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"

void PlayerControllerComponent::Update(float deltaTime, Transform& transform) {
	if (!enabled) return;

	Vector3 move{ 0.0f, 0.0f, 0.0f };
	if (Input::IsPressed(DIK_W)) move.z += 1.0f;
	if (Input::IsPressed(DIK_S)) move.z -= 1.0f;
	if (Input::IsPressed(DIK_D)) move.x += 1.0f;
	if (Input::IsPressed(DIK_A)) move.x -= 1.0f;

	// 斜め移動で速度が伸びないよう正規化してからスケールする
	if (move.x != 0.0f || move.z != 0.0f) {
		move = VectorMath::Normalize(move);
		transform.translation.x += move.x * moveSpeed * deltaTime;
		transform.translation.z += move.z * moveSpeed * deltaTime;
	}
}

void PlayerControllerComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "プレイヤー操作を有効化";
	std::string speedLabel  = std::string(namePrefix) + "移動速度";
	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	ImGui::SliderFloat(speedLabel.c_str(), &moveSpeed, 0.0f, 20.0f);
}

REGISTER_SIMPLE_COMPONENT(PlayerControllerComponent, "PlayerController", "プレイヤー操作", "物理");
