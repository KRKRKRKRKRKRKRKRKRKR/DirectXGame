#include "GridPuzzleTitleScene.h"

// タイトル文字・案内文言の自動生成、Enter/Spaceキーでの遷移はすべて廃止済み。
// このシーンは完全に空（GenericSceneと同等）で、見た目・遷移ボタンはすべてエディタ上で
// AlphabetTextComponent（enableClick+transitionTargetScene）等を使って自由に組み立てる運用にする

REGISTER_SCENE(GridPuzzleTitleScene, "GridPuzzleTitle");
