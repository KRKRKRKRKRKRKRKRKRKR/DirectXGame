#pragma once
#include "SceneBase.h"

// ゲームオーバー画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。ENTERキーでTitleへ戻る部分だけが
// GameOverScene固有
class GameOverScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;
};
