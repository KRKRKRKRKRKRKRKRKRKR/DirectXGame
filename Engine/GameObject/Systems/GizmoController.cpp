#include "GizmoController.h"
#include "../GameObject.h"
#include "../Component/Physics/ColliderComponentBase.h"
#include "../../../Math/Collision.h"
#include "../../../Math/MatrixMath.h"
#include "../../../Math/TransformMath.h"
#include "../../Graphics/Renderer/Renderer.h"
#include <algorithm>
#include <cfloat>

Transform* GizmoController::GetGizmoTargetTransform(const std::vector<GameObject*>& targets) {
	if (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(targets.size())) {
		GameObject* obj = targets[gizmoTargetIndex_];
		if (editCollider_) {
			// Collider編集モード：ColliderComponentBaseを継承していればSphere/OBBを問わず見つかる
			// （dynamic_castベースのGetComponent<T>()のため。1オブジェクト1コライダー運用を想定）
			if (obj->GetComponent<ColliderComponentBase>()) {
				return &colliderGizmoScratch_;
			}
			return nullptr; // Colliderを持たないオブジェクトを選んでいる場合は何も編集しない
		}
		return &obj->GetTransform();
	}
	return nullptr;
}

void GizmoController::UpdatePicking(const std::vector<GameObject*>& targets, Renderer* renderer,
	const Matrix4x4& view, const Matrix4x4& proj) {
	bool leftPressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool triggered = leftPressed && !prevMouseLeftPressed_;
	prevMouseLeftPressed_ = leftPressed;

	if (!triggered) return;
	if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return; // ギズモ操作中/ホバー中は発火しない
	if (ImGui::GetIO().WantCaptureMouse) return;           // ImGuiパネル上のクリックは無視

	// スクリーン座標 → NDC → ワールド空間レイ
	ImVec2 mousePos = ImGui::GetMousePos();
	float width  = static_cast<float>(renderer->GetClientWidth());
	float height = static_cast<float>(renderer->GetClientHeight());
	float ndcX = (mousePos.x / width)  * 2.0f - 1.0f;
	float ndcY = 1.0f - (mousePos.y / height) * 2.0f;

	Matrix4x4 invViewProj = MatrixMath::Inverse(view * proj);
	Vector3 nearPoint = TransformMath::Transform({ ndcX, ndcY, 0.0f }, invViewProj);
	Vector3 farPoint  = TransformMath::Transform({ ndcX, ndcY, 1.0f }, invViewProj);

	Collision::Ray ray;
	ray.origin = nearPoint;
	ray.diff   = farPoint - nearPoint;

	// targets内の全オブジェクトをBounding Sphere（pickingRadiusHint * scaleの最大成分を
	// 半径とする）とみなし、最もt値が小さい（＝最も手前の）ものを選ぶ。
	// pickingRadiusHintはGameObjectごとの基準半径（scale=1のときの半径）。
	// excludeFromPicking=trueのオブジェクト（Floor等の極端に平たい形状）はレイキャスト対象から外す
	// （コンボボックスからの選択は引き続き可能）
	int   closestIndex = -1;
	float closestT = FLT_MAX;
	for (int i = 0; i < static_cast<int>(targets.size()); i++) {
		if (targets[i]->excludeFromPicking) continue;
		const Transform& t = targets[i]->GetTransform();
		float maxScale = (std::max)({ t.scale.x, t.scale.y, t.scale.z });
		float radius = targets[i]->pickingRadiusHint * maxScale;
		Collision::Sphere sphere{ t.translation, radius };
		float hitT;
		if (Collision::RaySphere(ray, sphere, hitT) && hitT < closestT) {
			closestT = hitT;
			closestIndex = i;
		}
	}

	if (closestIndex >= 0) {
		gizmoTargetIndex_ = closestIndex;
	}
	// 何にも当たらなかった場合は現在の選択状態を維持する
}

