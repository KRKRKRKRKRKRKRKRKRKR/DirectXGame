#include "TitleScene.h"
#include "GameTags.h"
#include "GameSession.h"
#include "../Engine/Utils/Logger.h"

void TitleScene::HandleSceneTransitionInput() {
	// PlayButtonComponentは自分のOBBColliderComponentとマウスレイの交差判定・クリック検知は
	// 自分のUpdate()内で完結させている（Engine層のコンポーネントのため、Game層のタグ名・
	// シーン遷移先を知らない設計。詳しくはPlayButtonComponent.hのコメント参照）。
	// ここではその結果（IsHovering/ConsumeClicked）を読んで、PLAY文字への見た目反映と
	// シーン遷移の指示だけを行う
	GameObject* hitbox = FindObjectByTag(GameTags::kPlayButtonHitbox);
	if (!hitbox) return;
	auto* playButton = hitbox->GetComponent<PlayButtonComponent>();
	if (!playButton) return;

	if (GameObject* textObj = FindObjectByTag(GameTags::kPlayButtonText)) {
		if (auto* text = textObj->GetComponent<AlphabetTextComponent>()) {
			bool hovering = playButton->IsHovering();
			text->displayScaleMultiplier = hovering ? playButton->hoverScaleMultiplier : playButton->normalScaleMultiplier;
			text->displayColor = hovering ? playButton->hoverColor : playButton->normalColor;
		}
	}

	if (playButton->ConsumeClicked()) {
		// 前回プレイの最終スコア・入力名を引きずらないよう、新しいプレイの開始点でリセットする
		GameSession::GetInstance().Reset();
		nextScene_ = "Tutorial";
	}

	// 「ランキングを見る」ボタン。PLAYボタンと同じOBBColliderComponent+PlayButtonComponent方式で、
	// scene.json側に配置されている前提（無ければ何もしない）。ホバー中の色・サイズ変化もPLAYボタンと
	// 同様に行う（kViewRankingButtonTextタグのAlphabetTextComponentへ反映する）
	if (GameObject* viewRankingHitbox = FindObjectByTag(GameTags::kViewRankingHitbox)) {
		if (auto* viewRankingButton = viewRankingHitbox->GetComponent<PlayButtonComponent>()) {
			bool hovering = viewRankingButton->IsHovering();
			static bool s_lastLoggedHovering = false;
			if (hovering != s_lastLoggedHovering) {
				Logger::Log(std::string("[TitleScene] ViewRanking hovering=") + (hovering ? "true" : "false") + "\n");
				s_lastLoggedHovering = hovering;
			}

			if (GameObject* viewRankingTextObj = FindObjectByTag(GameTags::kViewRankingButtonText)) {
				if (auto* text = viewRankingTextObj->GetComponent<AlphabetTextComponent>()) {
					text->displayScaleMultiplier = hovering ? viewRankingButton->hoverScaleMultiplier : viewRankingButton->normalScaleMultiplier;
					text->displayColor = hovering ? viewRankingButton->hoverColor : viewRankingButton->normalColor;
				}
			} else {
				static bool s_loggedMissingText = false;
				if (!s_loggedMissingText) {
					Logger::Log("[TitleScene] ViewRankingButtonText not found\n");
					s_loggedMissingText = true;
				}
			}

			if (viewRankingButton->ConsumeClicked()) {
				nextScene_ = "Ranking";
			}
		}
	} else {
		static bool s_loggedMissingHitbox = false;
		if (!s_loggedMissingHitbox) {
			Logger::Log("[TitleScene] ViewRankingHitbox not found\n");
			s_loggedMissingHitbox = true;
		}
	}
}

REGISTER_SCENE(TitleScene, "Title");
