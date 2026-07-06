#pragma once
#include "ColliderComponentBase.h"
#include "../../../Math/MathTypes.h"
#include "../../../Math/Collision.h"
#include "../../../Math/VectorMath.h"
#include "../../Graphics/Renderer/Renderer.h"

// GameObjectに付与する球形の当たり判定。offsetはオーナーのtranslationからの
// ローカル相対位置、radiusはワールド単位の半径（scaleは考慮しない、絶対値を直接持つ）
// layer/isTriggerはColliderComponentBaseから継承
class SphereColliderComponent : public ColliderComponentBase {
public:
	Vector3 offset = { 0.0f, 0.0f, 0.0f };
	float   radius = 1.0f;

	Collision::Sphere GetWorldSphere(const Transform& ownerTransform) const {
		return { ownerTransform.translation + offset, radius };
	}

	// 球を3枚の直交円（XY/YZ/XZ平面）のワイヤーフレームとして描画する
	void DrawWireframe(Renderer* renderer, const Transform& ownerTransform, const Vector4& color,
		const Matrix4x4& view, const Matrix4x4& proj) const;
};
