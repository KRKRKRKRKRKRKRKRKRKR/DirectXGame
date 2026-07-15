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
	// 「world * 正射影(0,0)-(width,height)」空間で行うことで見た目とギズモを一致させる。
	// 何もない場所を左クリックしてドラッグした場合は矩形選択（ラバーバンド選択）を開始し、
	// 離した時点で矩形と交差する全オブジェクトをmultiSelected2D_へ登録する（小さいオブジェクトが
	// クリックだけでは当てづらい問題への対処。既存の単一クリック選択とは共存する）
	void UpdatePicking2D(const std::vector<GameObject*>& targets2D, Renderer* renderer);

	// ドラッグ中はPowerPoint等の「スマートガイド」と同じく、他の2Dオブジェクトと端/中心が
	// 一定距離まで近づくと自動的にスナップし、一致した軸に赤い点線ガイドを描画する
	// （SnapAndDrawGuides参照）
	void UpdateGizmo2D(const std::vector<GameObject*>& targets2D, Renderer* renderer);

	// "Gizmo"ウィンドウの中身のうちEdit Collider・Translate/Rotate/Scaleを描画する。
	// オブジェクト選択自体はHierarchyパネル（SceneBase::DrawHierarchy）に一本化したため、
	// ここでは選択済みのtargets[selection3D_.targetIndex]がColliderを持つかの判定にのみ使う。
	// ImGui::Begin/Endは呼ばない（呼び出し側が既にウィンドウを開いている前提。Play/Stopボタン等、
	// 他のUIと同じウィンドウに同居させるための「中身だけ描く」形）
	void DrawImGui(const std::vector<GameObject*>& targets);

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

	// Hierarchyパネルのクリック選択用。targets内でobjを探し、見つかればそのインデックスを
	// 選択状態（selection3D_/selection2D_）に反映する。見つからなければ何もしない
	void SetSelected(GameObject* obj, const std::vector<GameObject*>& targets);
	void SetSelected2D(GameObject* obj, const std::vector<GameObject*>& targets2D);

	// 3D/2Dはそれぞれ独立した選択状態を持つため、両方同時に選択中の場合は「最後に選んだ方」を
	// 優先したい。Objectsパネル等、1つだけ選んで詳細を出したい呼び出し元向け
	GameObject* GetSelectedPreferLatest(const std::vector<GameObject*>& targets, const std::vector<GameObject*>& targets2D) const {
		GameObject* sel3D = GetSelected(targets);
		GameObject* sel2D = GetSelected2D(targets2D);
		if (lastSelectedIs2D_ && sel2D) return sel2D;
		if (!lastSelectedIs2D_ && sel3D) return sel3D;
		return sel2D ? sel2D : sel3D;
	}

	// 矩形選択（ラバーバンド選択）で選ばれている2Dオブジェクトの一覧。空なら矩形選択は
	// 使われていない状態。Delete Selected等、複数まとめて操作したい呼び出し元向け
	const std::vector<GameObject*>& GetMultiSelected2D() const { return multiSelected2D_; }

	// オブジェクトの生成・削除・ロード等でtargetsの中身/順序が変わった直後に呼ぶ。
	// 古いインデックスが別のオブジェクトを指す/範囲外になるのを防ぐ
	void ResetSelection() {
		selection3D_.targetIndex = -1;
		selection2D_.targetIndex = -1;
		editCollider_ = false;
		lastSelectedIs2D_ = false;
		multiSelected2D_.clear();
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

	// PowerPoint/Keynote等の「スマートガイド」相当。draggedをtargets2D内の他オブジェクトと比較し、
	// 左端/右端/水平中心・上端/下端/垂直中心のうちkSnapThresholdPx以内で一致する軸があれば
	// dragged->translationをスナップしたうえで、一致した軸に赤い点線ガイドをImGuiのフォアグラウンド
	// 描画リストへ直接描画する。デザイン座標系（Renderer::kUiDesignWidth/Height基準）で計算し、
	// 描画直前だけ実ウィンドウpxへ変換する（UpdatePicking2Dと同じ理由）
	void SnapAndDrawGuides(Transform* dragged, GameObject* draggedObj,
		const std::vector<GameObject*>& targets2D, Renderer* renderer);

	// スナップが発動する許容距離（デザイン座標系のpx単位）
	static constexpr float kSnapThresholdPx = 6.0f;

	// 矩形選択のドラッグ開始判定・進行中のプレビュー描画・確定時のヒット判定をまとめる。
	// UpdatePicking2Dから、既存の「何かをクリックしたら単一選択」の判定に加えて呼ばれる
	void UpdateRectSelect2D(const std::vector<GameObject*>& targets2D, Renderer* renderer);

	// 選択中の2DオブジェクトのTransform（translation〜translation+scale、デザイン座標系）を
	// 実ウィンドウpxへ変換して黄色い矩形で縁取る。テキストは背景が透過しているため見た目上の
	// クアッド全体がどこまで広がっているか分かりづらく、選択中は常にこの矩形で範囲を明示する
	void DrawSelectionRect2D(const Transform& t, Renderer* renderer);

	// RenderComponentBase/ColliderComponentBaseのどちらも持たない「見た目が一切ない」
	// GameObject（Create Emptyで作った直後等）を選択中、その位置に黄色い十字マーカーを描画する。
	// 見た目がないと選択後もどこにあるか画面上で分からずGizmoハンドルも見失うため
	void DrawEmptyObjectMarker(const Transform& t, Renderer* renderer, const Matrix4x4& view, const Matrix4x4& proj);

	ImGuizmo::OPERATION gizmoOperation_ = ImGuizmo::TRANSLATE;

	SelectionState selection3D_;
	SelectionState selection2D_;

	// 矩形選択で選ばれている2Dオブジェクトの一覧（GetMultiSelected2D経由で公開）
	std::vector<GameObject*> multiSelected2D_;

	// 矩形選択がドラッグ中かどうか、および開始位置（実ウィンドウpx、ImGuiのマウス座標系）。
	// 「何もない場所を左クリックした」瞬間に true になり、ボタンを離すまで毎フレーム
	// 現在のマウス位置とstartPosでプレビュー矩形を描画する
	bool isRectSelecting_ = false;
	ImVec2 rectSelectStartPos_{};

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
