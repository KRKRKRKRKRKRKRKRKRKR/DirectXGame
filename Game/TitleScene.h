#pragma once
#include "SceneBase.h"

// タイトル画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。ENTERキーでSelectへ遷移する
// 部分だけがTitleScene固有
class TitleScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;
};
