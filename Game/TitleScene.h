#pragma once
#include "SceneBase.h"

// タイトル画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。
// キー入力によるSelectへの遷移は廃止済み（HandleSceneTransitionInputは現在空実装）
class TitleScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;
};
