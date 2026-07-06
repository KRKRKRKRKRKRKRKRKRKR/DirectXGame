#pragma once
#include "ColliderComponentBase.h"
#include "../../../Math/MathTypes.h"
#include "../../../Math/Collision.h"
#include "../../../Math/VectorMath.h"
#include "../../Graphics/Renderer/Renderer.h"

// GameObjectに付与する回転追従の直方体(OBB)当たり判定。offsetはオーナーのtranslationからの
// ローカル相対位置、halfSizeは中心からの半径ベクトル（片側の長さ）。回転はコライダー自身では
// 持たず、オーナーのTransform.rotationをそのまま使う（Cubeが回転すれば追従する）
// layer/isTriggerはColliderComponentBaseから継承
class OBBColliderComponent : public ColliderComponentBase {
public:
	Vector3 offset   = { 0.0f, 0.0f, 0.0f };
	Vector3 halfSize = { 0.5f, 0.5f, 0.5f };

	Collision::OBB GetWorldOBB(const Transform& ownerTransform) const;

	// OBBの8頂点から12本の辺をワイヤーフレームとして描画する
	void DrawWireframe(Renderer* renderer, const Transform& ownerTransform, const Vector4& color,
		const Matrix4x4& view, const Matrix4x4& proj) const;
};
