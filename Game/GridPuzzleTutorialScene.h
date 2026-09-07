#pragma once
#include "GridPuzzleScene.h"

// GridPuzzleの操作を教えるチュートリアル用シーン。ユーザー指示により、まずは「GridPuzzleScene
// と同じ環境（盤面・プレイヤー・アイテム・壁・敵HPバー等一式）」をそのまま持つ土台として、
// GridPuzzleSceneを継承するだけの空のサブクラスにしてある。GridPuzzleSceneのOnInitialize/
// HandleSceneTransitionInput等は一切上書きしていないため、現時点ではGridPuzzleSceneと全く同じ
// 挙動になる（保存データだけはシーン名が異なることで自動的に分かれる。SceneBase::Initializeが
// SceneManager::ChangeScene時の遷移先名（"GridPuzzleTutorial"）をそのままassetFolder_に使うため、
// Resources/GridPuzzleTutorial/配下に独立したscene.json/ui.jsonを持つ。Resources/GridPuzzle/Field/
// の手動配置ファイルはGridFieldLoaderComponent側がパスを固定で持っているため、こちらは
// GridPuzzle本編と共有される）。
//
// 今後、操作説明のテキスト・誘導演出（クリックを促すマーカー等、TutorialScene参照）といった
// チュートリアル固有のロジックをこのクラスに追加していく想定
class GridPuzzleTutorialScene : public GridPuzzleScene {
};
