#pragma once
#include "SceneBase.h"

// クリア画面。PlayScene::UpdateExecutionPhaseStatsが目標ラウンド数に到達した瞬間、最終スコアを
// GameSession::SetScore()経由でここへ渡してから遷移してくる。
// 画面頭上（PlayシーンのKillCount等と同じAlphabetTextComponent方式）に合計スコアを表示し、
// その下に名前入力欄を表示する。名前はImGuiに依存しない自作のキーボード直接入力（A〜Z、
// Backspace）で編集する（Enterキーでの確定は無い）。画面上のNextボタン（PlayButtonComponentと
// 同じクリック式）をクリックすると、その時点の名前でRankingManager::Submitに登録してからTitleへ
// 遷移する。名前が1文字も入力されていない間はNextボタンを無効化（クリック不可・グレー表示）する。
// GameObjectエディタ機能一式はSceneBaseが提供する（PlaySceneと同じ）
class ClearScene : public SceneBase {
protected:
	void OnInitialize() override;
	void HandleSceneTransitionInput() override;

private:
	// OnInitialize()はLoadScene()より前に呼ばれるため、その時点ではまだシーンJSON
	// （ScoreAlphabet/NameInputAlphabet等）が読み込まれていない。PlayScene::needsInitialSpawn_と
	// 同じ理由でtrue初期化し、HandleSceneTransitionInputの最初の呼び出しで一度だけ
	// TextProviderを紐付けてfalseに落とす（毎フレームSetTextProviderし直す必要は無いため）。
	// このタイミングで、画面上の全オブジェクトをHideAllUntilEntrance()でZ方向の奥へ配置する
	bool needsInitialBind_ = true;

	// 入力中の名前。A〜Zのキー入力で1文字ずつ追加し、Backspaceで1文字削除する
	std::string enteredName_;

	// 名前は最低1文字必要（空文字のままランキングに登録させない）。Nextボタンはこれが空の間
	// クリックできない（PlayButtonComponent::enabled=falseにする）
	static constexpr size_t kMaxNameLength = 12;

	// 画面全体の登場演出（ApplySpawnLikeEntrance）をまだ実行していないかどうか。
	// Clear画面へのシーン遷移フェードアウトが終わった直後は画面がまだ見え始めたばかりで
	// 唐突に見えるため、FadeManagerがkFadingOut→kIdleへ戻った瞬間を検知し、そこから
	// kPostFadeDelaySeconds秒待ってから演出を実行する。trueの間は毎フレーム
	// wasFadingOut_/postFadeDelayElapsed_を見て実行タイミングを判定する
	bool spawnEntrancePending_ = true;

	// 直前フレームでFadeManagerがkFadingOut状態だったか。kFadingOut→kIdleへの遷移（＝
	// フェードアウトが完了した瞬間）を検知するために使う
	bool wasFadingOut_ = false;

	// フェードアウト完了を検知してから経過した秒数。kPostFadeDelaySeconds以上になったら
	// 演出を実行する。フェードアウトを一度も観測しないまま（例：F2デバッグキーでの直接遷移等、
	// 将来フェード無しでClearへ入る経路が増えた場合）シーンが進行し続けるケースに備え、
	// -1.0fを「まだフェードアウト完了を観測していない」の意味で使う
	float postFadeDelayElapsed_ = -1.0f;

	// フェードアウト完了から演出開始までの待機時間(秒)
	static constexpr float kPostFadeDelaySeconds = 1.0f;

	// タグtargetTagのGameObjectが見つかれば、SceneビューでInspector/Gizmoにより決められた
	// translation（scene.json保存位置）を最終着地点(targetPos)として扱い、そこからZ方向奥へ
	// 離れた位置(startPos)から手前へイージングで戻ってくる、PlayScene::
	// BuildEnemyFromTemplateDataの敵スポーン演出と同じ動きをさせる。AlphabetTextComponentが
	// 付いている場合は文字ごとの個別登場演出（useCharEntranceAnimation）も有効にする。
	// needsInitialBind_のタイミングで1回だけ呼ぶ想定
	void ApplySpawnLikeEntrance(const char* targetTag);

	// ApplySpawnLikeEntranceが動的に追加したSpawnSoundComponent（Play()を呼んだ直後で
	// 役目は終えている）を毎フレーム見て回り、取り除く。演出用の一時的な追加コンポーネントが
	// scene.jsonへ保存されてしまうのを避けるため
	void CleanupFinishedSpawnEntrances();

	// 画面上の全オブジェクト（Score/NameInput/NextButtonText/EnterNamePrompt）のtranslationを
	// Z方向奥（kInitialZOffsetぶん離れた位置）へ動かしておく。フェードアウトが終わって画面が
	// 見え始めるより前に、scene.json由来の最終位置へ演出なしでいきなり文字が並んでいるのが
	// 見えてしまう問題（敵スポーンで言えば複製前から見えているような状態）を防ぐ。
	// needsInitialBind_のタイミングで1回だけ呼ぶ
	void HideAllUntilEntrance();

	// HideAllUntilEntranceがZ方向へずらす距離(奥方向オフセット)
	static constexpr float kInitialZOffset = 100.0f;

	// Backspaceで名前入力欄の最後の1文字が削除される直前に呼ぶ。NameInputAlphabetの子
	// （tag==kAlphabetChar）のうち末尾の文字GameObjectを「差し押さえ」て親から切り離し、
	// SceneBase::RebuildAlphabetTextChildren（次のUpdateAlphabetTextComponentsで、短くなった
	// enteredName_に合わせて子を作り直す処理）の削除対象から外す。切り離した文字には
	// SpawnMoveComponent（reverseScale=true、destroyOnFinish=true）を付け、登場演出とは逆に
	// 手前から奥へ縮小しながら消える退場アニメーションを開始する。文字が1つも無い場合は何もしない
	void PlayBackspaceExitAnimation();

	// PlayBackspaceExitAnimationが独立させた退場アニメーション中の文字GameObjectを毎フレーム
	// 見て回り、SpawnMoveComponent::destroyOnFinish && finishedになったものをDeleteObjectsする
	void CleanupExitingChars();
};
