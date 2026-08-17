#include "HitEffect.h"
#include <random>
#include <algorithm>

namespace HitEffect {

namespace {
	float shakeStrength_ = 0.0f; // 残りのシェイク強さ（時間経過で0まで減衰する）
	float shakeDuration_ = 0.0f; // シェイク開始時の継続時間（減衰計算の基準に使う）
	float shakeRemaining_ = 0.0f;

	float hitStopRemaining_ = 0.0f;

	std::mt19937 rng_{ std::random_device{}() };
}

void RequestShake(float strength, float duration) {
	// 同フレームに複数ヒットが重なっても、より強い/長い効果を優先する（弱い方で上書きしない）
	if (strength > shakeStrength_) shakeStrength_ = strength;
	if (duration > shakeRemaining_) {
		shakeDuration_ = duration;
		shakeRemaining_ = duration;
	}
}

void RequestHitStop(float duration) {
	hitStopRemaining_ = (std::max)(hitStopRemaining_, duration);
}

Vector3 ConsumeShakeOffset(float realDeltaTime) {
	if (shakeRemaining_ <= 0.0f) return Vector3{ 0.0f, 0.0f, 0.0f };

	// 残り時間の割合に応じて強さを線形に減衰させる（終わり際ほど揺れが小さくなる）
	float ratio = (shakeDuration_ > 0.0f) ? (shakeRemaining_ / shakeDuration_) : 0.0f;
	float currentStrength = shakeStrength_ * ratio;

	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	Vector3 offset{ dist(rng_) * currentStrength, dist(rng_) * currentStrength, 0.0f };

	shakeRemaining_ -= realDeltaTime;
	if (shakeRemaining_ <= 0.0f) {
		shakeRemaining_ = 0.0f;
		shakeStrength_ = 0.0f;
	}
	return offset;
}

bool IsHitStopActive(float realDeltaTime) {
	if (hitStopRemaining_ <= 0.0f) return false;
	hitStopRemaining_ -= realDeltaTime;
	if (hitStopRemaining_ < 0.0f) hitStopRemaining_ = 0.0f;
	return true;
}

} // namespace HitEffect
