#include "GameOverScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void GameOverScene::HandleSceneTransitionInput() {
	if (Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = SceneType::kTitle;
	}
}
