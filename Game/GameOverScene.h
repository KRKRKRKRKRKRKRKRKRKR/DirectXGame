#pragma once
#include "SceneBase.h"

// ゲームオーバー画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。エディタUIが無い実プレイ時でも
// 進行できるよう、Enter/EscキーでTitleへ戻る部分だけがGameOverScene固有
class GameOverScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;
};
