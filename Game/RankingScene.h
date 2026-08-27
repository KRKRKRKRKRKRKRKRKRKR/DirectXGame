#pragma once
#include "SceneBase.h"

// ランキング画面。ClearSceneでNextボタンを押した直後、およびTitle画面の「ランキングを見る」
// ボタンからの両方の経路で遷移してくる。RankingManager::GetEntries()の全件を、
// AlphabetTextComponentの3Dモデル文字で縦一列に表示する。
//
// 責務は完全に分離されている：
// - RankingComponent（Resources/Ranking/scene.jsonに配置するGameObject）：行・列の表示設定のみ。
//   スクロールでは一切動かない
// - RankingCameraScrollerComponent（カメラGameObjectに配置）：スクロール状態、Titleボタン、
//   自動フォーカス演出を一括管理し、カメラのtranslation.yだけを動かす
// このシーンは両コンポーネントを見つけて処理を回すだけの薄い実装で、依存の向きは一方向
// （RankingComponentが行を並べる → カメラ側コンポーネントがそれを動かして見せる）。
//
// スクロールのクランプは、1位（先頭）と最終行（末尾）の実際のワールドY座標を直接使う：
// カメラの高さ（baseY + scrollOffset）が1位のYより上（scrollOffset>0）、最終行のYより下
// （scrollOffset < 最終行Y - baseY）に行かないようclampするだけ。以前は「fov・カメラ距離・
// rowSpacingから可視行数を計算し、行数×rowSpacingで可動範囲を求める」という間接的な方式で、
// 手動値と実際の見た目がズレて不具合を繰り返し起こしていた。実際の行GameObjectの座標を
// そのままクランプの基準にすることで、幾何計算・手動設定値のどちらにも依存しなくなる
//
// Titleへ戻る手段はESCキー（SelectScene/GameOverSceneと同じ運用）と、Clear画面のNextボタンと
// 同じ方式（AlphabetTextComponent＋OBBCollider+PlayButtonComponent）のTitleボタンの両方
class RankingScene : public SceneBase {
protected:
	void OnInitialize() override;
	void HandleSceneTransitionInput() override;

private:
	// ---- RankingComponent（表示専用）----

	// RankingManager::GetEntries()の全件ぶん、ownerの子として表示行(1行=順位/名前/スコアの
	// 3列AlphabetTextComponent)を作り直す。各行のY位置はここで一度だけ確定させ、以後は動かさない
	// （スクロールはカメラ側コンポーネントが担当するため）。呼ぶ前に必ずClearRankingRowsで
	// 前回分を消しておくこと
	void RebuildRankingRows(GameObject& owner, const RankingComponent& comp);

	// ownerの子のうちtag==kRankingRowのGameObjectを全部削除する
	void ClearRankingRows(GameObject& owner);

	// comp.NeedsRebuild()を見て必要なら再構築するだけの薄いラッパー（毎フレームHandleSceneTransition
	// Inputから呼ぶ）
	void UpdateRankingDisplay(GameObject& owner, RankingComponent& comp);

	// ---- RankingCameraScrollerComponent（カメラの動き・演出）----

	// タグ"MainCamera"を優先し、無ければシーン内で最初に見つかったCameraComponent持ちの
	// GameObjectへフォールバックする（SceneBase::ResolveGameCameraと同じ探索順）。見つからなければ
	// nullptr（scene.jsonにカメラがまだ配置されていない状態でも遷移自体はできるようにする）
	GameObject* FindRankingCamera();

	// cameraObj（RankingCameraScrollerComponentが付いたGameObject自身）のtranslation.yへ
	// scroller.baseY + scroller.scrollOffsetを反映する。行そのものは一切動かさないため、
	// これが「スクロール」の実体になる。baseYは「カメラ自身の初期translation」ではなく
	// 「1位（先頭）の行の実際のワールドY座標」から一度だけ計算する（カメラ位置がscene.json上で
	// 1位からズレて保存されていても、scrollOffset=0が必ず1位を指すようにするため）。
	// rankingOwnerは1位の行を探すためだけに使う（表示側への書き込みは一切行わない）
	void ApplyCameraScroll(GameObject& cameraObj, RankingCameraScrollerComponent& scroller, GameObject& rankingOwner);

	// rankingOwnerの子（kRankingRowタグ）のうち先頭（1位）・末尾（最下位）の実際のワールドY座標を
	// 使って、scroller.scrollOffsetを[bottomY - baseY, 0]の範囲にクランプする。上限0は
	// 「1位がカメラ中心より上に行かない」、下限bottomY-baseYは「最終行がカメラ中心より下に
	// 行かない」を意味する。行が1件も無い場合は何もしない
	void ClampScrollOffset(RankingCameraScrollerComponent& scroller, GameObject& rankingOwner);

	// 自動フォーカス演出を開始する：scroller.scrollOffsetを0（1位が見える位置）に戻し、
	// targetRow（実際の行GameObject、ワールドY座標をGetWorldTransformで読む）がカメラの高さに
	// 来る値をそのまま目標値としてscroller.autoFocusPlaying=trueにする（クランプは行わない、
	// スクロール自体に上下限が無いため）。実際の補間はUpdateAutoFocusAnimationが毎フレーム行う。
	// HandleSceneTransitionInputから、自分がSubmitした行が判明した最初のフレームにのみ呼ぶ
	void StartAutoFocusAnimation(RankingCameraScrollerComponent& scroller, GameObject& targetRow);

	// targetRow（実際の行GameObject）の3列（順位/名前/スコア）の色をscroller.highlightColorに
	// 変える。UpdateAutoFocusAnimationが演出完了の瞬間にのみ呼ぶ（「到着した瞬間に黄色にする」
	// というユーザー指定のため）
	void ApplyHighlightColor(GameObject& targetRow, const RankingCameraScrollerComponent& scroller);

	// scroller.autoFocusPlaying中のみ、経過時間からイージング後のtでscroller.scrollOffsetを
	// autoFocusStartOffset→autoFocusTargetOffsetへ補間する。duration到達で再生を止め、
	// scrollOffsetを目標値に確定させたうえでApplyHighlightColorを呼ぶ。targetRowは演出対象の
	// 行GameObject（ハイライト適用に使う）
	void UpdateAutoFocusAnimation(RankingCameraScrollerComponent& scroller, GameObject* targetRow, float deltaTime);

	// Titleへ戻るボタン（kRankingTitleButtonText/kRankingTitleButtonHitboxタグ、scene.json配置）の
	// translationを、カメラのワールド位置＋scroller.titleButtonX/Y/Zで毎フレーム上書きする
	// （カメラに追従させる）。カメラ・対象GameObjectが見つからない場合は何もしない
	void UpdateTitleButtonPosition(GameObject& cameraObj, const RankingCameraScrollerComponent& scroller);

	// GameSession::ConsumeLastSubmittedEntryIndex()で取得した「自分がSubmitした行のインデックス」
	// を保持する。OnInitialize()時点ではまだRankingComponentのGameObjectが存在しない
	// （LoadScene()より前のため）ため、Scene側のメンバとして持ち越しておき、初回のHandleScene
	// TransitionInputで行GameObjectが揃ってから実際に使う
	bool hasPendingHighlight_ = false;
	size_t pendingHighlightIndex_ = 0;
};
