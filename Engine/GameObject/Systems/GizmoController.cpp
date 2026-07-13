#include "GizmoController.h"
#include "../GameObject.h"
#include "../Component/Physics/ColliderComponentBase.h"
#include "../../../Math/Collision.h"
#include "../../../Math/MatrixMath.h"
#include "../../../Math/TransformMath.h"
#include "../../Graphics/Renderer/Renderer.h"
#include <algorithm>
#include <cfloat>

bool GizmoController::IsPickingTriggered(SelectionState& selection, const char* pushId) {
	bool leftPressed = ImGui::IsMouseDown(ImGuiMouseButton_Left);
	bool triggered = leftPressed && !selection.prevMouseLeftPressed;
	selection.prevMouseLeftPressed = leftPressed;

	if (!triggered) return false;

	// IsOver()/IsUsing()の無引数版はID非依存でグローバルな状態を見るため、もう片方のギズモの
	// 見た目に引きずられて誤ってブロックされることがある。自分のIDスコープ内でIsUsing()だけを
	// 見て、「今まさに自分のギズモをドラッグ中か」だけを判定する
	ImGuizmo::PushID(pushId);
	bool blockedByGizmo = ImGuizmo::IsUsing();
	ImGuizmo::PopID();
	if (blockedByGizmo) return false;

	if (ImGui::GetIO().WantCaptureMouse) return false; // ImGuiパネル上のクリックは無視
	return true;
}

void GizmoController::ApplyDecomposedMatrix(const Matrix4x4& world, Transform* target) {
	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&world._11, t, r, s);
	target->translation = { t[0], t[1], t[2] };
	target->rotation    = {
		DirectX::XMConvertToRadians(r[0]),
		DirectX::XMConvertToRadians(r[1]),
		DirectX::XMConvertToRadians(r[2]) }; // ImGuizmoは度数法、Transform.rotationはラジアン
	target->scale        = { s[0], s[1], s[2] };
}

Transform* GizmoController::GetGizmoTargetTransform(const std::vector<GameObject*>& targets) {
	if (selection3D_.targetIndex >= 0 && selection3D_.targetIndex < static_cast<int>(targets.size())) {
		GameObject* obj = targets[selection3D_.targetIndex];
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
	if (!IsPickingTriggered(selection3D_, "GizmoController3D")) return;

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
		selection3D_.targetIndex = closestIndex;
		lastSelectedIs2D_ = false;
	}
	// 何にも当たらなかった場合は現在の選択状態を維持する
}

void GizmoController::UpdateGizmo(const std::vector<GameObject*>& targets, Renderer* renderer,
	const Matrix4x4& view, const Matrix4x4& proj) {
	Transform* target = GetGizmoTargetTransform(targets);
	if (!target) return;

	// Collider編集モードの場合、操作開始前に現在のoffset/radius(またはhalfSize)を
	// colliderGizmoScratch_へ反映する（ドラッグ中は再計算しない）
	GameObject* gizmoTargetObj = (selection3D_.targetIndex >= 0 && selection3D_.targetIndex < static_cast<int>(targets.size()))
		? targets[selection3D_.targetIndex] : nullptr;
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

	// 3D版と2D版(UpdateGizmo2D)は同じフレームで両方Manipulate()を呼ぶため、IDを分離しないと
	// ImGuizmoの内部状態（mbUsing等）が競合し、片方の操作がもう片方のTransformを壊す
	ImGuizmo::PushID("GizmoController3D");
	if (ImGuizmo::Manipulate(&view._11, &proj._11, operation, ImGuizmo::WORLD, &world._11)) {
		ApplyDecomposedMatrix(world, target);

		// Collider編集中の場合、ワールド座標系のtranslation/scaleをoffset/radius(またはhalfSize)へ変換して書き戻す
		if (editCollider_ && gizmoTargetObj) {
			if (auto* collider = gizmoTargetObj->GetComponent<ColliderComponentBase>()) {
				collider->ApplyGizmoEditTransform(gizmoTargetObj->GetTransform(), *target);
			}
		}
	}
	ImGuizmo::PopID();
}

