#include "ScreenRay.h"
#include "../../../Math/MatrixMath.h"
#include "../../../Math/TransformMath.h"
#include "../../Graphics/Renderer/Renderer.h"
#include "../../../Externals/imgui/imgui.h"

namespace ScreenRay {

Collision::Ray FromMouse(Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj) {
	ImVec2 mousePos = ImGui::GetMousePos();
	mousePos.x -= renderer->GetSceneViewportOffsetX();
	mousePos.y -= renderer->GetSceneViewportOffsetY();

	float width  = static_cast<float>(renderer->GetSceneViewportWidth());
	float height = static_cast<float>(renderer->GetSceneViewportHeight());
	float ndcX = (mousePos.x / width)  * 2.0f - 1.0f;
	float ndcY = 1.0f - (mousePos.y / height) * 2.0f;

	Matrix4x4 invViewProj = MatrixMath::Inverse(view * proj);
	Vector3 nearPoint = TransformMath::Transform({ ndcX, ndcY, 0.0f }, invViewProj);
	Vector3 farPoint  = TransformMath::Transform({ ndcX, ndcY, 1.0f }, invViewProj);

	Collision::Ray ray;
	ray.origin = nearPoint;
	ray.diff   = farPoint - nearPoint;
	return ray;
}

} // namespace ScreenRay
