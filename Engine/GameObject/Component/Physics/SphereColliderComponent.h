#pragma once
#include "ColliderComponentBase.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/Collision.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/JsonUtil.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include <algorithm>

// GameObjectに付与する球形の当たり判定。offsetはオーナーのtranslationからの
// ローカル相対位置、radiusはscale=1のときのワールド単位の半径。GetWorldSphereが
// オーナーのtransform.scaleを掛けて実際の判定半径にするため、敵のランダムサイズ
// （PlayScene::SpawnEnemyAtのsizeScale）にも当たり判定が追従する
// layer/isTriggerはColliderComponentBaseから継承
class SphereColliderComponent : public ColliderComponentBase {
public:
	Vector3 offset = { 0.0f, 0.0f, 0.0f };
	float   radius = 1.0f;

	Collision::Sphere GetWorldSphere(const Transform& ownerTransform) const {
		// PlayScene::SpawnEnemyAtが敵ごとにtransform.scaleへランダム倍率(sizeScaleMin~Max)を
		// 設定するため、見た目のサイズと当たり判定を一致させるにはscaleを反映する必要がある。
		// 球は等方スケール前提のため、非等方にドラッグされた場合でも判定が小さすぎないよう
		// 3軸のうち最大値を採用する
		float scale = (std::max)({ ownerTransform.scale.x, ownerTransform.scale.y, ownerTransform.scale.z });
		return { ownerTransform.translation + offset, radius * scale };
	}

	// 球を3枚の直交円（XY/YZ/XZ平面）のワイヤーフレームとして描画する
	void DrawWireframe(Renderer* renderer, const Transform& ownerTransform, const Vector4& color,
		const Matrix4x4& view, const Matrix4x4& proj) const override;

	Transform GetGizmoEditTransform(const Transform& ownerTransform) const override {
		Transform t;
		t.translation = ownerTransform.translation + offset;
		t.scale = { radius, radius, radius };
		return t;
	}
	void ApplyGizmoEditTransform(const Transform& ownerTransform, const Transform& edited) override {
		offset = edited.translation - ownerTransform.translation;
		radius = edited.scale.x; // Scaleギズモは等方的なドラッグを想定し、xの値を採用
	}

	void ToJson(nlohmann::json& out) const override {
		ColliderComponentBase::ToJson(out);
		out["offset"] = Vector3ToJson(offset);
		out["radius"] = radius;
	}
	void FromJson(const nlohmann::json& in) override {
		ColliderComponentBase::FromJson(in);
		if (in.contains("offset")) offset = Vector3FromJson(in["offset"]);
		radius = in.value("radius", radius);
	}
};
