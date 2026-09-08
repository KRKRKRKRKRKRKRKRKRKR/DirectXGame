#pragma once
#include "SceneBase.h"

// GridPuzzle企画の「タイトル→セレクト→プレイ」導線のうち、タイトル画面担当のシーン。
// GridPuzzleScene本編（盤面・プレイヤー等）とは無関係の、純粋なタイトル表示専用シーンのため
// GridPuzzleSceneではなくSceneBaseを直接継承する。
//
// 自動生成（タイトル文字・案内文言・Enter/Spaceキー遷移）は廃止済みで、SceneBaseをそのまま
// 使う空シーン（GenericSceneと同等）。見た目・遷移ボタンはエディタ上でAlphabetTextComponent
// （enableClick+transitionTargetScene）等を使って自由に組み立てる
class GridPuzzleTitleScene : public SceneBase {
};
