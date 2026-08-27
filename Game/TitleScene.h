#pragma once
#include "SceneBase.h"

// タイトル画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。
// PLAYボタン（タグ"PlayButtonHitbox"のGameObject、OBBColliderComponent+PlayButtonComponent）が
// クリックされたらPlaySceneへ遷移する。ホバー中は対象のPLAY文字（タグ"PlayButtonText"の
// AlphabetTextComponent）の色・サイズを変化させる
class TitleScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;
};
