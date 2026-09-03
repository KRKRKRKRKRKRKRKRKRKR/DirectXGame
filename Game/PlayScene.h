#pragma once
#include "SceneBase.h"
#include "../Math/Easing.h"
#include "EnemySpawnManager.h"

// ゲームプレイ画面。GameObjectエディタ機能一式はSceneBaseが提供し、本クラスは
// 「初期HUDとしてCamera Coordを1つ置く」「ESCでTitleへ遷移する」
// 「起動時は保存されていた敵構成を問答無用で破棄し、EnemySpawnerの設定通りに敵をランダム配置し直す」
// 「実行フェーズが終わって準備フェーズに入ったら、直前の実行フェーズ中に倒した敵の数だけ、
// 同じ種類を1体ずつ間隔を空けて補充スポーンし、出し終えたら計画フェーズへ戻す」
// 「計画フェーズへ戻った回数（ラウンド）が目標ラウンド数（EnemySpawnerのInspectorで調整）に
// 達したら、そのときのスコアをランキングに登録するためClearSceneへ遷移する」という
// PlayScene固有の差分だけを持つ。
// 本ゲームにHP・GameOverの概念は存在しない（プレイヤーはダメージを受けない、スコアアタック方式）。
// 敵の種類は「シーン上のテンプレートGameObject」（例：tag="A"に見た目・ReflexEnemyComponentの
// パラメータを設定したもの）で表し、EnemySpawner側はそのタグ名と出現数だけを持つ
class PlayScene : public SceneBase {
	// SpawnEnemyAt/SpawnParticleBurstAt等の実装をEnemySpawnManagerへ切り出したため、
	// CreateObject/FindObjectByTag/RebuildDerivedLists/MakeComponentLoadContext/objects_/
	// projectAudioClips_（いずれもSceneBaseのprotectedメンバ）へアクセスできるようfriend指定する
	friend class EnemySpawnManager;

protected:
	void OnInitialize() override;
	void HandleSceneTransitionInput() override;

	// SceneBase::DrawInspector()が汎用UI描画の直後に呼ぶ拡張フック。ReflexEnemySpawnerComponent
	// （敵の種類＋出現数のリストUI）はシーン内のテンプレート一覧を参照する必要があり
	// IComponent::DrawImGui単体では描画できないため、PlayScene固有の拡張としてここに実装する
	// （SceneBase.cpp側はReflexEnemySpawnerComponent/ReflexEnemyComponentという具体型を知らずに済む）
	void DrawSceneSpecificInspectorExtensions(GameObject& selected) override;

	// タグ"Player"のGameObjectを探し、そのReflexPlayerComponentを返す（どちらか一方でも
	// 見つからなければnullptr）。HandleSceneTransitionInput/BeginPreparingPhase/
	// UpdatePreparingPhaseで同一の「Player検索→nullチェック→コンポーネント取得」パターンが
	// 重複していたため共通化した
	ReflexPlayerComponent* GetReflexPlayer();

	// タグ"Player"のGameObjectを探し、そのComboPopupComponentを返す（無ければnullptr）。
	// GetReflexPlayerと同じパターン。ユーザーがInspectorでプレイヤーにComboPopupComponentを
	// 付けていない場合はnullptrが返り、コンボ演出は単に出ないだけになる（必須コンポーネントにしない）
	ComboPopupComponent* GetComboPopup();

	// EnemyComponent::pendingDestroy==trueのオブジェクトをまとめて回収する。
	// ColliderSystem::ResolveAndDrawのループ中（OnTriggerEnterの中）ではGameObjectを
	// その場でeraseできないため、ループが完全に終わった後のこのタイミングでまとめて処理する
	void ProcessPendingDestroys();

	// 起動時（HandleSceneTransitionInputの初回フレーム、needsInitialSpawn_）専用。
	// tag=="Enemy"のオブジェクトが保存シーンJSONから復元されていた場合でも、それを問答無用で
	// 全部削除してから、EnemySpawnerの設定通りに敵を新規配置し直す（企画書6章「敵を全滅できたら
	// 次フィールドへ」の簡易版：フィールド切替の代わりに敵だけ再配置する）。
	// Playシーンを開くたびに毎回この関数で敵構成をリセットする
	void ResetAllEnemies();

	// ResetAllEnemiesが使うスポーン処理本体（EnemySpawnerの設定を読み、グリッド配置で敵を生成する）。
	// 「既存の敵を先に消す」のはResetAllEnemies側の責務で、この関数自身は「今いる敵の状態」を
	// 一切気にしない
	void SpawnEnemiesFromConfig();

	// 実行フェーズ完了（ConsumeExecutionFinished）を検知した際に呼ぶ。pendingRespawnTags_に
	// 溜まっている「直前の実行フェーズ中に倒した敵の種類」を基にrespawnQueue_を構築し、
	// 準備フェーズ（1体ずつ間隔を空けたスポーン演出）を開始する。倒した敵が1体もいなければ
	// 補充するものが無いため、即座にFinishPreparing()を呼んで計画フェーズへ戻す。
	// virtual: TutorialSceneが「倒した敵を補充しない（全滅させたら次へ進める）」ようにする
	// ため、pendingRespawnTags_をクリアするだけの空実装で上書きする
	virtual void BeginPreparingPhase();

