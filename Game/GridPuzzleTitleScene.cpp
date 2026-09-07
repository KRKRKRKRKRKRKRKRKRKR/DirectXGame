#include "GridPuzzleTitleScene.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/Component/Render/TextSpriteComponent.h"

namespace {
	constexpr const char* kTitleTextTag = "GridPuzzleTitleText";
	constexpr const char* kPromptTextTag = "GridPuzzleTitlePrompt";
}

void GridPuzzleTitleScene::EnsureInitialObjectsExist() {
	// タイトル文字（画面中央やや上、大きめのフォント）
	if (!FindObjectByTag(kTitleTextTag)) {
		GameObject& title = CreateObject("TitleText");
		title.tag = kTitleTextTag;
		title.excludeFromPicking = true;
		title.GetTransform().translation = { 640.0f, 260.0f, 0.0f };

		auto* text = title.AddComponent<TextSpriteComponent>();
		text->fontSize = 72.0f;
		text->horizontalAlign = TextSpriteComponent::HorizontalAlign::kCenter;
		text->text = "GRID PUZZLE";
		text->Rebuild(renderer_);

		title.GetComponent<TransformComponent>()->is2D = true;
	}

	// 案内文字（タイトルの下、開始操作を促す）
	if (!FindObjectByTag(kPromptTextTag)) {
		GameObject& prompt = CreateObject("PromptText");
		prompt.tag = kPromptTextTag;
		prompt.excludeFromPicking = true;
		prompt.GetTransform().translation = { 640.0f, 420.0f, 0.0f };

		auto* text = prompt.AddComponent<TextSpriteComponent>();
		text->fontSize = 28.0f;
		text->horizontalAlign = TextSpriteComponent::HorizontalAlign::kCenter;
		text->text = "Enterキーでスタート";
		text->Rebuild(renderer_);

		prompt.GetComponent<TransformComponent>()->is2D = true;
	}

	// CreateObjectで追加したGameObjectをgizmoTargets_（Update/Draw対象一覧）に反映する
	RebuildDerivedLists();
}

void GridPuzzleTitleScene::HandleSceneTransitionInput() {
	if (needsInitialSpawn_) {
		needsInitialSpawn_ = false;
		EnsureInitialObjectsExist();
	}

	// セレクト画面（フィールドを選ぶ画面）がまだ無いため、暫定的に直接GridPuzzleへ遷移する。
	// セレクト画面ができたらここを"GridPuzzleSelect"等の遷移先へ差し替える
	if (Input::IsTriggered(DIK_RETURN) || Input::IsTriggered(DIK_SPACE)) {
		nextScene_ = "GridPuzzle";
	}
}

REGISTER_SCENE(GridPuzzleTitleScene, "GridPuzzleTitle");
