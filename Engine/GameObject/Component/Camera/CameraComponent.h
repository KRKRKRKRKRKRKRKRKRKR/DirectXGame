#pragma once
#include <DirectXMath.h>
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/TransformMath.h"
#include "../../../../Math/MatrixMath.h"
#include "../../../../Math/JsonUtil.h"

// GameObjectに付与するゲーム内カメラ。位置・向きは自分では持たず、オーナーのTransformから
// 導出する（DirectionalLightComponentと同じ方針）。シーン内で最初に見つかったCameraComponentが
// Gameビュー用のメインカメラとして自動的に使われる（SceneBase::Render参照）
class CameraComponent : public IComponent {
public:
	float fov       = 45.0f;
	float nearClip   = 0.1f;
	float farClip    = 1000.0f;

	// worldTransform.rotation/translationから、Camera::GetViewMatrix()と同じロジックでView行列を作る。
	// scale=1固定のアフィン行列を使うのは、GameObjectやその親が非一様スケールを持っていても
	// 視界が歪まないようにするため（Transform.scaleは意図的に無視する）
	Matrix4x4 GetViewMatrix(const Transform& worldTransform) const {
		Matrix4x4 m = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, worldTransform.rotation, worldTransform.translation);
		return MatrixMath::Inverse(m);
	}
	Matrix4x4 GetProjectionMatrix(float aspectRatio) const {
		return TransformMath::MakePerspectiveForMatrix(DirectX::XMConvertToRadians(fov), aspectRatio, nearClip, farClip);
	}

	void DrawImGui(const char* namePrefix) override;

	void ToJson(nlohmann::json& out) const override {
		out["fov"] = fov;
		out["nearClip"] = nearClip;
		out["farClip"] = farClip;
	}
	void FromJson(const nlohmann::json& in) override {
		fov = in.value("fov", fov);
		nearClip = in.value("nearClip", nearClip);
		farClip = in.value("farClip", farClip);
	}
};
