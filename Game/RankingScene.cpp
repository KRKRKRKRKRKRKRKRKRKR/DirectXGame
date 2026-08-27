#include "RankingScene.h"
#include "GameTags.h"
#include "RankingManager.h"
#include "GameSession.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Physics/PlayButtonComponent.h"
#include "../Math/EaseUtil.h"
#include <algorithm>

namespace {
	// row（1行分の親GameObject）の下に、1列分のAlphabetTextComponent付き子GameObjectを作る。
	// 「順位」「名前」「スコア」の3列それぞれで呼ぶ共通ロジック（列位置・文字サイズ以外は
	// 同じ組み立て方のため）。columnXはrowからの相対Xオフセット、実際のGameObject生成は
	// 呼び出し元（RankingScene::RebuildRankingRows）のCreateObjectに委ねる。alignは列ごとに
	// 個別指定する（スコア列だけ右揃えにしたいユーザー指定のため、順位/名前は左揃えのまま）
	void SetupColumnObject(GameObject& column, GameObject& row, float columnX,
		const std::string& text, const RankingComponent& comp, AlphabetTextComponent::HorizontalAlign align) {
		column.tag = GameTags::kRankingRow;
		column.excludeFromPicking = true;  // 演出用オブジェクトと同じく3Dクリック選択の対象外にする
		column.excludeFromGizmoList = true;
		column.excludeFromSave = true;     // 動的生成・エントリ数が変わるたびに作り直すため保存しない
		column.SetParent(&row);
		column.GetTransform().translation = { columnX, 0.0f, 0.0f };

		auto* textComp = column.AddComponent<AlphabetTextComponent>();
		textComp->text = text;
		textComp->horizontalAlign = align;
		textComp->charScale = comp.charScale;
		textComp->charSpacing = comp.charSpacing;
	}
}

void RankingScene::OnInitialize() {
	// ClearSceneのNext押下時のみGameSessionへ値がセットされている（TitleScene::
	// HandleSceneTransitionInputの「ランキングを見る」ボタン経由では何もセットしないため、
	// その場合はhasPendingHighlight_==falseのままになり、自動フォーカスは行われない）。
	// Consumeなので一度読んだらGameSession側は消費済み（-1）に戻る
	hasPendingHighlight_ = GameSession::GetInstance().ConsumeLastSubmittedEntryIndex(pendingHighlightIndex_);
}

// ============================================================
// RankingComponent（表示専用）
// ============================================================

void RankingScene::ClearRankingRows(GameObject& owner) {
	std::vector<GameObject*> rows;
	for (GameObject* child : owner.GetChildren()) {
		if (child->tag == GameTags::kRankingRow) rows.push_back(child);
	}
	if (!rows.empty()) DeleteObjects(rows);
}

void RankingScene::RebuildRankingRows(GameObject& owner, const RankingComponent& comp) {
	ClearRankingRows(owner);

	const auto& entries = RankingManager::GetInstance().GetEntries();

	for (size_t i = 0; i < entries.size(); ++i) {
		GameObject& row = CreateObject("RankingRow " + std::to_string(i + 1));
		row.tag = GameTags::kRankingRow;
		row.excludeFromPicking = true;
		row.excludeFromGizmoList = true;
		row.excludeFromSave = true;
		row.SetParent(&owner);
		// 行はここで一度だけ位置を確定させ、以後は一切動かさない（スクロールはカメラ側
		// コンポーネントが担当する。「カメラだけが上下に動く」というユーザー指定の方針）
		row.GetTransform().translation = {
			0.0f, comp.rowStartY - comp.rowSpacing * static_cast<float>(i), comp.rowZ };

		int rank = RankingManager::GetInstance().GetRank(i);
		const RankingManager::Entry& entry = entries[i];

		GameObject& rankColumn = CreateObject("RankingRow " + std::to_string(i + 1) + " Rank");
		SetupColumnObject(rankColumn, row, comp.rankColumnX, std::to_string(rank), comp,
			AlphabetTextComponent::HorizontalAlign::kLeft);

		GameObject& nameColumn = CreateObject("RankingRow " + std::to_string(i + 1) + " Name");
		SetupColumnObject(nameColumn, row, comp.nameColumnX, entry.name, comp,
			AlphabetTextComponent::HorizontalAlign::kLeft);

		// スコアのみ右揃え（ユーザー指定）：columnXが右端の基準位置になり、桁数が変わっても
		// 右端が揃うようになる
		GameObject& scoreColumn = CreateObject("RankingRow " + std::to_string(i + 1) + " Score");
		SetupColumnObject(scoreColumn, row, comp.scoreColumnX, std::to_string(entry.score), comp,
			AlphabetTextComponent::HorizontalAlign::kRight);
	}

	RebuildDerivedLists();
}

