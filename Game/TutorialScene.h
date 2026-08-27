#pragma once
#include "PlayScene.h"

// チュートリアル画面。Title→Tutorial→Playの導線の中間に挟む、操作説明を兼ねた
// 体験プレイフィールド。PlaySceneのサブクラスとして作ることで、計画/実行/準備フェーズ、
// 敵撃破・HP・コンボ等のロジックをそのまま流用する（操作感をPlaySceneと完全に一致させるため）。
// PlaySceneとの差分は以下の3点：
// ・EnemySpawnerによる自動ランダムスポーンを行わない。代わりに、HandleSceneTransitionInputの
//   初回フレームで、コード上に固定した4座標（TutorialScene.cpp内kEnemySpawnPositions）へ
//   SpawnEnemyAtで敵を再配置する。scene.json側にtag=Enemyの実体を直接置く方式は、実機で
//   プレイして全滅させた状態のまま保存すると次回起動時に敵が0体のまま復活しなくなる
//   バグになっていたため撤去した（GameObjectをJSONへ永続化せず、毎回コードから作り直す）
// ・敵を全滅させた瞬間、自動でPlaySceneへ遷移する（PlayScene固有のESC/F1デバッグ遷移は行わない）
// ・実行フェーズ終了後の「倒した敵の自動補充スポーン」を行わない（BeginPreparingPhaseを空実装で
//   上書きし、常に0体まで倒しきれる＝全滅判定ができるようにする）
// アセットはResources/Tutorial/scene.json（敵テンプレート"AEnemy"だけを持つ小さなフィールド）を想定
class TutorialScene : public PlayScene {
protected:
	void HandleSceneTransitionInput() override;

	// PlayScene::BeginPreparingPhaseを空実装で上書きし、倒した敵の補充スポーンを行わない
	// （pendingRespawnTags_をクリアし、即座に計画フェーズへ戻すだけ）
	void BeginPreparingPhase() override;

	// タグ"Enemy"を経由せず、objects_を直接走査してReflexEnemyComponentを持つ実体の敵
	// （isTemplate==falseかつenabled==true）が1体も残っていないかを調べる。
	// FindObjectByTagは最初の1件しか返せないため、全滅判定にはこの専用関数を使う
	bool AreAllEnemiesDefeated();

	// kClickHintMarkerPositions[0]（最初の地点＝右上）に、ReflexPlayerComponentの経路予約
	// マーカーと同じ見た目（Circle.obj、波紋パルスアニメーション）のClickHintMarkerComponent
	// （tag=kClickHintMarker）と、その真上に"Click"というAlphabetTextComponentの子GameObjectを
	// 1組だけ配置する。HandleSceneTransitionInputの初回フレームで1度だけ呼ぶ
	void SpawnClickHintMarkers();

	// 毎フレーム呼ぶ。現在表示中のクリック誘導マーカーが持つPlayButtonComponentが
	// クリックされていたら（TitleScene::PLAYボタンと同じOBBColliderComponent+PlayButtonComponent
	// によるマウスレイ当たり判定）、マーカーを次の座標（右上→左上→左下→右下→また右上）へ移動する。
	// 以前はプレイヤーのwaypoint座標とマーカー座標の一致判定で間接的に検知していたが、
	// Circle.objの見た目サイズに対して許容誤差が合わず反応しないバグがあったため、
	// マーカー自身にOBB当たり判定を持たせて直接クリックを検知する方式に変更した
	void AdvanceClickHintIfClicked();

	// 現在何番目のkClickHintMarkerPositionsを表示中か（0=右上, 1=左上, 2=左下, 3=右下）。
	// クリックされるたびに巡回的に進める
	size_t clickHintIndex_ = 0;

	// tag=kTutorialHintAlphabetのGameObject（「CLICK to move」等の操作説明テキスト）に
	// SpawnMoveComponentを付与し、現在位置から奥（Z+方向）へ縮小しながら移動する退場演出を
	// 開始する（ClearScene::PlayBackspaceExitAnimationと同じ「SpawnMoveComponentのreverseScale+
	// destroyOnFinish」パターン）。実際の削除はHandleSceneTransitionInputが演出完了を検知して行う
	void StartHintExitAnimation();

	// プレイヤーが最初のクリック（経路予約の1点目）をした瞬間、StartHintExitAnimation()を
	// 1度だけ呼ぶためのフラグ
	bool hintRemoved_ = false;

	// tag=kTutorialHintAlphabetのGameObjectのtextを、FadeManagerが完全に消え終わった
	// （State::kIdleに戻った）瞬間に一度だけ流し込み、AlphabetTextComponent::
	// useCharEntranceAnimationによる登場演出を開始させる。scene.json上ではtextを空にしておき、
	// このタイミングまで表示させない（フェード中に文字が見えてしまわないようにするため）
	void ShowHintIfFadeFinished();
	bool hintShown_ = false;
};
