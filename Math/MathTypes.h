#pragma once
#include <DirectXMath.h>
using Vector2 = DirectX::XMFLOAT2;
using Vector3 = DirectX::XMFLOAT3;
using Vector4 = DirectX::XMFLOAT4;
using Matrix4x4 = DirectX::XMFLOAT4X4;

struct Transform {
	Vector3 scale = {1.0f, 1.0f, 1.0f};
	Vector3 rotation = {0.0f, 0.0f, 0.0f};
	Vector3 translation = {0.0f, 0.0f, 0.0f};
};