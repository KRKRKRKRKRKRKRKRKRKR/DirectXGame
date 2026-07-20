#pragma once
#include <DirectXMath.h>
#include "../../IComponent.h"
#include "../../../../Math/MathTypes.h"
#include "../../../../Math/TransformMath.h"
#include "../../../../Math/MatrixMath.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Math/JsonUtil.h"

class Renderer;

// GameObjectに付与するゲーム内カメラ。位置・向きは自分では持たず、オーナーのTransformから
// 導出する（DirectionalLightComponentと同じ方針）。オーナーのGameObject::tagを"MainCamera"に
// するとGameビュー用のメインカメラとして明示的に選ばれる。タグが無ければ、シーン内で最初に
// 見つかったCameraComponentが自動的に使われる（SceneBase::Render参照）
class CameraComponent : public IComponent {
public:
	float fov       = 45.0f;
	float nearClip   = 0.1f;
	float farClip    = 1000.0f;

	// オーナーのTransformにそのまま乗せると、CameraComponentを付けたオブジェクト自身の
	// メッシュの内側にカメラが来てしまい（例：Playerに直接付けた場合）、画面がメッシュの
	// 内部になって何も見えなくなることがある。このオフセットでオーナーから見た相対位置・
	// 向きをずらして調整できるようにする。既定値は{0,0,0}（従来通りオーナーの位置そのまま）
	Vector3 positionOffset = { 0.0f, 0.0f, 0.0f };
	Vector3 rotationOffset = { 0.0f, 0.0f, 0.0f };

	// オーナーのワールドTransformにpositionOffset/rotationOffsetを適用した、カメラの実際の
	// ワールド位置・向き。GetViewMatrixとDrawGizmoVisualization（Sceneビューでのカメラ可視化）の
	// 両方が「実際に見ている位置」を一致させるためにこれを共通で使う
	Transform GetEffectiveWorldTransform(const Transform& ownerWorldTransform) const {
		// positionOffsetはオーナーから見た相対方向（前後左右上下）で指定したいので、
		// オーナーの回転だけのアフィン行列でオフセットベクトルを回転させてからワールド位置に加算する
		Matrix4x4 ownerRotOnly = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, ownerWorldTransform.rotation, { 0.0f, 0.0f, 0.0f });
		Vector3 rotatedOffset = TransformMath::Transform(positionOffset, ownerRotOnly);

		Transform result;
		result.translation = ownerWorldTransform.translation + rotatedOffset;
		result.rotation = ownerWorldTransform.rotation + rotationOffset;
		result.scale = { 1.0f, 1.0f, 1.0f };
		return result;
	}

	// worldTransform.rotation/translationから、Camera::GetViewMatrix()と同じロジックでView行列を作る。
	// scale=1固定のアフィン行列を使うのは、GameObjectやその親が非一様スケールを持っていても
	// 視界が歪まないようにするため（Transform.scaleは意図的に無視する）
	Matrix4x4 GetViewMatrix(const Transform& worldTransform) const {
		Transform effective = GetEffectiveWorldTransform(worldTransform);
		Matrix4x4 m = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, effective.rotation, effective.translation);
		return MatrixMath::Inverse(m);
	}
	Matrix4x4 GetProjectionMatrix(float aspectRatio) const {
		return TransformMath::MakePerspectiveForMatrix(DirectX::XMConvertToRadians(fov), aspectRatio, nearClip, farClip);
	}

	void DrawImGui(const char* namePrefix) override;

	// Blenderのカメラオブジェクトを模したワイヤーフレーム可視化（本体の箱＋fov/aspectに応じた
	// レンズの四角錐＋上向きを示す屋根三角形）をワールド空間へ直接描く。カメラ自体にはメッシュが
	// 無く、Sceneビューで選択しても向き・画角・映る範囲が全く分からないままだったため追加した。
	// SceneBase::RenderがScene表示中（!useGameCamera）のみ呼ぶ（Game表示中は自分自身の視点を
	// 描いても仕方ないため）。ILightComponentとは異なりCameraComponentは1種類しかなく、
	// インターフェース越しの汎用ディスパッチにする必要が無いため、直接の非virtualメソッドにしてある
	void DrawGizmoVisualization(Renderer* renderer, const Transform& worldTransform,
		const Matrix4x4& view, const Matrix4x4& proj, float aspectRatio) const;

	void ToJson(nlohmann::json& out) const override {
		out["fov"] = fov;
		out["nearClip"] = nearClip;
		out["farClip"] = farClip;
		out["positionOffset"] = Vector3ToJson(positionOffset);
		out["rotationOffset"] = Vector3ToJson(rotationOffset);
	}
	void FromJson(const nlohmann::json& in) override {
		fov = in.value("fov", fov);
		nearClip = in.value("nearClip", nearClip);
		farClip = in.value("farClip", farClip);
		if (in.contains("positionOffset")) positionOffset = Vector3FromJson(in["positionOffset"]);
		if (in.contains("rotationOffset")) rotationOffset = Vector3FromJson(in["rotationOffset"]);
	}
};
