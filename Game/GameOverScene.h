#pragma once
#include "SceneBase.h"

// ゲームオーバー画面。GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じく
// GameObjectの作成・削除・Gizmo編集・Save/Loadが可能）。シーン遷移はImGuiのシーン切替
// ボタンから行うため、GameOverScene固有のキー入力は無い（SceneBaseのデフォルトのまま）
class GameOverScene : public SceneBase {
};
