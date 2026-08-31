#include "SphereColliderComponent.h"
#include "../../ComponentRegistry.h"
#include <cmath>

namespace {
	// 円1枚あたりの分割数。球1個で3平面ぶん（kWireSegments*3本）の線を消費するため、
	// Line::kMaxInstanceCountを圧迫しないよう見た目に支障が出ない範囲で抑えている
	constexpr int kWireSegments = 12;

	// 中心center、半径radiusの円を1枚、指定した2軸(axis0, axis1。0=x,1=y,2=z)平面上に
	// 線分で近似して描画する
	void DrawWireCircle(Renderer* renderer, const Vector3& center, float radius, int axis0, int axis1,
		const Vector4& color, const Matrix4x4& view, const Matrix4x4& proj) {
		float coords[3] = { center.x, center.y, center.z };
		auto pointAt = [&](float angle) {
			float c[3] = { coords[0], coords[1], coords[2] };
			c[axis0] += cosf(angle) * radius;
			c[axis1] += sinf(angle) * radius;
			return Vector3{ c[0], c[1], c[2] };
			};
		for (int i = 0; i < kWireSegments; i++) {
			float a0 = (float)i / kWireSegments * 2.0f * 3.14159265f;
			float a1 = (float)(i + 1) / kWireSegments * 2.0f * 3.14159265f;
			renderer->DrawLine(pointAt(a0), pointAt(a1), color, view, proj);
		}
	}
}

void SphereColliderComponent::DrawWireframe(Renderer* renderer, const Transform& ownerTransform,
	const Vector4& color, const Matrix4x4& view, const Matrix4x4& proj) const {
	Collision::Sphere sphere = GetWorldSphere(ownerTransform);
	DrawWireCircle(renderer, sphere.center, sphere.radius, 0, 1, color, view, proj); // XY
	DrawWireCircle(renderer, sphere.center, sphere.radius, 1, 2, color, view, proj); // YZ
	DrawWireCircle(renderer, sphere.center, sphere.radius, 0, 2, color, view, proj); // XZ
}

REGISTER_SIMPLE_COMPONENT(SphereColliderComponent, "SphereCollider", "球コライダー", "物理");
