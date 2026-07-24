#include "TitleScene.h"
#include "../Engine/InputDevice/InputDevice.h"

void TitleScene::HandleSceneTransitionInput() {
	// Enterキーでセレクト画面へ進む（エディタUIが無いReleaseビルド既定状態でも
	// キーボードだけで先へ進めるようにするための最低限の導線）
	if (Input::IsTriggered(DIK_RETURN)) nextScene_ = SceneType::kSelect;
}
