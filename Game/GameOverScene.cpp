#include "GameOverScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void GameOverScene::HandleSceneTransitionInput() {
	// Enter/EscどちらでもTitleへ戻る（エディタUIが無いReleaseビルド既定状態でも
	// キーボードだけで先へ進めるようにするための最低限の導線）
	if (Input::IsTriggered(DIK_RETURN) || Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = SceneType::kTitle;
	}
}
