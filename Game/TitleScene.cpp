#include "TitleScene.h"
#include "GameTags.h"
#include "GameSession.h"

void TitleScene::HandleSceneTransitionInput() {
	// PlayButtonComponentは自分のOBBColliderComponentとマウスレイの交差判定・クリック検知は
	// 自分のUpdate()内で完結させている（Engine層のコンポーネントのため、Game層のタグ名・
	// シーン遷移先を知らない設計。詳しくはPlayButtonComponent.hのコメント参照）。
	// SceneBase::UpdateButtonAndReflectHoverが「hitbox取得→見た目反映→クリック検知」の
	// 骨格を共通化しているため、ここでは戻り値を見てシーン遷移の指示だけを行う
	auto playResult = UpdateButtonAndReflectHover(GameTags::kPlayButtonHitbox, GameTags::kPlayButtonText);
	if (playResult.clicked) {
		// 前回プレイの最終スコア・入力名を引きずらないよう、新しいプレイの開始点でリセットする
		GameSession::GetInstance().Reset();
		nextScene_ = "Tutorial";
	}

	// 「ランキングを見る」ボタン。PLAYボタンと同じOBBColliderComponent+PlayButtonComponent方式で、
	// scene.json側に配置されている前提（無ければ何もしない）
	auto viewRankingResult = UpdateButtonAndReflectHover(GameTags::kViewRankingHitbox, GameTags::kViewRankingButtonText);
	if (viewRankingResult.clicked) {
		nextScene_ = "Ranking";
	}
}

REGISTER_SCENE(TitleScene, "Title");
