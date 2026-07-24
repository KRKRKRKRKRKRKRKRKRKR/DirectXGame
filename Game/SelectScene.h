#pragma once
#include "SceneBase.h"

// セレクト画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。ESCキーでTitleへ戻る／Enterキーで
// Playへ進む部分だけがSelectScene固有
class SelectScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;
};
