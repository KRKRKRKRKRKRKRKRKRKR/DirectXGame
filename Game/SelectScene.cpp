#include "SelectScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void SelectScene::HandleSceneTransitionInput() {
	if (Input::IsTriggered(DIK_RETURN)) {
		nextScene_ = SceneType::kPlay;
	} else if (Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = SceneType::kTitle;
	}
}