void RankingScene::UpdateRankingDisplay(GameObject& owner, RankingComponent& comp) {
	const auto& entries = RankingManager::GetInstance().GetEntries();
	if (!comp.NeedsRebuild(entries.size())) return;

	// DeleteObjects（ClearRankingRows経由）はgizmoTargets_の中身が変わりうる前提で、呼ぶたびに
	// 無条件でgizmoController_.ResetSelection()する。Inspectorで値をドラッグするたびに子（行）を
	// 再構築すると、そのたびに今まさに編集中のRankingComponent自身の選択が外れてしまう。
	// ここで再構築前の選択オブジェクトを控えておき、再構築後に選び直す
	GameObject* selectedBefore = gizmoController_.GetSelected(gizmoTargets_);
	RebuildRankingRows(owner, comp);
	comp.MarkBuilt(entries.size());
	if (selectedBefore) {
		gizmoController_.SetSelected(selectedBefore, gizmoTargets_);
	}
}

// ============================================================
// RankingCameraScrollerComponent（カメラの動き・演出）
// ============================================================

GameObject* RankingScene::FindRankingCamera() {
	// タグ"MainCamera"を優先し、無ければシーン内で最初に見つかったCameraComponentへ
	// フォールバックする（SceneBase::ResolveGameCameraと同じ探索順。HandleSceneTransitionInputは
	// GameCameraResolutionを直接受け取れないため、ここで同じロジックを再現している）
	if (GameObject* tagged = FindObjectByTag(GameTags::kMainCamera)) {
		if (tagged->GetComponent<CameraComponent>()) return tagged;
	}
	for (auto& obj : objects_) {
		if (obj->GetComponent<CameraComponent>()) return obj.get();
	}
	return nullptr;
}

void RankingScene::ApplyCameraScroll(GameObject& cameraObj, RankingCameraScrollerComponent& scroller,
	GameObject& rankingOwner) {
	// baseYは「カメラ自身の初期translation」ではなく「1位（先頭）の行の実際のワールドY座標」
	// から一度だけ計算する。カメラ位置がscene.json上で1位からズレて保存されていても、
	// scrollOffset=0が必ず1位を指すようにするため
	if (!scroller.baseYCaptured) {
		GameObject* firstRow = nullptr;
		for (GameObject* child : rankingOwner.GetChildren()) {
			if (child->tag == GameTags::kRankingRow) { firstRow = child; break; }
		}
		if (firstRow) {
			scroller.baseY = firstRow->GetWorldTransform().translation.y;
			scroller.baseYCaptured = true;
		}
	}
	if (!scroller.baseYCaptured) return; // 1位の行がまだ生成されていない：次フレーム以降に再試行

	Vector3 pos = cameraObj.GetTransform().translation;
	pos.y = scroller.baseY + scroller.scrollOffset;
	cameraObj.GetTransform().translation = pos;
}

void RankingScene::ClampScrollOffset(RankingCameraScrollerComponent& scroller, GameObject& rankingOwner) {
	if (!scroller.baseYCaptured) return;

	// rankingOwnerの直接の子（kRankingRowタグ）を順番に走査し、先頭（1位）と末尾（最下位）の
	// ワールドY座標を求める。RebuildRankingRowsがCreateObjectで生成順に積んでいるため、
	// GetChildren()の並びはそのまま順位順になっている
	GameObject* firstRow = nullptr;
	GameObject* lastRow = nullptr;
	for (GameObject* child : rankingOwner.GetChildren()) {
		if (child->tag != GameTags::kRankingRow) continue;
		if (!firstRow) firstRow = child;
		lastRow = child;
	}
	if (!firstRow || !lastRow) return;

	float topY = firstRow->GetWorldTransform().translation.y;
	float bottomY = lastRow->GetWorldTransform().translation.y;

	// カメラの高さ = baseY + scrollOffset。これがtopYより上（scrollOffset>0側）にも、
	// bottomYより下（scrollOffsetがさらに負側）にも行かないようクランプする
	float minOffset = bottomY - scroller.baseY; // カメラがbottomYに一致する時のscrollOffset
	float maxOffset = topY - scroller.baseY;    // カメラがtopYに一致する時のscrollOffset（通常0）
	scroller.scrollOffset = std::clamp(scroller.scrollOffset, minOffset, maxOffset);
}

