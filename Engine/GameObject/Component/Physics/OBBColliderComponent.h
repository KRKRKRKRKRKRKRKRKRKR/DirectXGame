#pragma once
#include "ColliderComponentBase.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Collision.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../Graphics/Renderer/Renderer.h"

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

	Transform GetGizmoEditTransform(const Transform& ownerTransform) const override {
		Transform t;
		t.translation = ownerTransform.translation + offset;
		t.scale = { halfSize.x * 2.0f, halfSize.y * 2.0f, halfSize.z * 2.0f };
		return t;
	}
	void ApplyGizmoEditTransform(const Transform& ownerTransform, const Transform& edited) override {
		offset   = edited.translation - ownerTransform.translation;
		halfSize = { edited.scale.x * 0.5f, edited.scale.y * 0.5f, edited.scale.z * 0.5f };
	}

	void ToJson(nlohmann::json& out) const override {
		ColliderComponentBase::ToJson(out);
		out["offset"] = Vector3ToJson(offset);
		out["halfSize"] = Vector3ToJson(halfSize);
	}
	void FromJson(const nlohmann::json& in) override {
		ColliderComponentBase::FromJson(in);
		if (in.contains("offset")) offset = Vector3FromJson(in["offset"]);
		if (in.contains("halfSize")) halfSize = Vector3FromJson(in["halfSize"]);
	}
};
