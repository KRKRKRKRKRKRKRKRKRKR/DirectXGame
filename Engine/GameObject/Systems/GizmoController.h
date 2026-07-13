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

	// UpdatePicking/UpdateGizmoの2D版。Sprite2D/Text等、TransformComponent::is2D==trueの
	// スクリーン空間オブジェクト（ピクセル座標、Transform.translationが画面px）向け。
	// 3D版と違い、ワールド空間へのレイキャストではなく画面px座標そのままでAABBヒット判定する
	// （回転は判定に反映しない簡易版）。ギズモ操作もRenderer::DrawSprite2Dと同じ
	// 「world * 正射影(0,0)-(width,height)」空間で行うことで見た目とギズモを一致させる
	void UpdatePicking2D(const std::vector<GameObject*>& targets2D, Renderer* renderer);
	void UpdateGizmo2D(const std::vector<GameObject*>& targets2D, Renderer* renderer);

	// "Gizmo"ウィンドウの中身のうちTargetコンボ・Edit Collider・Translate/Rotate/Scaleを
	// 描画する。ImGui::Begin/Endは呼ばない（呼び出し側が既にウィンドウを開いている前提。
	// Play/Stopボタン等、他のUIと同じウィンドウに同居させるための「中身だけ描く」形）
	void DrawImGui(const std::vector<GameObject*>& targets);

	// 2Dターゲット用のTargetコンボのみ描画する（Edit Collider/操作モードは3D側と共有）
	void DrawImGui2D(const std::vector<GameObject*>& targets2D);

	// 現在選択中のオブジェクトを返す（未選択、またはtargetsの範囲外ならnullptr）。
	// Delete機能等、選択中オブジェクトそのものが必要な呼び出し元向け
	GameObject* GetSelected(const std::vector<GameObject*>& targets) const {
		if (selection3D_.targetIndex < 0 || selection3D_.targetIndex >= static_cast<int>(targets.size())) return nullptr;
		return targets[selection3D_.targetIndex];
	}

	GameObject* GetSelected2D(const std::vector<GameObject*>& targets2D) const {
		if (selection2D_.targetIndex < 0 || selection2D_.targetIndex >= static_cast<int>(targets2D.size())) return nullptr;
		return targets2D[selection2D_.targetIndex];
	}

	// 3D/2Dはそれぞれ独立した選択状態を持つため、両方同時に選択中の場合は「最後に選んだ方」を
	// 優先したい。Objectsパネル等、1つだけ選んで詳細を出したい呼び出し元向け
	GameObject* GetSelectedPreferLatest(const std::vector<GameObject*>& targets, const std::vector<GameObject*>& targets2D) const {
		GameObject* sel3D = GetSelected(targets);
		GameObject* sel2D = GetSelected2D(targets2D);
		if (lastSelectedIs2D_ && sel2D) return sel2D;
		if (!lastSelectedIs2D_ && sel3D) return sel3D;
		return sel2D ? sel2D : sel3D;
	}

	// オブジェクトの生成・削除・ロード等でtargetsの中身/順序が変わった直後に呼ぶ。
	// 古いインデックスが別のオブジェクトを指す/範囲外になるのを防ぐ
	void ResetSelection() {
		selection3D_.targetIndex = -1;
		selection2D_.targetIndex = -1;
		editCollider_ = false;
		lastSelectedIs2D_ = false;
	}

private:
	Transform* GetGizmoTargetTransform(const std::vector<GameObject*>& targets);

	// 3D/2Dそれぞれが持つ「選択中インデックス＋前フレームのマウス押下状態」をまとめた1組。
	// 3D版と2D版は同じフレームで両方判定されるため、前フレーム状態を共有すると片方が読んだ
	// 時点でもう片方のエッジ検出が壊れる。そのため3D用/2D用で別々のインスタンスを持つ
	// （ロジック自体はUpdatePicking/UpdatePicking2Dに残したまま、状態の置き場所だけをまとめている）。
	// IsPickingTriggeredの引数型として使うため、メンバ関数宣言より前に定義しておく必要がある
	struct SelectionState {
		int  targetIndex = -1;         // targets内で現在選択中のインデックス。-1は「何も選んでいない」
		bool prevMouseLeftPressed = false; // 左クリックの立ち上がり（トリガー）検知用の前フレーム状態
	};

	// UpdatePicking/UpdatePicking2D共通のクリック立ち上がり検知。selectionのprevMouseLeftPressed
	// を更新したうえで、「今回クリックが立ち上がったか」かつ「pushId側のギズモを操作中でないか」
	// かつ「ImGuiパネル上でないか」をまとめて判定する。呼び出し元はtrueが返った場合のみ
	// 実際のピッキング処理（レイキャスト/AABB判定）を行う
	static bool IsPickingTriggered(SelectionState& selection, const char* pushId);

	// UpdateGizmo/UpdateGizmo2D共通の後始末：ImGuizmo::Manipulateがtrueを返した後の
	// DecomposeMatrixToComponents→Transformへの書き戻し（度数法→ラジアン変換込み）
	static void ApplyDecomposedMatrix(const Matrix4x4& world, Transform* target);

	ImGuizmo::OPERATION gizmoOperation_ = ImGuizmo::TRANSLATE;

	SelectionState selection3D_;
	SelectionState selection2D_;

	// 3D/2Dのうち最後に選択操作されたのはどちらか（GetSelectedPreferLatestが使う）。
	// falseの初期値は「3D優先」（既存の挙動を壊さないデフォルト）
	bool lastSelectedIs2D_ = false;

	// "Edit Collider"チェックボックス。オンの間、ギズモの対象は選択中GameObjectのTransformではなく、
	// そのGameObjectが持つCollider（オフセット+サイズ）に切り替わる
	bool editCollider_ = false;

	// Collider編集を仲介する一時バッファ。translationはワールド座標に変換したコライダー中心、
	// scaleはSphereなら{radius,radius,radius}、Boxならhalfsize*2を格納する。rotationは使わない
	Transform colliderGizmoScratch_;
};
