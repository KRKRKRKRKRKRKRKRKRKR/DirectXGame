#pragma once
#include "MathTypes.h"
#include "../Externals/Json/json.hpp"

// Vector2/Vector3/Vector4とnlohmann::jsonの間の変換ヘルパー。各コンポーネントのToJson/FromJsonで
// 同じ2〜4行を繰り返し書かずに済むようにまとめている
inline nlohmann::json Vector2ToJson(const Vector2& v) { return { v.x, v.y }; }
inline Vector2 Vector2FromJson(const nlohmann::json& j) { return { j[0], j[1] }; }

inline nlohmann::json Vector3ToJson(const Vector3& v) { return { v.x, v.y, v.z }; }
inline Vector3 Vector3FromJson(const nlohmann::json& j) { return { j[0], j[1], j[2] }; }

inline nlohmann::json Vector4ToJson(const Vector4& v) { return { v.x, v.y, v.z, v.w }; }
inline Vector4 Vector4FromJson(const nlohmann::json& j) { return { j[0], j[1], j[2], j[3] }; }
