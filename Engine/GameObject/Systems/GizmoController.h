#pragma once
#include "../../../Math/MathTypes.h"
#include "../../../Externals/imgui/imgui.h" // ImGuizmo.hがImDrawList等imgui型を前提にしており、先にインクルードする必要がある
#include "../../../Externals/ImGuizmo/src/ImGuizmo.h"
#include <vector>

class GameObject;
class Renderer;

// 3Dビュー上でのオブジェクト選択（マウスピッキング＋ImGuiコンボ）とImGuizmoによる
// ドラッグ編集（Transform本体、またはCollider編集モード時はオフセット/サイズ）を担当する。
// 特定のGameSceneの中身に依存しない汎用のエディタ機能のため、ColliderSystemと同じく
// Engine/GameObject/Systems/に置く。ColliderSystemと違いフレームをまたぐ選択状態を自分で持つ
class GizmoController {
public:
	// targets内の全オブジェクトをTransform.scaleから概算したBounding Sphereとみなし、
	// 左クリック位置から飛ばしたレイとの交差判定で最も手前のものを選択状態に反映する。
	// ImGuizmo操作中/ImGuiパネル操作中は発火しない（DrawImGuiのコンボ選択と共存する追加手段）
	void UpdatePicking(const std::vector<GameObject*>& targets, Renderer* renderer,
		const Matrix4x4& view, const Matrix4x4& proj);

	// DrawImGuiで選んだ1オブジェクトのTransform（Edit Collider中はそのCollider）を
	// ImGuizmo::Manipulateでドラッグ編集する
	void UpdateGizmo(const std::vector<GameObject*>& targets, Renderer* renderer,
		const Matrix4x4& view, const Matrix4x4& proj);

	// "Gizmo"ウィンドウの中身のうちTargetコンボ・Edit Collider・Translate/Rotate/Scaleを
	// 描画する。ImGui::Begin/Endは呼ばない（呼び出し側が既にウィンドウを開いている前提。
	// Play/Stopボタン等、他のUIと同じウィンドウに同居させるための「中身だけ描く」形）
	void DrawImGui(const std::vector<GameObject*>& targets);

	// 現在選択中のオブジェクトを返す（未選択、またはtargetsの範囲外ならnullptr）。
	// Delete機能等、選択中オブジェクトそのものが必要な呼び出し元向け
	GameObject* GetSelected(const std::vector<GameObject*>& targets) const {
		if (gizmoTargetIndex_ < 0 || gizmoTargetIndex_ >= static_cast<int>(targets.size())) return nullptr;
		return targets[gizmoTargetIndex_];
	}

	// オブジェクトの生成・削除・ロード等でtargetsの中身/順序が変わった直後に呼ぶ。
	// 古いインデックスが別のオブジェクトを指す/範囲外になるのを防ぐ
	void ResetSelection() {
		gizmoTargetIndex_ = -1;
		editCollider_ = false;
	}

private:
	Transform* GetGizmoTargetTransform(const std::vector<GameObject*>& targets);

	ImGuizmo::OPERATION gizmoOperation_ = ImGuizmo::TRANSLATE;

	// targets内で現在選択中のインデックス。-1は「何も選んでいない」
	int gizmoTargetIndex_ = -1;

	// "Edit Collider"チェックボックス。オンの間、ギズモの対象は選択中GameObjectのTransformではなく、
	// そのGameObjectが持つCollider（オフセット+サイズ）に切り替わる
	bool editCollider_ = false;

	// Collider編集を仲介する一時バッファ。translationはワールド座標に変換したコライダー中心、
	// scaleはSphereなら{radius,radius,radius}、Boxならhalfsize*2を格納する。rotationは使わない
	Transform colliderGizmoScratch_;

	// マウスピッキング：3Dビュー上で左クリックした瞬間を検知するための前フレーム状態。
	// InputDeviceは左クリックの「押されているか」のみでトリガー版を持たないため、ここで保持する
	bool prevMouseLeftPressed_ = false;
};
