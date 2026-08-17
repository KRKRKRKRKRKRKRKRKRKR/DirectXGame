#include "RotatorComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../../Externals/imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>
#include <string>

namespace {
constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;

// (-π, π]に正規化する。TransformComponent::DrawImGuiがDragFloat3の表示レンジを
// -180〜180度にクランプしているため、正規化せずに加算し続けると180度を超えた瞬間に
// インスペクター側で-180度側へ切り詰められ、見た目上「右と左に反転し続ける」ように見える
float NormalizeAngle(float radians) {
	radians = std::fmod(radians + kPi, kTwoPi);
	if (radians < 0.0f) radians += kTwoPi;
	return radians - kPi;
}
}

void RotatorComponent::Randomize() {
	static std::mt19937 rng{ std::random_device{}() };
	float lo = std::min(randomSpeedMin, randomSpeedMax);
	float hi = std::max(randomSpeedMin, randomSpeedMax);
	std::uniform_real_distribution<float> magnitude(lo, hi);
	std::uniform_int_distribution<int> sign(0, 1);
	auto rollAxis = [&]() {
		float value = magnitude(rng);
		return sign(rng) == 0 ? value : -value;
	};
	speedX = rollAxis();
	speedY = rollAxis();
	speedZ = rollAxis();
}

void RotatorComponent::Update(float deltaTime, Transform& transform, const UpdateContext& ctx) {
	(void)ctx;
	if (!enabled) return;
	transform.rotation.x = NormalizeAngle(transform.rotation.x + speedX * kDegToRad * deltaTime);
	transform.rotation.y = NormalizeAngle(transform.rotation.y + speedY * kDegToRad * deltaTime);
	transform.rotation.z = NormalizeAngle(transform.rotation.z + speedZ * kDegToRad * deltaTime);
}

void RotatorComponent::DrawImGui(const char* namePrefix) {
	std::string enableLabel = std::string(namePrefix) + "回転を有効化";
	std::string speedXLabel = std::string(namePrefix) + "X軸回転速度(度/秒)";
	std::string speedYLabel = std::string(namePrefix) + "Y軸回転速度(度/秒)";
	std::string speedZLabel = std::string(namePrefix) + "Z軸回転速度(度/秒)";
	ImGui::Checkbox(enableLabel.c_str(), &enabled);
	// 符号がそのまま回転方向（正=+回転、負=-回転、0=停止）になるので、マイナス側にも
	// 振れるDragFloatをそのまま使う（別途「方向」だけを選ぶUIを設けない）
	ImGui::DragFloat(speedXLabel.c_str(), &speedX, 1.0f, -1000.0f, 1000.0f);
	ImGui::DragFloat(speedYLabel.c_str(), &speedY, 1.0f, -1000.0f, 1000.0f);
	ImGui::DragFloat(speedZLabel.c_str(), &speedZ, 1.0f, -1000.0f, 1000.0f);

	std::string randomizeLabel = std::string(namePrefix) + "スポーン時にランダム化";
	std::string minLabel = std::string(namePrefix) + "ランダム速度の下限(度/秒)";
	std::string maxLabel = std::string(namePrefix) + "ランダム速度の上限(度/秒)";
	std::string previewLabel = std::string(namePrefix) + "ランダム化をプレビュー";
	ImGui::Separator();
	ImGui::Checkbox(randomizeLabel.c_str(), &randomizeOnSpawn);
	if (randomizeOnSpawn) {
		ImGui::DragFloat(minLabel.c_str(), &randomSpeedMin, 1.0f, 0.0f, 1000.0f);
		ImGui::DragFloat(maxLabel.c_str(), &randomSpeedMax, 1.0f, 0.0f, 1000.0f);
		// テンプレート編集中に見た目を確認できるよう、エディタ上でも手動で1回分だけ試し引きできる
		// ようにする（実際のスポーン時の抽選はPlayScene::SpawnEnemyAt等が明示的にRandomize()を呼ぶ）
		if (ImGui::Button(previewLabel.c_str())) {
			Randomize();
		}
	}
}

REGISTER_SIMPLE_COMPONENT(RotatorComponent, "Rotator", "回転", "物理");
