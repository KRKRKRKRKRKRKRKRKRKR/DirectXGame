#include "MirrorComponent.h"
#include "../../../../Math/TransformMath.h"
#include "../../../../Math/VectorMath.h"
#include "../../../Graphics/Renderer/Renderer.h"

Collision::Plane MirrorComponent::ComputePlane(const Transform& ownerTransform) const {
	// Camera::HandleInputと同じ「回転のみのアフィン行列で方向ベクトルを変換する」パターン
	Matrix4x4 rotationOnly = TransformMath::MakeAffineMatrix({ 1.0f, 1.0f, 1.0f }, ownerTransform.rotation, { 0.0f, 0.0f, 0.0f });
	Vector3 normal = TransformMath::Transform({ 0.0f, 0.0f, 1.0f }, rotationOnly);
	float distance = VectorMath::Dot(normal, ownerTransform.translation);
	return Collision::Plane{ normal, distance };
}

void MirrorComponent::Draw(Renderer* renderer, const Transform& ownerTransform) const {
	render_->textureHandle = renderer->GetMirrorTextureHandle();
	render_->Draw(renderer, ownerTransform, 0.0f); // 鏡面自体はアニメーションしないためdeltaTime不要
}