void GizmoController::UpdatePicking2D(const std::vector<GameObject*>& targets2D, Renderer* renderer) {
	if (!IsPickingTriggered(selection2D_, "GizmoController2D")) return;

	// Transform.translation/scaleはRenderer::kUiDesignWidth/Height基準のデザイン座標系（Sprite2Dの
	// 見た目と一致させるため）だが、ImGuiのマウス座標は実ウィンドウpx基準なので、比較の前に
	// マウス座標をデザイン座標系へ変換する
	ImVec2 mousePosRaw = ImGui::GetMousePos();
	float scaleX = Renderer::GetUiDesignWidth()  / static_cast<float>(renderer->GetClientWidth());
	float scaleY = Renderer::GetUiDesignHeight() / static_cast<float>(renderer->GetClientHeight());
	ImVec2 mousePos{ mousePosRaw.x * scaleX, mousePosRaw.y * scaleY };

	int hitIndex = -1;
	// 後ろから探すことで、重なっている場合は最後に描画される＝一番手前に見えるものを優先する
	for (int i = static_cast<int>(targets2D.size()) - 1; i >= 0; --i) {
		if (targets2D[i]->excludeFromPicking) continue;
		const Transform& t = targets2D[i]->GetTransform();
		float left = t.translation.x, top = t.translation.y;
		float right = left + t.scale.x, bottom = top + t.scale.y;
		if (mousePos.x >= left && mousePos.x <= right && mousePos.y >= top && mousePos.y <= bottom) {
			hitIndex = i;
			break;
		}
	}

	if (hitIndex >= 0) {
		selection2D_.targetIndex = hitIndex;
		lastSelectedIs2D_ = true;
	}
	// 何にも当たらなかった場合は現在の選択状態を維持する
}

void GizmoController::UpdateGizmo2D(const std::vector<GameObject*>& targets2D, Renderer* renderer) {
	if (selection2D_.targetIndex < 0 || selection2D_.targetIndex >= static_cast<int>(targets2D.size())) return;
	Transform* target = &targets2D[selection2D_.targetIndex]->GetTransform();

	ImGuizmo::SetOrthographic(true);
	// SetRectは実際に描画・マウス判定される画面領域なので実ウィンドウpxのまま指定する
	ImGuizmo::SetRect(0, 0, (float)renderer->GetClientWidth(), (float)renderer->GetClientHeight());

	// Renderer::DrawSprite2Dと全く同じ「world * 正射影(デザイン解像度)」空間で操作することで、
	// 画面上の見た目とギズモの位置・大きさを一致させる（3Dカメラのview/projは使わない）。
	// orthoをデザイン解像度にし、SetRectで実ウィンドウpxへマッピングさせることで、
	// target(デザイン座標系のtranslation/scale)と実際の描画位置がウィンドウサイズによらず一致する
	Matrix4x4 identityView = MatrixMath::Identity();
	Matrix4x4 ortho = MatrixMath::MakeOrthographicMatrix(Renderer::GetUiDesignWidth(), Renderer::GetUiDesignHeight());
	Matrix4x4 world = TransformMath::MakeAffineMatrix(target->scale, target->rotation, target->translation);

	// 3D版(UpdateGizmo)と同じフレームで呼ばれるため、IDを分離してImGuizmoの内部状態を独立させる
	ImGuizmo::PushID("GizmoController2D");
	if (ImGuizmo::Manipulate(&identityView._11, &ortho._11, gizmoOperation_, ImGuizmo::WORLD, &world._11)) {
		ApplyDecomposedMatrix(world, target);
	}
	ImGuizmo::PopID();
}

void GizmoController::DrawImGui2D(const std::vector<GameObject*>& targets2D) {
	std::vector<const char*> comboNames;
	comboNames.push_back("None");
	for (auto* obj : targets2D) comboNames.push_back(obj->name.c_str());

	int currentCombo = selection2D_.targetIndex + 1;
	if (ImGui::Combo("Target (2D)", &currentCombo, comboNames.data(), static_cast<int>(comboNames.size()))) {
		selection2D_.targetIndex = currentCombo - 1;
		lastSelectedIs2D_ = true;
	}
}

void GizmoController::DrawImGui(const std::vector<GameObject*>& targets) {
	// コンボの選択肢：0="None", 1..N=targets[0..N-1]の名前
	std::vector<const char*> comboNames;
	comboNames.push_back("None");
	for (auto* obj : targets) comboNames.push_back(obj->name.c_str());

	int currentCombo = selection3D_.targetIndex + 1; // -1("None")+1=0, 0以上はそのまま+1

	if (ImGui::Combo("Target", &currentCombo, comboNames.data(), static_cast<int>(comboNames.size()))) {
		selection3D_.targetIndex = currentCombo - 1; // 0("None")-1=-1
		lastSelectedIs2D_ = false;
	}

	// Collider編集モード：選択中オブジェクトがCollider（Sphere/Box）を持つ場合のみ有効化できる。
	// オンの間、ギズモの対象はGameObject本体のTransformではなくColliderのオフセット/サイズになる
	bool hasCollider = false;
	if (selection3D_.targetIndex >= 0 && selection3D_.targetIndex < static_cast<int>(targets.size())) {
		hasCollider = targets[selection3D_.targetIndex]->GetComponent<ColliderComponentBase>() != nullptr;
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