	// 準備フェーズ中、毎フレーム呼ぶ。respawnQueue_が空でなければrespawnTimer_をカウントし、
	// currentSpawnInterval_秒おきに1体ずつSpawnEnemyAtする。キューを使い切ったらFinishPreparing()を
	// 呼んで計画フェーズへ戻す。respawnQueue_が最初から空の場合は何もしない
	// （BeginPreparingPhaseが既にFinishPreparing()を呼んでいるはず）
	void UpdatePreparingPhase(float deltaTime);

	// EnemySpawner（タグ"EnemySpawner"、ReflexEnemySpawnerComponent）のspawnIntervalMin/Maxの
	// 範囲から次のスポーンまでの待ち時間をランダムに抽選する。EnemySpawnerが見つからない場合は
	// 既定値kRespawnIntervalを返す。BeginPreparingPhase（最初の1体分）とUpdatePreparingPhase
	// （2体目以降）の両方から呼ぶ共通ロジック
	float PickNextSpawnInterval();

	// tagを持つ空のGameObject（ヒエラルキー上のフォルダ代わり）を探し、無ければ新規作成して返す。
	// EnemySpawnManager::SpawnEnemyAt/SpawnParticleBurstAtが生成物をこの下にぶら下げることで、
	// 大量にスポーンしてもヒエラルキーがフラットに埋まらないようにする
	GameObject& GetOrCreateGroupFolder(const std::string& tag);

	// テンプレートからのGameObject組み立て（グリッド配置計算・テンプレート読み取り・複製）は
	// EnemySpawnManagerへ切り出した。PlayScene自身は「いつ・何体スポーンするか」という
	// フェーズ制御・タイミングに専念する
	EnemySpawnManager enemySpawnManager_;

	// 準備フェーズ用のスポーン待ちキュー（テンプレートタグの列）。BeginPreparingPhaseが
	// pendingRespawnTags_から構築し、UpdatePreparingPhaseが先頭から1つずつ消費する
	std::vector<std::string> respawnQueue_;

	// 準備フェーズ中の経過時間。currentSpawnInterval_秒に達するたびリセットして1体スポーンする
	float respawnTimer_ = 0.0f;

	// 次の1体をスポーンするまでの待ち時間（秒）。EnemySpawner::spawnIntervalMin/Maxの範囲から
	// 1体スポーンするたびに新しく抽選し直す値（＝ランダム間隔）。毎フレーム再抽選すると
	// 待ち時間が安定しなくなるため、1体分のスポーン間で固定して使う
	float currentSpawnInterval_ = 0.0f;

	// 実行フェーズ中（まだ準備フェーズに入る前）に倒された敵のspawnedFromTagを溜めておく場所。
	// ProcessPendingDestroysが撃破のたびにここへ追加するだけに留め、実際のスポーンは
	// BeginPreparingPhase/UpdatePreparingPhaseが準備フェーズ中に行う
	std::vector<std::string> pendingRespawnTags_;

	// OnInitialize()はLoadScene()より前に呼ばれるため、その時点ではまだシーンJSON
	// （EnemySpawner・テンプレート等）が読み込まれておらず初回スポーンができない。
	// trueで初期化し、HandleSceneTransitionInputの最初の呼び出しで一度だけ
	// RespawnEnemiesIfCleared()を実行してfalseに落とす
	bool needsInitialSpawn_ = true;

	// ResetAllEnemies()/SpawnEnemiesFromConfig()が起動直後の初回スポーンのためにReflexPlayerComponent
	// をkPreparingへ入れている間はtrue。この間に敵を出し終えてkPlanningへ戻っても、それは
	// 「1ラウンドを実際にプレイし終えた」わけではないため、UpdateExecutionPhaseStatsの
	// ラウンドカウントには数えない（数えてしまうと、目標ラウンド数を3に設定していても
	// 起動直後の初回スポーン完了分が1ラウンド目として先取りされ、実際には2ラウンド分しか
	// プレイしていないのにクリア扱いになる不具合になっていた）
	bool isInitialSpawnInProgress_ = false;

	// 実行フェーズ（ReflexPlayerComponent::Phase::kExecuting）中だけ加算される経過秒数。
	// HandleSceneTransitionInput内でフェーズを見ながら更新し、計画フェーズに戻るたびに
	// UpdateExecutionPhaseStats内で0にリセットする（1回の実行フェーズごとの計測に揃える）
	float executionTimer_ = 0.0f;

	// このプレイ（Title等からのReset以降）を通じての累計撃破数。実行フェーズかどうかに関わらず
	// ProcessPendingDestroysが敵を回収するたびに加算し続ける（フェーズをまたいでリセットしない）
	int killCount_ = 0;

