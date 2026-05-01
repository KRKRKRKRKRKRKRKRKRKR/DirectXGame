#pragma once
#include "MathTypes.h"
#include <cmath>

namespace VectorMath {

    // =========================================
    // Vector2
    // =========================================

    // 加算
    inline Vector2 Add(const Vector2& a, const Vector2& b) {
        return { a.x + b.x, a.y + b.y };
    }

    // 減算
    inline Vector2 Subtract(const Vector2& a, const Vector2& b) {
        return { a.x - b.x, a.y - b.y };
    }

    // スカラー倍
    inline Vector2 Scale(const Vector2& v, float scalar) {
        return { v.x * scalar, v.y * scalar };
    }

    // 内積
    inline float Dot(const Vector2& a, const Vector2& b) {
        return a.x * b.x + a.y * b.y;
    }

    // 長さ
    inline float Length(const Vector2& v) {
        return std::sqrt(Dot(v, v));
    }

    // 正規化
    inline Vector2 Normalize(const Vector2& v) {
        float len = Length(v);
        if (len == 0.0f) return { 0.0f, 0.0f };
        return Scale(v, 1.0f / len);
    }

    // =========================================
    // Vector3
    // =========================================

    // 加算
    inline Vector3 Add(const Vector3& a, const Vector3& b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    // 減算
    inline Vector3 Subtract(const Vector3& a, const Vector3& b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    // スカラー倍
    inline Vector3 Scale(const Vector3& v, float scalar) {
        return { v.x * scalar, v.y * scalar, v.z * scalar };
    }

    // 内積
    inline float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // 外積
    inline Vector3 Cross(const Vector3& a, const Vector3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    // 長さ
    inline float Length(const Vector3& v) {
        return std::sqrt(Dot(v, v));
    }

    // 正規化
    inline Vector3 Normalize(const Vector3& v) {
        float len = Length(v);
        if (len == 0.0f) return { 0.0f, 0.0f, 0.0f };
        return Scale(v, 1.0f / len);
    }
}

// =========================================
// Vector2 グローバル演算子
// =========================================
inline Vector2 operator+(const Vector2& a, const Vector2& b) {
    return VectorMath::Add(a, b);
}
inline Vector2 operator-(const Vector2& a, const Vector2& b) {
    return VectorMath::Subtract(a, b);
}
inline Vector2 operator*(const Vector2& v, float scalar) {
    return VectorMath::Scale(v, scalar);
}
inline Vector2 operator*(float scalar, const Vector2& v) {
    return VectorMath::Scale(v, scalar);
}

// =========================================
// Vector3 グローバル演算子
// =========================================
inline Vector3 operator+(const Vector3& a, const Vector3& b) {
    return VectorMath::Add(a, b);
}
inline Vector3 operator-(const Vector3& a, const Vector3& b) {
    return VectorMath::Subtract(a, b);
}
inline Vector3 operator*(const Vector3& v, float scalar) {
    return VectorMath::Scale(v, scalar);
}
inline Vector3 operator*(float scalar, const Vector3& v) {
    return VectorMath::Scale(v, scalar);
}