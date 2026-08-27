#include "GizmoController.h"
#include "../GameObject.h"
#include "../Component/Physics/ColliderComponentBase.h"
#include "../Component/Render/RenderComponentBase.h"
#include "ScreenRay.h"
#include "../../../Math/Collision.h"
#include "../../../Math/MatrixMath.h"
#include "../../../Math/TransformMath.h"
#include "../../Graphics/Renderer/Renderer.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
// 実ウィンドウpx座標（ImGui::GetMousePos()等）を、Sceneビューの矩形オフセット分だけ
// 差し引いた「Sceneビュー内のpx座標」に変換する（逆方向はToScreenPos）。3D描画がウィンドウ
// 全体ではなくドック中央ノードだけに収まるようになったため、この変換が必須になった
ImVec2 ToSceneLocal(const ImVec2& screenPos, Renderer* renderer) {
	return ImVec2(screenPos.x - renderer->GetSceneViewportOffsetX(), screenPos.y - renderer->GetSceneViewportOffsetY());
}
ImVec2 ToScreenPos(const ImVec2& sceneLocalPos, Renderer* renderer) {
	return ImVec2(sceneLocalPos.x + renderer->GetSceneViewportOffsetX(), sceneLocalPos.y + renderer->GetSceneViewportOffsetY());
}
}

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

void GizmoController::ApplyDecomposedMatrix(const Matrix4x4& world, Transform* target, ImGuizmo::OPERATION operation) {
	float t[3], r[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&world._11, t, r, s);

	// operationに対応する成分だけを書き戻す。3つとも毎回上書きすると、回転済みオブジェクトを
	// TRANSLATEでワールド軸方向にドラッグしただけでも、行列→オイラー角再分解の数値誤差で
	// rotationが微小に変化し、その結果translationの意図しない軸まで動いて見えてしまうため
	if (operation == ImGuizmo::TRANSLATE) {
		target->translation = { t[0], t[1], t[2] };
	} else if (operation == ImGuizmo::ROTATE) {
		target->rotation = {
			DirectX::XMConvertToRadians(r[0]),
			DirectX::XMConvertToRadians(r[1]),
			DirectX::XMConvertToRadians(r[2]) }; // ImGuizmoは度数法、Transform.rotationはラジアン
	} else if (operation == ImGuizmo::SCALE) {
		target->scale = { s[0], s[1], s[2] };
	}
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

	// スクリーン座標 → NDC → ワールド空間レイ（ReflexPlayerComponent等のゲームロジック側の
	// クリック判定とも共有するため、ScreenRay::FromMouseに抽出済み）
	Collision::Ray ray = ScreenRay::FromMouse(renderer, view, proj);

	// targets内の全オブジェクトをBounding Sphere（pickingRadiusHint * scaleの最大成分を
	// 半径とする）とみなし、最もt値が小さい（＝最も手前の）ものを選ぶ。
	// pickingRadiusHintはGameObjectごとの基準半径（scale=1のときの半径）。
	// excludeFromPicking=trueのオブジェクト（Floor等の極端に平たい形状）はレイキャスト対象から外す
	// （コンボボックスからの選択は引き続き可能）
	int   closestIndex = -1;
	float closestT = FLT_MAX;
	for (int i = 0; i < static_cast<int>(targets.size()); i++) {
		if (targets[i]->excludeFromPicking) continue;
		Transform t = targets[i]->GetWorldTransform();
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
	} else {
		// 何もない空間をクリックした場合は選択を解除する（Unityの Scene ビューと同じ操作感）
		ResetSelection();
	}
}

void GizmoController::DrawEmptyObjectMarker(const Transform& t, Renderer* renderer,
	const Matrix4x4& view, const Matrix4x4& proj) {
	// RenderComponentBase/ColliderComponentBaseのどちらも持たない「見た目が一切ない」
	// GameObjectは、選択してもどこにあるか画面上で分からないため、Gizmoハンドルの土台として
	// 3軸に短い線を引いた十字マーカーを常に描画する（Colliderのワイヤーフレームと同じ発想）
	constexpr float kMarkerLength = 0.5f;
	constexpr Vector4 kMarkerColor = { 1.0f, 1.0f, 0.2f, 1.0f }; // 黄色
	Vector3 c = t.translation;
	renderer->DrawLine(c - Vector3{ kMarkerLength, 0, 0 }, c + Vector3{ kMarkerLength, 0, 0 }, kMarkerColor, view, proj);
	renderer->DrawLine(c - Vector3{ 0, kMarkerLength, 0 }, c + Vector3{ 0, kMarkerLength, 0 }, kMarkerColor, view, proj);
	renderer->DrawLine(c - Vector3{ 0, 0, kMarkerLength }, c + Vector3{ 0, 0, kMarkerLength }, kMarkerColor, view, proj);
}

