#include "TitleScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void TitleScene::HandleSceneTransitionInput() {
	if (Input::IsTriggered(DIK_RETURN)) {
		nextScene_ = SceneType::kSelect;
	}
}
