#include "CameraComponent.h"
#include "../../ComponentRegistry.h"
#include "../../../Graphics/Renderer/Renderer.h"
#include "../../../../Math/VectorMath.h"
#include "../../../../Externals/imgui/imgui.h"
#include <string>
#include <cmath>

namespace {

// オイラー角（rotation）が表すローカルX/Y/Z軸を、ワールド空間の方向ベクトルとして取り出す。
// TransformMath::EulerRadiansToDirection()は+Z（forward）専用のため、+X（right）・+Y（up）も
// 欲しいカメラの可視化用にここだけ汎用版を用意する
Vector3 LocalAxisToWorldDirection(const Vector3& rotation, const DirectX::XMVECTOR& baseAxis) {
	DirectX::XMMATRIX rot = DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);
	DirectX::XMVECTOR dir = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(baseAxis, rot));
	Vector3 result;
	DirectX::XMStoreFloat3(&result, dir);
	return result;
}

} // namespace

void CameraComponent::DrawGizmoVisualization(Renderer* renderer, const Transform& worldTransform,
	const Matrix4x4& view, const Matrix4x4& proj, float aspectRatio) const {
	Vector3 right   = LocalAxisToWorldDirection(worldTransform.rotation, DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
	Vector3 up      = LocalAxisToWorldDirection(worldTransform.rotation, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
	Vector3 forward = LocalAxisToWorldDirection(worldTransform.rotation, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	Vector3 pos = worldTransform.translation;

	constexpr Vector4 kColor = { 0.9f, 0.9f, 0.9f, 1.0f };
	// farClipは数百〜数千になり得るため、可視化用のレンズ底面は固定距離に描く
	// （実際に映る範囲の"角度"はfov/aspectで正しく表現しつつ、絵としての大きさだけ固定する）
	constexpr float kGizmoDistance = 1.5f;

	float halfHeight = kGizmoDistance * std::tanf(DirectX::XMConvertToRadians(fov) * 0.5f);
	float halfWidth = halfHeight * aspectRatio;

	Vector3 lensCenter = pos + forward * kGizmoDistance;
	Vector3 tr = lensCenter + right * halfWidth + up * halfHeight;
	Vector3 tl = lensCenter - right * halfWidth + up * halfHeight;
	Vector3 br = lensCenter + right * halfWidth - up * halfHeight;
	Vector3 bl = lensCenter - right * halfWidth - up * halfHeight;

	// レンズ（四角錐）：カメラ位置(pos)を頂点として、画角の分だけ開いた四隅へ
	renderer->DrawLine(pos, tr, kColor, view, proj);
	renderer->DrawLine(pos, tl, kColor, view, proj);
	renderer->DrawLine(pos, br, kColor, view, proj);
	renderer->DrawLine(pos, bl, kColor, view, proj);

	// レンズ底面（実際に画面に映る範囲の縁）
	renderer->DrawLine(tr, tl, kColor, view, proj);
	renderer->DrawLine(tl, bl, kColor, view, proj);
	renderer->DrawLine(bl, br, kColor, view, proj);
	renderer->DrawLine(br, tr, kColor, view, proj);

	// 上向き三角形（Blenderのカメラと同じく、ロール方向＝どちらが上かを示す屋根）
	Vector3 roofApex = (tl + tr) * 0.5f + up * (halfHeight * 0.5f);
	renderer->DrawLine(tl, roofApex, kColor, view, proj);
	renderer->DrawLine(tr, roofApex, kColor, view, proj);

	// 本体（レンズの後ろにくっつく小さな箱。Blenderのカメラの"胴体"部分に相当）
	constexpr float kBodyHalfWidth = 0.2f;
	constexpr float kBodyHalfHeight = 0.15f;
	constexpr float kBodyDepth = 0.4f;
	Vector3 bodyFrontCenter = pos;
	Vector3 bodyBackCenter = pos - forward * kBodyDepth;
	Vector3 corners[8] = {
		bodyFrontCenter + right * kBodyHalfWidth + up * kBodyHalfHeight,
		bodyFrontCenter - right * kBodyHalfWidth + up * kBodyHalfHeight,
		bodyFrontCenter + right * kBodyHalfWidth - up * kBodyHalfHeight,
		bodyFrontCenter - right * kBodyHalfWidth - up * kBodyHalfHeight,
		bodyBackCenter + right * kBodyHalfWidth + up * kBodyHalfHeight,
		bodyBackCenter - right * kBodyHalfWidth + up * kBodyHalfHeight,
		bodyBackCenter + right * kBodyHalfWidth - up * kBodyHalfHeight,
		bodyBackCenter - right * kBodyHalfWidth - up * kBodyHalfHeight,
	};
	// front(0-3)同士・back(4-7)同士・front-back対応(0-4,1-5,2-6,3-7)の12辺
	constexpr int kEdges[12][2] = {
		{0,1}, {1,3}, {3,2}, {2,0},
		{4,5}, {5,7}, {7,6}, {6,4},
		{0,4}, {1,5}, {2,6}, {3,7},
	};
	for (auto& edge : kEdges) {
		renderer->DrawLine(corners[edge[0]], corners[edge[1]], kColor, view, proj);
	}
}

void CameraComponent::DrawImGui(const char* namePrefix) {
	std::string fovLabel  = std::string(namePrefix) + "画角(FOV)";
	std::string nearLabel = std::string(namePrefix) + "ニアクリップ";
	std::string farLabel  = std::string(namePrefix) + "ファークリップ";
	std::string posOffsetLabel = std::string(namePrefix) + "位置オフセット";
	std::string rotOffsetLabel = std::string(namePrefix) + "回転オフセット";

	ImGui::SliderFloat(fovLabel.c_str(), &fov, 1.0f, 179.0f);
	ImGui::DragFloat(nearLabel.c_str(), &nearClip, 0.01f, 0.001f, farClip - 0.01f);
	ImGui::DragFloat(farLabel.c_str(), &farClip, 1.0f, nearClip + 0.01f, 10000.0f);

	// オーナーに直接付けた場合にメッシュの内側へ埋め込まれてしまう問題への対処。
	// オーナーから見た相対位置・向きをここでずらせる（既定値0=従来通りオーナーの位置そのまま）
	ImGui::DragFloat3(posOffsetLabel.c_str(), &positionOffset.x, 0.1f);
	ImGui::DragFloat3(rotOffsetLabel.c_str(), &rotationOffset.x, 0.01f, -3.14f, 3.14f);
}

REGISTER_SIMPLE_COMPONENT(CameraComponent, "Camera", "カメラ", "カメラ");
