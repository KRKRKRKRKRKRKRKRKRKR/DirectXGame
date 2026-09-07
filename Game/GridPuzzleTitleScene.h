#pragma once
#include "SceneBase.h"

// GridPuzzle企画の「タイトル→セレクト→プレイ」導線のうち、タイトル画面担当のシーン。
// GridPuzzleScene本編（盤面・プレイヤー等）とは無関係の、純粋なタイトル表示専用シーンのため
// GridPuzzleSceneではなくSceneBaseを直接継承する。
//
// セレクト画面（フィールドを選ぶ画面）がまだ無いため、現時点ではEnter/Spaceキーを押すと
// 暫定的に直接GridPuzzleへ遷移する。セレクト画面ができたら、HandleSceneTransitionInput内の
// 遷移先をそちらへ差し替える想定
class GridPuzzleTitleScene : public SceneBase {
protected:
	void HandleSceneTransitionInput() override;

private:
	// OnInitialize()はLoadScene()より前に呼ばれるため、その時点ではまだResources/
	// GridPuzzleTitle/scene.jsonの内容が読み込まれていない。GridPuzzleScene::needsInitialSpawn_と
	// 同じ理由で、LoadScene()完了後に必ず呼ばれるHandleSceneTransitionInputの初回フレームで
	// 一度だけEnsureInitialObjectsExist()を実行してfalseに落とす
	bool needsInitialSpawn_ = true;

	// タイトルテキスト・案内テキストのうち、まだ存在しないものだけを新規に組み立てる
	// （scene.jsonから既に読み込み済みなら何もしない。GridPuzzleScene::EnsureInitialObjectsExistと
	// 同じ「無ければ作る」方式）
	void EnsureInitialObjectsExist();
};
