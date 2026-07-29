#include "SelectScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void SelectScene::HandleSceneTransitionInput() {
	if (Input::IsTriggered(DIK_ESCAPE)) {
		nextScene_ = SceneType::kTitle;
	}
}
