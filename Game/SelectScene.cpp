#include "SelectScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void SelectScene::HandleSceneTransitionInput() {
	if (Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = SceneType::kTitle;
	}
	// Enterキーでプレイ開始（エディタUIが無いReleaseビルド既定状態でも
	// キーボードだけで先へ進めるようにするための最低限の導線）
	if (Input::IsTriggered(DIK_RETURN)) {
		nextScene_ = SceneType::kPlay;
	}
}
