#pragma once
#include "MathTypes.h" 
#include <cmath>

namespace MatrixMath {

    // 加算
    inline Matrix4x4 Add(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 result;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                result.m[i][j] = a.m[i][j] + b.m[i][j];
        return result;
    }

    // 減算
    inline Matrix4x4 Subtract(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 result;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                result.m[i][j] = a.m[i][j] - b.m[i][j];
        return result;
    }

    // 行列の積
    inline Matrix4x4 Multiply(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 result;
        DirectX::XMMATRIX ma = DirectX::XMLoadFloat4x4(&a);
        DirectX::XMMATRIX mb = DirectX::XMLoadFloat4x4(&b);
        DirectX::XMStoreFloat4x4(&result, ma * mb);
        return result;
    }

    // 逆行列
    inline Matrix4x4 Inverse(const Matrix4x4& mat) {
        Matrix4x4 result;
        DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&mat);
        DirectX::XMStoreFloat4x4(&result, DirectX::XMMatrixInverse(nullptr, m));
        return result;
    }

    // 転置行列
    inline Matrix4x4 Transpose(const Matrix4x4& mat) {
        Matrix4x4 result;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                result.m[i][j] = mat.m[j][i];
        return result;
    }

    // 単位行列
    inline Matrix4x4 Identity() {
        Matrix4x4 result = {};
        for (int i = 0; i < 4; i++)
            result.m[i][i] = 1.0f;
        return result;
    }

    // 正射影行列（スクリーンピクセル座標系: 左上原点）
    inline Matrix4x4 MakeOrthographicMatrix(float width, float height) {
        Matrix4x4 result;
        DirectX::XMStoreFloat4x4(&result,
            DirectX::XMMatrixOrthographicOffCenterLH(0.0f, width, height, 0.0f, 0.0f, 1.0f));
        return result;
    }
}

// グローバル演算子
inline Matrix4x4 operator+(const Matrix4x4& a, const Matrix4x4& b) {
    return MatrixMath::Add(a, b);
}
inline Matrix4x4 operator-(const Matrix4x4& a, const Matrix4x4& b) {
    return MatrixMath::Subtract(a, b);
}
inline Matrix4x4 operator*(const Matrix4x4& a, const Matrix4x4& b) {
    return MatrixMath::Multiply(a, b);
}