void GizmoController::UpdateGizmo(const std::vector<GameObject*>& targets, Renderer* renderer,
	const Matrix4x4& view, const Matrix4x4& proj) {
	Transform* target = GetGizmoTargetTransform(targets);
	if (!target) return;

	// Collider編集モードの場合、操作開始前に現在のoffset/radius(またはhalfSize)を
	// colliderGizmoScratch_へ反映する（ドラッグ中は再計算しない）
	GameObject* gizmoTargetObj = (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(targets.size()))
		? targets[gizmoTargetIndex_] : nullptr;
	if (editCollider_ && gizmoTargetObj && !ImGuizmo::IsUsing()) {
		if (auto* collider = gizmoTargetObj->GetComponent<ColliderComponentBase>()) {
			colliderGizmoScratch_ = collider->GetGizmoEditTransform(gizmoTargetObj->GetTransform());
		}
	}

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(0, 0, (float)renderer->GetClientWidth(), (float)renderer->GetClientHeight());

	Matrix4x4 world = TransformMath::MakeAffineMatrix(target->scale, target->rotation, target->translation);

	ImGuizmo::OPERATION operation = gizmoOperation_;
	// Collider編集中はRotate操作を無効化する（Sphere/AABBに回転の意味がないため）
	if (editCollider_ && operation == ImGuizmo::ROTATE) {
		operation = ImGuizmo::TRANSLATE;
	}

	if (ImGuizmo::Manipulate(&view._11, &proj._11, operation, ImGuizmo::WORLD, &world._11)) {
		float t[3], r[3], s[3];
		ImGuizmo::DecomposeMatrixToComponents(&world._11, t, r, s);
		target->translation = { t[0], t[1], t[2] };
		target->rotation    = {
			DirectX::XMConvertToRadians(r[0]),
			DirectX::XMConvertToRadians(r[1]),
			DirectX::XMConvertToRadians(r[2]) }; // ImGuizmoは度数法、Transform.rotationはラジアン
		target->scale        = { s[0], s[1], s[2] };

		// Collider編集中の場合、ワールド座標系のtranslation/scaleをoffset/radius(またはhalfSize)へ変換して書き戻す
		if (editCollider_ && gizmoTargetObj) {
			if (auto* collider = gizmoTargetObj->GetComponent<ColliderComponentBase>()) {
				collider->ApplyGizmoEditTransform(gizmoTargetObj->GetTransform(), *target);
			}
		}
	}
}

void GizmoController::DrawImGui(const std::vector<GameObject*>& targets) {
	// コンボの選択肢：0="None", 1..N=targets[0..N-1]の名前
	std::vector<const char*> comboNames;
	comboNames.push_back("None");
	for (auto* obj : targets) comboNames.push_back(obj->name.c_str());

	int currentCombo = gizmoTargetIndex_ + 1; // -1("None")+1=0, 0以上はそのまま+1

	if (ImGui::Combo("Target", &currentCombo, comboNames.data(), static_cast<int>(comboNames.size()))) {
		gizmoTargetIndex_ = currentCombo - 1; // 0("None")-1=-1
	}

	// Collider編集モード：選択中オブジェクトがCollider（Sphere/Box）を持つ場合のみ有効化できる。
	// オンの間、ギズモの対象はGameObject本体のTransformではなくColliderのオフセット/サイズになる
	bool hasCollider = false;
	if (gizmoTargetIndex_ >= 0 && gizmoTargetIndex_ < static_cast<int>(targets.size())) {
		hasCollider = targets[gizmoTargetIndex_]->GetComponent<ColliderComponentBase>() != nullptr;
	}
	if (!hasCollider) editCollider_ = false; // Colliderを持たないオブジェクト選択中は強制オフ
	if (!hasCollider) ImGui::BeginDisabled();
	ImGui::Checkbox("Edit Collider", &editCollider_);
	if (!hasCollider) ImGui::EndDisabled();

	// Collider編集中はRotateに意味がないためグレーアウトする（Sphere/AABBに回転の概念がない）
	bool disableRotate = editCollider_;

	if (ImGui::RadioButton("Translate", gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (disableRotate) ImGui::BeginDisabled();
	if (ImGui::RadioButton("Rotate", gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
	if (disableRotate) ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
}