	// 現在の連続撃破数。ProcessPendingDestroysが敵を倒すたびに加算し、計画フェーズに戻った
	// 瞬間（実行フェーズが完全に終わった扱い）に0へリセットする
	int comboCount_ = 0;

	// 現在の実行フェーズ中に稼いだスコアの累計（score_と同じ加算式で、comboCount_と同じ
	// タイミングで0にリセットする）。HUDの「Combo」欄はcomboCount_（連続撃破数そのもの）ではなく
	// こちらを表示する
	int phaseScore_ = 0;

	// 直前フレームのReflexPlayerComponent::Phaseを控えておく。kExecuting→kPlanning等の
	// 「フェーズが切り替わった瞬間」を検知するために使う（comboCount_のリセット・
	// executionTimer_のリセットは切り替わった瞬間の1回だけ行いたいため）
	ReflexPlayerComponent::Phase previousPhase_ = ReflexPlayerComponent::Phase::kPlanning;

	// HandleSceneTransitionInputから毎フレーム呼ぶ。ReflexPlayerComponentの現在フェーズを見て
	// executionTimer_の加算、計画フェーズへ戻った瞬間のexecutionTimer_/comboCount_リセット、
	// および計画フェーズへ戻った回数（roundCount_）のカウントアップを行う
	void UpdateExecutionPhaseStats(ReflexPlayerComponent* reflexPlayer, float deltaTime);

	// このプレイを通じての累計スコア。ProcessPendingDestroysが敵を倒すたびに、その瞬間の
	// comboCount_（1体倒すごとに加算される連続撃破数）を加算する。コンボを繋げるほど
	// 1体あたりの獲得スコアが増えていく仕組み
	int score_ = 0;

	// ScoreAlphabetのHUDに実際に表示している値。score_が加算された瞬間に一気に反映せず、
	// UpdateScoreDisplayが毎フレーム連続で近づけることで「1,2,3…10」と一の位から連続で
	// カウントアップする演出にする。score_が減ることは無い想定のため、追いつく方向は常に+
	int displayScore_ = 0;

	// カウントアップ演出1回ぶんの所要時間（秒）。差分の大小に関わらずこの時間で必ず追いつく
	// （+1でも+1000でも同じ1秒）。「スコアが多いほど演出が長くなる」体感を避けるため、
	// 1ずつ固定間隔で進める方式（差分に比例して時間が伸びる）から、固定時間で線形補間する
	// 方式に変更した
	static constexpr float kScoreCountUpDuration = 1.0f;

	// 現在再生中のカウントアップ演出の始点・終点・経過時間。score_が加算されてscoreAnimTarget_
	// と食い違った瞬間、UpdateScoreDisplayがその時点のdisplayScore_を新しい始点、score_を
	// 新しい終点として演出をやり直す（実行フェーズ中に連続で敵を倒すたびに再スタートする）
	int scoreAnimStart_ = 0;
	int scoreAnimTarget_ = 0;
	float scoreAnimElapsed_ = 0.0f;

	// HandleSceneTransitionInputから毎フレーム呼ぶ。score_とscoreAnimTarget_が食い違って
	// いれば演出を再スタートし、kScoreCountUpDuration秒かけてdisplayScore_をscore_へ
	// 線形補間で近づける
	void UpdateScoreDisplay(float deltaTime);

	// kPreparing→kPlanning（準備フェーズが終わって計画フェーズへ戻った瞬間）を迎えた回数。
	// UpdateExecutionPhaseStatsがカウントアップする。GetTotalRounds()に達したらClearSceneへ遷移する
	int roundCount_ = 0;

	// 目標ラウンド数（ReflexEnemySpawnerComponent::totalRounds）を読み取る。PlayScene自身の
	// メンバとして持つとシーンJSON保存/読込の対象外になり、Inspectorで変更しても保存されない
	// 問題があったため、EnemySpawnerのコンポーネントデータとして持たせている（Inspector側の
	// DrawSceneSpecificInspectorExtensionsも同じGameObjectを触っている）。EnemySpawnerが
	// 見つからない場合は既定値30を返す
	int GetTotalRounds();

	// 残りラウンド数がkRoundWarningThreshold以下になったら経過時間を積算するタイマー。
	// RoundCountAlphabetのパルス演出（拡大→元のサイズを繰り返す）の位相計算に使う。
	// 閾値を上回っている間は0のまま止めておく（次に閾値以下へ入った瞬間、常に同じ位相から始める）
	float roundWarningPulseTimer_ = 0.0f;

	// HandleSceneTransitionInputから毎フレーム呼ぶ。残りラウンド数がkRoundWarningThreshold以下の
	// 間、RoundCountAlphabetのdisplayColor（赤固定）とdisplayScaleMultiplier（パルス、残りが
	// 少ないほど周期が速くなる）を書き換えて警告演出を出す。閾値を上回っていれば既定値に戻す
	void UpdateRoundCountWarningVisual(float deltaTime);
};