void RankingScene::StartAutoFocusAnimation(RankingCameraScrollerComponent& scroller, GameObject& targetRow) {
	// targetRowのワールドYがカメラの高さに来る値を、そのまま目標値にする。targetRow自身が
	// 1位〜最終行のいずれかの実在する行であるため、この値は常にClampScrollOffsetが使う範囲
	// [bottomY-baseY, 0]に自動的に収まる（クランプ計算不要）
	scroller.autoFocusStartOffset = 0.0f; // 1位（先頭）が見える位置からスタート（ユーザー指定）
	scroller.autoFocusTargetOffset = targetRow.GetWorldTransform().translation.y - scroller.baseY;
	scroller.autoFocusElapsed = 0.0f;
	scroller.autoFocusPlaying = true;
	scroller.scrollOffset = scroller.autoFocusStartOffset;
}

void RankingScene::ApplyHighlightColor(GameObject& targetRow, const RankingCameraScrollerComponent& scroller) {
	for (GameObject* column : targetRow.GetChildren()) {
		if (auto* text = column->GetComponent<AlphabetTextComponent>()) {
			text->displayColor = scroller.highlightColor;
		}
	}
}

void RankingScene::UpdateAutoFocusAnimation(RankingCameraScrollerComponent& scroller, GameObject* targetRow, float deltaTime) {
	if (!scroller.autoFocusPlaying) return;

	scroller.autoFocusElapsed += deltaTime;
	float t = scroller.autoFocusDuration > 0.0f
		? EaseUtil::Clamp01(scroller.autoFocusElapsed / scroller.autoFocusDuration)
		: 1.0f;
	float easedT = Easing::Apply(scroller.autoFocusEasing, t);
	scroller.scrollOffset = EaseUtil::Lerp(scroller.autoFocusStartOffset, scroller.autoFocusTargetOffset, easedT);

	if (scroller.autoFocusElapsed >= scroller.autoFocusDuration) {
		scroller.autoFocusPlaying = false;
		scroller.scrollOffset = scroller.autoFocusTargetOffset;
		scroller.autoFocusApplied = true;
		// 「到着した瞬間に黄色にする」というユーザー指定のため、演出が終わったこのタイミングで
		// 初めてハイライト色を適用する
		if (targetRow) ApplyHighlightColor(*targetRow, scroller);
	}
}

void RankingScene::UpdateTitleButtonPosition(GameObject& cameraObj, const RankingCameraScrollerComponent& scroller) {
	Vector3 camPos = cameraObj.GetWorldTransform().translation;
	Vector3 buttonPos = { camPos.x + scroller.titleButtonX, camPos.y + scroller.titleButtonY, scroller.titleButtonZ };

	if (GameObject* textObj = FindObjectByTag(GameTags::kRankingTitleButtonText)) {
		textObj->GetTransform().translation = buttonPos;
	}
	if (GameObject* hitboxObj = FindObjectByTag(GameTags::kRankingTitleButtonHitbox)) {
		hitboxObj->GetTransform().translation = buttonPos;
	}
}

// ============================================================
// シーン全体のとりまとめ
// ============================================================

