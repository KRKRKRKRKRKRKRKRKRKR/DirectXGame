#include "GravityFlipComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../InputDevice/InputDevice.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>

// JSON保存キー・Inspector表示用の名前テーブル（CollisionLayerの
// kCollisionLayerNames/kCollisionLayerDisplayNamesと同じ形）。GravityDirectionの
// 消費者はこのコンポーネントだけなので、enum自体ではなくここに置く
static const char* kGravityDirectionNames[]        = { "Down", "Up", "Left", "Right" };
static const char* kGravityDirectionDisplayNames[] = { "下", "上", "左", "右" };
constexpr int kGravityDirectionCount = 4;

void GravityFlipComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (!enabled) return;

	// WASDは世界座標のコンパス方向に固定する（W→+Y面, S→-Y面, A→-X面, D→+X面）。
	// カメラ回転演出(CameraGravityTweenComponent)を廃止したため、画面が常に固定される前提に戻し、
	// 画面相対(カメラ相対)のマッピングは不要になった
	bool requestedChange = false;
	GravityDirection requested = direction;
	if (Input::IsTriggered(DIK_W) && direction != GravityDirection::kUp) {
		requested = GravityDirection::kUp; requestedChange = true;
	} else if (Input::IsTriggered(DIK_S) && direction != GravityDirection::kDown) {
		requested = GravityDirection::kDown; requestedChange = true;
	} else if (Input::IsTriggered(DIK_A) && direction != GravityDirection::kLeft) {
		requested = GravityDirection::kLeft; requestedChange = true;
	} else if (Input::IsTriggered(DIK_D) && direction != GravityDirection::kRight) {
		requested = GravityDirection::kRight; requestedChange = true;
	}

	if (requestedChange) {
		// isGrounded==true（床に立っている）→ 無条件で許可（残り回数を消費しない。壁を蹴って
		// 離陸する操作）。isGrounded==false（既に空中）→ directionChangesRemainingが
		// 残っているときだけ許可し1減らす
		bool allowed = false;
		if (isGrounded) {
			allowed = true;
		} else if (directionChangesRemaining > 0) {
			allowed = true;
			directionChangesRemaining--;
		}
		if (allowed) {
			direction = requested;
			isGrounded = false;
			velocity = 0.0f; // 新方向へ速度0から落下し直す
		}
	}

	velocity += gravity * deltaTime;
	if (velocity > maxFallSpeed) velocity = maxFallSpeed; // 終端速度でクランプ

	Vector3 fallDir = GravityDirectionToVector(direction);
	transform.translation = transform.translation + fallDir * (velocity * deltaTime);
}

void GravityFlipComponent::NotifyLanded() {
	landingEventId++;
	// 着地＝新しいジャンプの始まりなので、方向転換の残り回数をlandingDirectionChangeBonus分
	// 回復する（参考実装と同じく、敵撃破時の回復上限maxDirectionChangeより高い値になりうる）
	directionChangesRemaining = landingDirectionChangeBonus;
}

void GravityFlipComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel   = std::string(namePrefix) + "重力反転を有効化";
	std::string directionLabel = std::string(namePrefix) + "重力方向";
	std::string gravityLabel  = std::string(namePrefix) + "重力の強さ";
	std::string maxFallLabel  = std::string(namePrefix) + "落下速度の上限";
	std::string maxChangeLabel = std::string(namePrefix) + "敵撃破時の回復上限";
	std::string landingBonusLabel = std::string(namePrefix) + "着地時に回復する回数";
	std::string velocityLabel = std::string(namePrefix) + "現在の落下速度";
	std::string remainingLabel = std::string(namePrefix) + "残り方向転換回数";
	std::string groundedLabel = std::string(namePrefix) + "接地中";

	ImGui::Checkbox(enableLabel.c_str(), &enabled);

	int current = static_cast<int>(direction);
	if (ImGui::BeginCombo(directionLabel.c_str(), kGravityDirectionDisplayNames[current])) {
		for (int i = 0; i < kGravityDirectionCount; i++) {
			bool selected = (i == current);
			if (ImGui::Selectable(kGravityDirectionDisplayNames[i], selected))
				direction = static_cast<GravityDirection>(i);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::SliderFloat(gravityLabel.c_str(), &gravity, 0.0f, 60.0f);
	ImGui::SliderFloat(maxFallLabel.c_str(), &maxFallSpeed, 1.0f, 100.0f);
	ImGui::SliderInt(maxChangeLabel.c_str(), &maxDirectionChange, 0, 5);
	ImGui::SliderInt(landingBonusLabel.c_str(), &landingDirectionChangeBonus, 0, 5);

	// 実行時の一時状態はデバッグ表示のみ（直接編集はさせない）
	ImGui::BeginDisabled();
	ImGui::DragFloat(velocityLabel.c_str(), &velocity);
	ImGui::DragInt(remainingLabel.c_str(), &directionChangesRemaining);
	ImGui::Checkbox(groundedLabel.c_str(), &isGrounded);
	ImGui::EndDisabled();
}

void GravityFlipComponent::ToJson(nlohmann::json& out) const {
	out["enabled"] = enabled;
	out["direction"] = kGravityDirectionNames[static_cast<int>(direction)];
	out["gravity"] = gravity;
	out["maxFallSpeed"] = maxFallSpeed;
	out["maxDirectionChange"] = maxDirectionChange;
	out["landingDirectionChangeBonus"] = landingDirectionChangeBonus;
}

void GravityFlipComponent::FromJson(const nlohmann::json& in) {
	enabled = in.value("enabled", enabled);
	std::string directionName = in.value("direction", std::string(kGravityDirectionNames[static_cast<int>(direction)]));
	for (int i = 0; i < kGravityDirectionCount; i++) {
		if (directionName == kGravityDirectionNames[i]) { direction = static_cast<GravityDirection>(i); break; }
	}
	gravity = in.value("gravity", gravity);
	maxFallSpeed = in.value("maxFallSpeed", maxFallSpeed);
	maxDirectionChange = in.value("maxDirectionChange", maxDirectionChange);
	landingDirectionChangeBonus = in.value("landingDirectionChangeBonus", landingDirectionChangeBonus);
	// ロード直後は静止・着地済み・残り回数満タン(landingDirectionChangeBonus分)から再開する
	velocity = 0.0f;
	isGrounded = true;
	directionChangesRemaining = landingDirectionChangeBonus;
	landingEventId = 0;
}

REGISTER_SIMPLE_COMPONENT(GravityFlipComponent, "GravityFlip", "重力反転", "物理");
