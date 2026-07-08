#include "OBBColliderComponent.h"
#include "../../../../Math/TransformMath.h"
#include <DirectXMath.h>

Collision::OBB OBBColliderComponent::GetWorldOBB(const Transform& ownerTransform) const {
	// オーナーのrotationだけから3軸の向きを作る（scaleはhalfSize側で別途考慮する設計のため、
	// ここでは正規直交基底として扱う）
	DirectX::XMMATRIX rot = DirectX::XMMatrixRotationRollPitchYaw(
		ownerTransform.rotation.x, ownerTransform.rotation.y, ownerTransform.rotation.z);

	Collision::OBB obb;
	obb.center = ownerTransform.translation + offset;

	DirectX::XMVECTOR axisX = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), rot);
	DirectX::XMVECTOR axisY = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 1, 0, 0), rot);
	DirectX::XMVECTOR axisZ = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 0, 1, 0), rot);
	DirectX::XMStoreFloat3(&obb.Orientation[0], axisX);
	DirectX::XMStoreFloat3(&obb.Orientation[1], axisY);
	DirectX::XMStoreFloat3(&obb.Orientation[2], axisZ);

	// halfSizeはオーナーのscaleも反映する（Cubeが拡大縮小されればコライダーも追従する）
	obb.Size = {
		halfSize.x * ownerTransform.scale.x,
		halfSize.y * ownerTransform.scale.y,
		halfSize.z * ownerTransform.scale.z,
	};
	return obb;
}

void OBBColliderComponent::DrawWireframe(Renderer* renderer, const Transform& ownerTransform,
	const Vector4& color, const Matrix4x4& view, const Matrix4x4& proj) const {
	Collision::OBB obb = GetWorldOBB(ownerTransform);

	// OBBのローカル8頂点（±Size）をワールド空間へ変換して12本の辺を描く
	auto toWorld = [&](float sx, float sy, float sz) {
		Vector3 local = { obb.Size.x * sx, obb.Size.y * sy, obb.Size.z * sz };
		Vector3 world = obb.center
			+ Vector3{ obb.Orientation[0].x * local.x, obb.Orientation[0].y * local.x, obb.Orientation[0].z * local.x }
			+ Vector3{ obb.Orientation[1].x * local.y, obb.Orientation[1].y * local.y, obb.Orientation[1].z * local.y }
			+ Vector3{ obb.Orientation[2].x * local.z, obb.Orientation[2].y * local.z, obb.Orientation[2].z * local.z };
		return world;
		};
	Vector3 p[8] = {
		toWorld(-1,-1,-1), toWorld(+1,-1,-1), toWorld(+1,+1,-1), toWorld(-1,+1,-1),
		toWorld(-1,-1,+1), toWorld(+1,-1,+1), toWorld(+1,+1,+1), toWorld(-1,+1,+1),
	};
	static constexpr int kEdges[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0},
		{4,5}, {5,6}, {6,7}, {7,4},
		{0,4}, {1,5}, {2,6}, {3,7},
	};
	for (auto& e : kEdges) renderer->DrawLine(p[e[0]], p[e[1]], color, view, proj);
}