void RankingScene::HandleSceneTransitionInput() {
	// Titleへ戻る手段。ESCキー（SelectScene/GameOverSceneと同じ運用）に加えて、Clear画面の
	// Nextボタンと同じ方式（AlphabetTextComponentの見た目＋OBBCollider+PlayButtonComponentの
	// 当たり判定）でTitleボタンも用意する
	if (Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = "Title";
	}

	GameObject* titleHitbox = FindObjectByTag(GameTags::kRankingTitleButtonHitbox);
	auto* titleButton = titleHitbox ? titleHitbox->GetComponent<PlayButtonComponent>() : nullptr;
	if (titleButton) {
		if (GameObject* titleTextObj = FindObjectByTag(GameTags::kRankingTitleButtonText)) {
			if (auto* text = titleTextObj->GetComponent<AlphabetTextComponent>()) {
				bool hovering = titleButton->IsHovering();
				text->displayScaleMultiplier = hovering ? titleButton->hoverScaleMultiplier : titleButton->normalScaleMultiplier;
				text->displayColor = hovering ? titleButton->hoverColor : titleButton->normalColor;
			}
		}
		if (titleButton->ConsumeClicked()) {
			nextScene_ = "Title";
		}
	}

	// RankingComponent（表示）を更新する。通常1つのみ配置される想定だが、複数あっても
	// 構わないよう全部処理する。objects_を直接回すが、UpdateRankingDisplay内でDeleteObjects/
	// CreateObjectを呼ぶため、対象を先に集めてからループの外で処理する
	// （ClearAlphabetTextChildren等と同じ理由でイテレータ無効化を避ける）
	std::vector<GameObject*> rankingOwners;
	for (auto& obj : objects_) {
		if (obj->GetComponent<RankingComponent>()) rankingOwners.push_back(obj.get());
	}
	for (GameObject* owner : rankingOwners) {
		UpdateRankingDisplay(*owner, *owner->GetComponent<RankingComponent>());
	}

	// RankingCameraScrollerComponent（カメラの動き）を更新する。表示（RankingComponent）が
	// 1つ目に必要なので、上のループの後に行う
	GameObject* cameraObj = FindRankingCamera();
	auto* scroller = cameraObj ? cameraObj->GetComponent<RankingCameraScrollerComponent>() : nullptr;
	if (cameraObj && scroller && !rankingOwners.empty()) {
		GameObject& rankingOwner = *rankingOwners.front();
		const auto& entries = RankingManager::GetInstance().GetEntries();

		ApplyCameraScroll(*cameraObj, *scroller, rankingOwner);

		// GameSessionから受け取った「自分がSubmitした行」への自動フォーカス演出を開始する（初回のみ）
		GameObject* highlightRow = nullptr;
		if (scroller->highlightEntryIndex >= 0) {
			size_t idx = 0;
			for (GameObject* child : rankingOwner.GetChildren()) {
				if (child->tag != GameTags::kRankingRow) continue;
				if (idx == static_cast<size_t>(scroller->highlightEntryIndex)) { highlightRow = child; break; }
				++idx;
			}
		}
		if (hasPendingHighlight_ && !scroller->autoFocusApplied && !scroller->autoFocusPlaying
			&& pendingHighlightIndex_ < entries.size()) {
			scroller->highlightEntryIndex = static_cast<int>(pendingHighlightIndex_);
			size_t idx = 0;
			for (GameObject* child : rankingOwner.GetChildren()) {
				if (child->tag != GameTags::kRankingRow) continue;
				if (idx == pendingHighlightIndex_) { highlightRow = child; break; }
				++idx;
			}
			if (highlightRow) {
				StartAutoFocusAnimation(*scroller, *highlightRow);
				hasPendingHighlight_ = false;
			}
		}

		// 演出中はユーザー指定により入力を無視する（「演出中は入力を無視して最後まで見せる」）。
		// 演出中でなければ、マウスホイールの分だけscrollOffsetを加減算し、1位・最終行の実座標で
		// クランプする（過去に作った別プロジェクトのランキング実装、Novice/2Dスプライトベースで
		// W/Sキーによりカメラを直接動かす方式を参照して採用したシンプルな方式）
		if (!scroller->autoFocusPlaying) {
			long wheel = Input::GetMouseWheel();
			if (wheel != 0) {
				// DirectInputのGetMouseWheel()は上向きに回すと正の値になる。ホイールを上に
				// 回したときに下位（2位以降）が見える＝カメラが下がる方向にscrollOffsetが
				// 動くよう、符号は反転させない
				scroller->scrollOffset += static_cast<float>(wheel) / 120.0f * scroller->wheelSensitivity;
				ClampScrollOffset(*scroller, rankingOwner);
			}
		} else {
			UpdateAutoFocusAnimation(*scroller, highlightRow, lastDeltaTime_);
		}

		ApplyCameraScroll(*cameraObj, *scroller, rankingOwner);
		UpdateTitleButtonPosition(*cameraObj, *scroller);
	}

	// リセットボタン（RankingComponent側のInspectorボタン）。表示側のコンポーネントに
	// 付いているが、実際のリセット処理・スクロール状態のクリアはここでまとめて行う
	for (GameObject* owner : rankingOwners) {
		auto* comp = owner->GetComponent<RankingComponent>();
		if (comp && comp->ConsumeResetRequested()) {
			RankingManager::GetInstance().Reset();
			// 全件削除後は表示中の行が消えたエントリを指したままになるため、件数自体が
			// 変わらない場合（0件→0件等）でもNeedsRebuildが食い違いを検知できるよう、直前に
			// 組み立てたスナップショットを強制的に無効化して次回必ず再構築させる
			comp->lastBuiltEntryCount = static_cast<size_t>(-1);
			if (scroller) scroller->scrollOffset = 0.0f;
		}
	}
}

REGISTER_SCENE(RankingScene, "Ranking");