void GizmoController::UpdateGizmo(const std::vector<GameObject*>& targets, Renderer* renderer,
	const Matrix4x4& view, const Matrix4x4& proj) {
	Transform* target = GetGizmoTargetTransform(targets);
	if (!target) return;

	GameObject* gizmoTargetObj = (selection3D_.targetIndex >= 0 && selection3D_.targetIndex < static_cast<int>(targets.size()))
		? targets[selection3D_.targetIndex] : nullptr;
	if (gizmoTargetObj && !gizmoTargetObj->GetComponent<RenderComponentBase>() && !gizmoTargetObj->GetComponent<ColliderComponentBase>()) {
		DrawEmptyObjectMarker(gizmoTargetObj->GetWorldTransform(), renderer, view, proj);
	}

	// Collider編集モードの場合、操作開始前に現在のoffset/radius(またはhalfSize)を
	// colliderGizmoScratch_へ反映する（ドラッグ中は再計算しない）
	if (editCollider_ && gizmoTargetObj && !ImGuizmo::IsUsing()) {
		if (auto* collider = gizmoTargetObj->GetComponent<ColliderComponentBase>()) {
			colliderGizmoScratch_ = collider->GetGizmoEditTransform(gizmoTargetObj->GetTransform());
		}
	}

	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(renderer->GetSceneViewportOffsetX(), renderer->GetSceneViewportOffsetY(),
		(float)renderer->GetSceneViewportWidth(), (float)renderer->GetSceneViewportHeight());

	// 親を持つ場合はworldを親込みのGetWorldMatrix()にすることで、ギズモハンドルが画面上の
	// 実際の位置に表示される（Collider編集中はスコープ外＝従来通りローカル基準のまま）
	GameObject* parent3D = (!editCollider_ && gizmoTargetObj) ? gizmoTargetObj->GetParent() : nullptr;
	Matrix4x4 world = (!editCollider_ && gizmoTargetObj)
		? gizmoTargetObj->GetWorldMatrix()
		: TransformMath::MakeAffineMatrix(target->scale, target->rotation, target->translation);

	ImGuizmo::OPERATION operation = gizmoOperation_;
	// Collider編集中はRotate操作を無効化する（Sphere/AABBに回転の意味がないため）
	if (editCollider_ && operation == ImGuizmo::ROTATE) {
		operation = ImGuizmo::TRANSLATE;
	}

	// 3D版と2D版(UpdateGizmo2D)は同じフレームで両方Manipulate()を呼ぶため、IDを分離しないと
	// ImGuizmoの内部状態（mbUsing等）が競合し、片方の操作がもう片方のTransformを壊す
	ImGuizmo::PushID("GizmoController3D");
	if (ImGuizmo::Manipulate(&view._11, &proj._11, operation, ImGuizmo::WORLD, &world._11)) {
		if (parent3D) {
			// ドラッグ後のワールド行列から親のワールド行列を除いてローカルへ戻す
			Matrix4x4 local = world * MatrixMath::Inverse(parent3D->GetWorldMatrix());
			ApplyDecomposedMatrix(local, target, operation);
		} else {
			ApplyDecomposedMatrix(world, target, operation);
		}

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
	// ドラッグ中（矩形選択の進行中）は、クリックの立ち上がり判定より先に継続処理する。
	// マウスを離すまでは毎フレームここでプレビュー描画・最終的な確定処理を行う
	if (isRectSelecting_) {
		UpdateRectSelect2D(targets2D, renderer);
		return;
	}

	if (!IsPickingTriggered(selection2D_, "GizmoController2D")) return;

	// Transform.translation/scaleはRenderer::kUiDesignWidth/Height基準のデザイン座標系（Sprite2Dの
	// 見た目と一致させるため）だが、ImGuiのマウス座標は実ウィンドウpx基準（かつSceneビューの
	// 矩形オフセット込み）なので、比較の前にSceneビュー内座標へ変換してからデザイン座標系へ変換する。
	// mousePosRaw自体は矩形選択の開始位置保存用に実スクリーンpxのまま残しておく
	ImVec2 mousePosRaw = ImGui::GetMousePos();
	ImVec2 mousePosSceneLocal = ToSceneLocal(mousePosRaw, renderer);
	float scaleX = Renderer::GetUiDesignWidth()  / static_cast<float>(renderer->GetSceneViewportWidth());
	float scaleY = Renderer::GetUiDesignHeight() / static_cast<float>(renderer->GetSceneViewportHeight());
	ImVec2 mousePos{ mousePosSceneLocal.x * scaleX, mousePosSceneLocal.y * scaleY };

	int hitIndex = -1;
	// 後ろから探すことで、重なっている場合は最後に描画される＝一番手前に見えるものを優先する
	for (int i = static_cast<int>(targets2D.size()) - 1; i >= 0; --i) {
		if (targets2D[i]->excludeFromPicking) continue;
		Transform t = targets2D[i]->GetWorldTransform();
		// Sprite頂点は-0.5〜0.5（中心原点）なので、translationはSpriteの中心。
		// 左端/右端/上端/下端はtranslation±scale/2で求める
		float left = t.translation.x - t.scale.x * 0.5f, top = t.translation.y - t.scale.y * 0.5f;
		float right = t.translation.x + t.scale.x * 0.5f, bottom = t.translation.y + t.scale.y * 0.5f;
		if (mousePos.x >= left && mousePos.x <= right && mousePos.y >= top && mousePos.y <= bottom) {
			hitIndex = i;
			break;
		}
	}

	if (hitIndex >= 0) {
		selection2D_.targetIndex = hitIndex;
		lastSelectedIs2D_ = true;
		multiSelected2D_.clear(); // 単一クリックで選び直したら矩形選択の結果は破棄する
	} else {
		// 何もない場所をクリックした＝矩形選択の開始。開始位置は実ウィンドウpxのまま保持する
		// （プレビュー描画がImGuiのフォアグラウンド描画リストで実px系を使うため）
		isRectSelecting_ = true;
		rectSelectStartPos_ = mousePosRaw;
	}
}

void GizmoController::UpdateRectSelect2D(const std::vector<GameObject*>& targets2D, Renderer* renderer) {
	ImVec2 currentPos = ImGui::GetMousePos();

	// プレビュー矩形を描画する（実ウィンドウpxそのまま。開始位置と現在位置のどちらが
	// 右下か分からないので min/max で正規化する）
	ImVec2 rectMin{ (std::min)(rectSelectStartPos_.x, currentPos.x), (std::min)(rectSelectStartPos_.y, currentPos.y) };
	ImVec2 rectMax{ (std::max)(rectSelectStartPos_.x, currentPos.x), (std::max)(rectSelectStartPos_.y, currentPos.y) };

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	constexpr ImU32 kRectFillColor   = IM_COL32(80, 140, 255, 40);
	constexpr ImU32 kRectBorderColor = IM_COL32(80, 140, 255, 220);
	drawList->AddRectFilled(rectMin, rectMax, kRectFillColor);
	drawList->AddRect(rectMin, rectMax, kRectBorderColor, 0.0f, 0, 1.5f);

	// マウスを離した瞬間に確定する。それ以外のフレームはプレビューだけして終わる
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;
	isRectSelecting_ = false;

	// 矩形（実ウィンドウpx）をSceneビュー内座標へ変換してから、デザイン座標系へ変換して
	// 各オブジェクトのAABBと交差判定する
	ImVec2 selMinLocal = ToSceneLocal(rectMin, renderer);
	ImVec2 selMaxLocal = ToSceneLocal(rectMax, renderer);
	float scaleX = Renderer::GetUiDesignWidth()  / static_cast<float>(renderer->GetSceneViewportWidth());
	float scaleY = Renderer::GetUiDesignHeight() / static_cast<float>(renderer->GetSceneViewportHeight());
	float selLeft   = selMinLocal.x * scaleX;
	float selTop    = selMinLocal.y * scaleY;
	float selRight  = selMaxLocal.x * scaleX;
	float selBottom = selMaxLocal.y * scaleY;

	multiSelected2D_.clear();
	for (auto* obj : targets2D) {
		if (obj->excludeFromPicking) continue;
		Transform t = obj->GetWorldTransform();
		// Sprite頂点は-0.5〜0.5（中心原点）なので、translationはSpriteの中心。
		// 左端/右端/上端/下端はtranslation±scale/2で求める
		float left = t.translation.x - t.scale.x * 0.5f, top = t.translation.y - t.scale.y * 0.5f;
		float right = t.translation.x + t.scale.x * 0.5f, bottom = t.translation.y + t.scale.y * 0.5f;
		// AABB同士の交差判定（矩形が完全に包含していなくても、一部でも重なれば選択対象にする。
		// PowerPoint等のドラッグ選択も同じく「かすっただけ」で選ばれる挙動のため）
		bool intersects = left <= selRight && right >= selLeft && top <= selBottom && bottom >= selTop;
		if (intersects) multiSelected2D_.push_back(obj);
	}

	if (!multiSelected2D_.empty()) {
		lastSelectedIs2D_ = true;
		// 単一選択（Gizmoでのドラッグ編集対象）は矩形内の最後の1つに合わせておく。
		// こうすることでGizmoハンドルが矩形内のどれかに表示され、複数選択後も
		// そのままドラッグ編集に移れる
		auto it = std::find(targets2D.begin(), targets2D.end(), multiSelected2D_.back());
		if (it != targets2D.end()) {
			selection2D_.targetIndex = static_cast<int>(it - targets2D.begin());
		}
	} else {
		// 矩形に何も入らなかった場合（ドラッグせず何もない場所をクリックしただけの場合を含む）は
		// 2D側の選択だけを解除する。3D版UpdatePickingは同じフレームで独立して動いており、
		// 「3Dオブジェクトはヒットしたが、その画面位置がたまたま2Dオブジェクトの矩形に
		// 入っていなかった」だけのクリックも普通にあるため、ここでResetSelection()を呼んで
		// 3D側の選択（selection3D_/lastSelectedIs2D_）まで巻き込んで消してしまうと、3Dビューで
		// オブジェクトをクリックして選択してもマウスを離した瞬間に選択解除される不具合になる
		selection2D_.targetIndex = -1;
		multiSelected2D_.clear();
		if (lastSelectedIs2D_) lastSelectedIs2D_ = false; // 2D側が「最後の選択」だった場合のみ、その主張を取り下げる
	}
}

void GizmoController::DrawSelectionRect2D(const Transform& t, Renderer* renderer) {
	// デザイン座標系（Sprite2Dの見た目と同じ基準）→Sceneビュー内の実pxへ変換してから、
	// Sceneビューの矩形オフセットを足して実スクリーンpxへ変換する（SnapAndDrawGuidesと同じ変換）
	float toRealX = static_cast<float>(renderer->GetSceneViewportWidth())  / Renderer::GetUiDesignWidth();
	float toRealY = static_cast<float>(renderer->GetSceneViewportHeight()) / Renderer::GetUiDesignHeight();

	// Sprite頂点は-0.5〜0.5（中心原点）のクアッドなので、実際の描画範囲はtranslationを中心に
	// ±scale/2広がる。translationを左上として扱うと右下にscale分ズレるため中心基準で計算する
	float halfW = t.scale.x * 0.5f;
	float halfH = t.scale.y * 0.5f;
	ImVec2 rectMin = ToScreenPos(ImVec2{ (t.translation.x - halfW) * toRealX, (t.translation.y - halfH) * toRealY }, renderer);
	ImVec2 rectMax = ToScreenPos(ImVec2{ (t.translation.x + halfW) * toRealX, (t.translation.y + halfH) * toRealY }, renderer);

	constexpr ImU32 kSelectionRectColor = IM_COL32(255, 220, 0, 220); // 黄色。透過テキストの見た目の範囲を明示する
	ImGui::GetForegroundDrawList()->AddRect(rectMin, rectMax, kSelectionRectColor, 0.0f, 0, 1.5f);
}

void GizmoController::UpdateGizmo2D(const std::vector<GameObject*>& targets2D, Renderer* renderer) {
	// 矩形選択で複数選ばれている場合は、そのすべてに範囲矩形を出す（Gizmoハンドル自体は
	// selection2D_.targetIndexの1つだけに表示される、既存の単一編集の挙動と共存させる）
	for (auto* obj : multiSelected2D_) {
		DrawSelectionRect2D(obj->GetWorldTransform(), renderer);
	}

	if (selection2D_.targetIndex < 0 || selection2D_.targetIndex >= static_cast<int>(targets2D.size())) return;
	GameObject* targetObj = targets2D[selection2D_.targetIndex];
	Transform* target = &targetObj->GetTransform();

	// 単一選択時（矩形選択の複数選択に含まれていない場合のみ、二重描画を避ける）
	if (std::find(multiSelected2D_.begin(), multiSelected2D_.end(), targetObj) == multiSelected2D_.end()) {
		DrawSelectionRect2D(targetObj->GetWorldTransform(), renderer);
	}

	ImGuizmo::SetOrthographic(true);
	// SetRectは実際に描画・マウス判定される画面領域なので、Sceneビューの実際の矩形を指定する
	ImGuizmo::SetRect(renderer->GetSceneViewportOffsetX(), renderer->GetSceneViewportOffsetY(),
		(float)renderer->GetSceneViewportWidth(), (float)renderer->GetSceneViewportHeight());

	// Renderer::DrawSprite2Dと全く同じ「world * 正射影(デザイン解像度)」空間で操作することで、
	// 画面上の見た目とギズモの位置・大きさを一致させる（3Dカメラのview/projは使わない）。
	// orthoをデザイン解像度にし、SetRectで実ウィンドウpxへマッピングさせることで、
	// target(デザイン座標系のtranslation/scale)と実際の描画位置がウィンドウサイズによらず一致する。
	// 親を持つ場合はworldを親込みのGetWorldMatrix()にすることで、ギズモハンドルが画面上の
	// 実際の位置に表示される
	Matrix4x4 identityView = MatrixMath::Identity();
	Matrix4x4 ortho = MatrixMath::MakeOrthographicMatrix(Renderer::GetUiDesignWidth(), Renderer::GetUiDesignHeight());
	GameObject* parent2D = targetObj->GetParent();
	Matrix4x4 world = targetObj->GetWorldMatrix();

	// 3D版(UpdateGizmo)と同じフレームで呼ばれるため、IDを分離してImGuizmoの内部状態を独立させる
	ImGuizmo::PushID("GizmoController2D");
	bool changed = ImGuizmo::Manipulate(&identityView._11, &ortho._11, gizmoOperation_, ImGuizmo::WORLD, &world._11);
	if (changed) {
		if (parent2D) {
			// ドラッグ後のワールド行列から親のワールド行列を除いてローカルへ戻す
			Matrix4x4 local = world * MatrixMath::Inverse(parent2D->GetWorldMatrix());
			ApplyDecomposedMatrix(local, target, gizmoOperation_);
		} else {
			ApplyDecomposedMatrix(world, target, gizmoOperation_);
		}
	}

	// TRANSLATE操作中（ドラッグ中）だけスマートガイドを判定・描画する。Scale/Rotate中は
	// 端/中心の一致という考え方自体が馴染まないため対象外にする
	if (gizmoOperation_ == ImGuizmo::TRANSLATE && ImGuizmo::IsUsing()) {
		SnapAndDrawGuides(target, targetObj, targets2D, renderer);
	}
	ImGuizmo::PopID();
}

void GizmoController::SnapAndDrawGuides(Transform* dragged, GameObject* draggedObj,
	const std::vector<GameObject*>& targets2D, Renderer* renderer) {
	// Sprite頂点は-0.5〜0.5（中心原点）のクアッドなので、translationはSpriteの中心。
	// 左端/右端はtranslation±scale/2で求める（translationを左上とすると右方向にscale分ズレる）
	// ドラッグ中オブジェクトの左/右/水平中心・上/下/垂直中心（デザイン座標系）
	float dLeft   = dragged->translation.x - dragged->scale.x * 0.5f;
	float dRight  = dragged->translation.x + dragged->scale.x * 0.5f;
	float dHCenter= dragged->translation.x;
	float dTop    = dragged->translation.y - dragged->scale.y * 0.5f;
	float dBottom = dragged->translation.y + dragged->scale.y * 0.5f;
	float dVCenter= dragged->translation.y;

	// 見つかった中で最も近い一致だけを採用する（複数候補があると線がちらついて見づらいため）
	bool  foundX = false, foundY = false;
	float bestXDist = kSnapThresholdPx, bestYDist = kSnapThresholdPx;
	float snapX = 0.0f, snapY = 0.0f;   // ガイド線を引くデザイン座標系のx/y位置
	float snapDeltaX = 0.0f, snapDeltaY = 0.0f; // dragged->translationへ加算する補正量

	auto considerX = [&](float draggedValue, float otherValue) {
		float dist = std::fabs(draggedValue - otherValue);
		if (dist < bestXDist) {
			bestXDist = dist;
			foundX = true;
			snapX = otherValue;
			snapDeltaX = otherValue - draggedValue;
		}
	};
	auto considerY = [&](float draggedValue, float otherValue) {
		float dist = std::fabs(draggedValue - otherValue);
		if (dist < bestYDist) {
			bestYDist = dist;
			foundY = true;
			snapY = otherValue;
			snapDeltaY = otherValue - draggedValue;
		}
	};

	for (auto* other : targets2D) {
		if (other == draggedObj) continue;
		const Transform& t = other->GetTransform();
		float oLeft    = t.translation.x - t.scale.x * 0.5f;
		float oRight   = t.translation.x + t.scale.x * 0.5f;
		float oHCenter = t.translation.x;
		float oTop     = t.translation.y - t.scale.y * 0.5f;
		float oBottom  = t.translation.y + t.scale.y * 0.5f;
		float oVCenter = t.translation.y;

		// 左/右端同士だけでなく「左端と右端」等の組み合わせも見る（PowerPointと同じく、
		// 一方の右端ともう一方の左端が接する配置も揃えたいケースがあるため）
		considerX(dLeft, oLeft);   considerX(dLeft, oRight);   considerX(dLeft, oHCenter);
		considerX(dRight, oLeft);  considerX(dRight, oRight);  considerX(dRight, oHCenter);
		considerX(dHCenter, oHCenter);

		considerY(dTop, oTop);       considerY(dTop, oBottom);       considerY(dTop, oVCenter);
		considerY(dBottom, oTop);    considerY(dBottom, oBottom);    considerY(dBottom, oVCenter);
		considerY(dVCenter, oVCenter);
	}

	if (foundX) dragged->translation.x += snapDeltaX;
	if (foundY) dragged->translation.y += snapDeltaY;

	if (!foundX && !foundY) return;

	// デザイン座標→Sceneビュー内実pxへ変換してからフォアグラウンド描画リストに直接線を引く
	// （UpdatePicking2Dの逆変換：こちらはデザイン→実pxなのでGetSceneViewportWidth/Heightを掛ける。
	// 最後に描画時にToScreenPosで実スクリーンpxへ変換する）
	float toRealX = static_cast<float>(renderer->GetSceneViewportWidth())  / Renderer::GetUiDesignWidth();
	float toRealY = static_cast<float>(renderer->GetSceneViewportHeight()) / Renderer::GetUiDesignHeight();

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	constexpr ImU32 kGuideColor = IM_COL32(255, 0, 90, 255); // ピンク〜赤の点線ガイド
	constexpr float kDashLen = 6.0f, kGapLen = 4.0f;

	auto drawDashedLine = [&](ImVec2 p0, ImVec2 p1) {
		ImVec2 diff{ p1.x - p0.x, p1.y - p0.y };
		float length = std::sqrt(diff.x * diff.x + diff.y * diff.y);
		if (length < 0.001f) return;
		ImVec2 dir{ diff.x / length, diff.y / length };
		float travelled = 0.0f;
		while (travelled < length) {
			float segEnd = (std::min)(travelled + kDashLen, length);
			ImVec2 a{ p0.x + dir.x * travelled, p0.y + dir.y * travelled };
			ImVec2 b{ p0.x + dir.x * segEnd,    p0.y + dir.y * segEnd };
			drawList->AddLine(a, b, kGuideColor, 1.5f);
			travelled = segEnd + kGapLen;
		}
	};

	// ガイド線はSceneビュー全体を縦/横に貫通させる（PowerPoint等と同じく、揃った軸を
	// 表示領域の端まで示す。ToScreenPosでSceneビューの矩形オフセットを足して実スクリーンpxにする）
	if (foundX) {
		float px = snapX * toRealX;
		drawDashedLine(ToScreenPos(ImVec2(px, 0.0f), renderer),
			ToScreenPos(ImVec2(px, static_cast<float>(renderer->GetSceneViewportHeight())), renderer));
	}
	if (foundY) {
		float py = snapY * toRealY;
		drawDashedLine(ToScreenPos(ImVec2(0.0f, py), renderer),
			ToScreenPos(ImVec2(static_cast<float>(renderer->GetSceneViewportWidth()), py), renderer));
	}
}

void GizmoController::UpdateContextMenu(Renderer* renderer) {
	bool hasSelection = selection3D_.targetIndex >= 0 || selection2D_.targetIndex >= 0 || !multiSelected2D_.empty();

	// IsMouseDragging(..., threshold)で「ボタンが下りてから閾値以上動いたか」を見て、動いていなければ
	// Look用の右ドラッグではなく単発クリックだったとみなす。WantCaptureMouseでImGuiパネル上のクリックは除外し、
	// ToSceneLocalでSceneビューの矩形内かどうかも確認する（ビュー外での右クリックには反応させない）
	if (hasSelection && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
		&& !ImGui::IsMouseDragging(ImGuiMouseButton_Right, 4.0f)
		&& !ImGui::GetIO().WantCaptureMouse) {
		ImVec2 local = ToSceneLocal(ImGui::GetMousePos(), renderer);
		bool insideViewport = local.x >= 0.0f && local.y >= 0.0f
			&& local.x <= static_cast<float>(renderer->GetSceneViewportWidth())
			&& local.y <= static_cast<float>(renderer->GetSceneViewportHeight());
		if (insideViewport) {
			ImGui::OpenPopup("GizmoOperationContextMenu");
		}
	}

	if (ImGui::BeginPopup("GizmoOperationContextMenu")) {
		// Collider編集中はRotateに意味がないため無効化する（DrawImGuiのラジオボタンと同じ制約）
		bool disableRotate = editCollider_;
		if (ImGui::MenuItem("移動", nullptr, gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
		if (disableRotate) ImGui::BeginDisabled();
		if (ImGui::MenuItem("回転", nullptr, gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
		if (disableRotate) ImGui::EndDisabled();
		if (ImGui::MenuItem("拡縮", nullptr, gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
		ImGui::EndPopup();
	}
}

void GizmoController::SetSelected(GameObject* obj, const std::vector<GameObject*>& targets) {
	for (size_t i = 0; i < targets.size(); i++) {
		if (targets[i] == obj) {
			selection3D_.targetIndex = static_cast<int>(i);
			lastSelectedIs2D_ = false;
			return;
		}
	}
}

void GizmoController::SetSelected2D(GameObject* obj, const std::vector<GameObject*>& targets2D) {
	for (size_t i = 0; i < targets2D.size(); i++) {
		if (targets2D[i] == obj) {
			selection2D_.targetIndex = static_cast<int>(i);
			lastSelectedIs2D_ = true;
			return;
		}
	}
}

void GizmoController::DrawImGui(const std::vector<GameObject*>& targets) {
	// Collider編集モード：選択中オブジェクトがCollider（Sphere/Box）を持つ場合のみ有効化できる。
	// オンの間、ギズモの対象はGameObject本体のTransformではなくColliderのオフセット/サイズになる
	bool hasCollider = false;
	if (selection3D_.targetIndex >= 0 && selection3D_.targetIndex < static_cast<int>(targets.size())) {
		hasCollider = targets[selection3D_.targetIndex]->GetComponent<ColliderComponentBase>() != nullptr;
	}
	if (!hasCollider) editCollider_ = false; // Colliderを持たないオブジェクト選択中は強制オフ
	if (!hasCollider) ImGui::BeginDisabled();
	ImGui::Checkbox("コライダーを編集", &editCollider_);
	if (!hasCollider) ImGui::EndDisabled();

	// Collider編集中はRotateに意味がないためグレーアウトする（Sphere/AABBに回転の概念がない）
	bool disableRotate = editCollider_;

	if (ImGui::RadioButton("移動", gizmoOperation_ == ImGuizmo::TRANSLATE)) gizmoOperation_ = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (disableRotate) ImGui::BeginDisabled();
	if (ImGui::RadioButton("回転", gizmoOperation_ == ImGuizmo::ROTATE)) gizmoOperation_ = ImGuizmo::ROTATE;
	if (disableRotate) ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::RadioButton("拡縮", gizmoOperation_ == ImGuizmo::SCALE)) gizmoOperation_ = ImGuizmo::SCALE;
}
