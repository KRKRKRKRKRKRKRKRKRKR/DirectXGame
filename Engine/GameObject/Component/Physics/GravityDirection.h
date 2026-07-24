#pragma once
#include "../../../../Math/MathTypes.h"

// 重力の向き。GraviTwistの仕様「上下左右いずれかの壁が"床"」を、斜めを許さない
// 4値のみのenumとして表現する。CollisionLayerと同じ「固定enum」の考え方
// （自由なVector3にすると単位ベクトルか・軸に揃っているかを毎回検証する必要が出るため、
// コンパイラに状態の範囲を保証させたい）
enum class GravityDirection {
	kDown,
	kUp,
	kLeft,
	kRight,
};

// 重力方向 → 実際にオブジェクトが落ちていく単位ベクトル。
// GravityFlipComponentとColliderSystemの両方から参照するためヘッダのinline関数にする
inline Vector3 GravityDirectionToVector(GravityDirection direction) {
	switch (direction) {
		case GravityDirection::kDown:  return { 0.0f, -1.0f, 0.0f };
		case GravityDirection::kUp:    return { 0.0f,  1.0f, 0.0f };
		case GravityDirection::kLeft:  return { -1.0f, 0.0f, 0.0f };
		case GravityDirection::kRight: return { 1.0f,  0.0f, 0.0f };
	}
	return { 0.0f, -1.0f, 0.0f };
}
