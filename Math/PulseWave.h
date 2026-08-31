#pragma once
#include "Easing.h"
#include <algorithm>
#include <cmath>

// 「一定周期でスケールが波紋のように広がりながら不透明度が下がって消える」演出の計算式。
// ClickHintMarkerComponent（クリック誘導マーカー）とReflexPlayerComponent::
// DrawPlanningVisualization（経路予約マーカー）が同じ計算式を独立に複製していたため、
// 計算部分だけをここへ切り出した。モデルの遅延ロードやDrawModel呼び出し・複数waypoint
// のループはそれぞれのコンポーネント固有の状態に依存するため、こちらには持ち込まない
struct PulseWaveParams {
	float minScale = 0.15f;
	float maxScale = 0.3f;
	float duration = 1.0f;
	int   waveCount = 3;
	Easing::Type easing = Easing::Type::kInOutSine;
};

struct PulseWaveSample {
	float scale;
	// 元の色のアルファ値に掛け合わせるための係数（0=完全透明、1=不透明）
	float alphaMultiplier;
};

// elapsed: 波紋アニメーション全体の基準経過時間（秒）
// wave: 何本目の波紋か（0-indexed）。waveCount本を等間隔に位相をずらして重ねて描くために使う
inline PulseWaveSample SamplePulseWave(const PulseWaveParams& params, float elapsed, int wave) {
	float duration = (std::max)(params.duration, 0.0f);
	int waves = (std::max)(params.waveCount, 1);

	float t = 0.0f;
	if (duration > 0.0f) {
		float phaseOffset = duration * (static_cast<float>(wave) / static_cast<float>(waves));
		float waveElapsed = std::fmod(elapsed + phaseOffset, duration);
		t = waveElapsed / duration;
	}

	float eased = Easing::Apply(params.easing, t);
	PulseWaveSample sample;
	sample.scale = params.minScale + (params.maxScale - params.minScale) * eased;
	sample.alphaMultiplier = 1.0f - eased;
	return sample;
}
