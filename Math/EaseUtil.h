#pragma once
#include "MathTypes.h"
#include "VectorMath.h"

// 線形補間(Lerp)・イージングの汎用ユーティリティ。エンジンにこの種の関数が
// 1つも存在しなかったため新設した（VectorMath.hと同じ「namespace＋inline関数のみ」形式）。
// GraviTwistのカメラ90度回転Tween等、「開始値→終了値をちょうどduration秒で補間する」
// 演出全般で使う想定
namespace EaseUtil {

	// tを[0,1]にクランプする（呼び出し側でelapsed/durationを渡すと1.0fを超えうるため）
	inline float Clamp01(float t) {
		if (t < 0.0f) return 0.0f;
		if (t > 1.0f) return 1.0f;
		return t;
	}

	// スカラーの線形補間
	inline float Lerp(float a, float b, float t) {
		return a + (b - a) * t;
	}

	// Vector3の線形補間（成分ごとにLerpするだけ）
	inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t) {
		return VectorMath::Add(a, VectorMath::Scale(VectorMath::Subtract(b, a), t));
	}

	// 角度(ラジアン)の線形補間。単純にa+(b-a)*tするだけだと、呼び出し側が
	// 目標角を「+90度」等あらかじめ最短方向に組み立てて渡す前提になる
	// （2π境界をまたぐ最短経路の自動判定はここでは行わない、シンプルな実装）
	inline float LerpAngleRadians(float a, float b, float t) {
		return Lerp(a, b, t);
	}

	// EaseOutCubic: 開始直後は速く、終盤にかけて滑らかに減速する。
	// t=0で0、t=1で1になる（tは事前にClamp01しておくこと）
	inline float EaseOutCubic(float t) {
		float inv = 1.0f - t;
		return 1.0f - inv * inv * inv;
	}

	// EaseInOutQuad: 開始・終了の両方が滑らかに加減速する（参考実装GraviTwistBetaの
	// カメラ回転演出(Easing.cpp)が使っていたのと同じ種類のイージング）。
	// t=0で0、t=1で1になる（tは事前にClamp01しておくこと）
	inline float EaseInOutQuad(float t) {
		if (t < 0.5f) return 2.0f * t * t;
		float inv = -2.0f * t + 2.0f;
		return 1.0f - (inv * inv) * 0.5f;
	}
}
