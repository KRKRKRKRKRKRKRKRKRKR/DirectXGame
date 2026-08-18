#pragma once
#include "SceneBase.h"
#include "../Math/Easing.h"

// ゲームプレイ画面。GameObjectエディタ機能一式はSceneBaseが提供し、本クラスは
// 「初期HUDとしてCamera Coordを1つ置く」「ESCでTitle・F1でGameOverへ遷移する」
// 「起動直後（敵0体）はEnemySpawnerの設定通りに敵をランダム配置する」
// 「実行フェーズが終わって準備フェーズに入ったら、直前の実行フェーズ中に倒した敵の数だけ、
// 同じ種類を1体ずつ間隔を空けて補充スポーンし、出し終えたら計画フェーズへ戻す」
// という PlayScene固有の差分だけを持つ。
// 敵の種類は「シーン上のテンプレートGameObject」（例：tag="A"に見た目・ReflexEnemyComponentの
// パラメータを設定したもの）で表し、EnemySpawner側はそのタグ名と出現数だけを持つ
class PlayScene : public SceneBase {
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

	// EnemyComponent::pendingDestroy==trueのオブジェクトをまとめて回収する。
	// ColliderSystem::ResolveAndDrawのループ中（OnTriggerEnterの中）ではGameObjectを
	// その場でeraseできないため、ループが完全に終わった後のこのタイミングでまとめて処理する
	void ProcessPendingDestroys();

	// 敵が全滅していれば（＝tag=="Enemy"のオブジェクトが1体も無ければ）EnemySpawnerの設定通りに
	// 次の敵をランダム配置で生成する
	// （企画書6章「敵を全滅できたら次フィールドへ」の簡易版：フィールド切替の代わりに敵だけ再配置する）
	// 呼び出し元はHandleSceneTransitionInputの初回フレーム（needsInitialSpawn_）のみ。
	// 準備フェーズ導入後、実行フェーズ完了トリガーからは呼ばれない（代わりにBeginPreparingPhaseが呼ばれる）
	void RespawnEnemiesIfCleared();

	// 実行フェーズ完了（ConsumeExecutionFinished）を検知した際に呼ぶ。pendingRespawnTags_に
	// 溜まっている「直前の実行フェーズ中に倒した敵の種類」を基にrespawnQueue_を構築し、
	// 準備フェーズ（1体ずつ間隔を空けたスポーン演出）を開始する。倒した敵が1体もいなければ
	// 補充するものが無いため、即座にFinishPreparing()を呼んで計画フェーズへ戻す
	void BeginPreparingPhase();

	// 準備フェーズ中、毎フレーム呼ぶ。respawnQueue_が空でなければrespawnTimer_をカウントし、
	// kRespawnInterval秒おきに1体ずつSpawnEnemyAtする。キューを使い切ったらFinishPreparing()を
	// 呼んで計画フェーズへ戻す。respawnQueue_が最初から空の場合は何もしない
	// （BeginPreparingPhaseが既にFinishPreparing()を呼んでいるはず）
	void UpdatePreparingPhase(float deltaTime);

	// フィールド内（壁の内側）をkGridCellSize間隔のグリッドに区切り、シャッフル済みの
	// 未使用セル一覧cellsから1つ取り出して消費する（呼び出しごとに違うセルを返す＝敵同士が
	// 絶対に重ならない）。cellsが尽きた場合はkMinSpawnDistance以上離れた地点をランダム再抽選する
	// 従来方式にフォールバックする（フィールド容量を超える数を要求された場合の保険）。
	// cellsはRespawnEnemiesIfClearedが1回のリスポーン処理の最初に1度だけ生成し、
	// スポーンする敵の数だけ呼び出し側でこの関数へ渡し続ける
	Vector3 PickEnemySpawnPosition(std::vector<Vector3>& cells);

	// フィールド内をkGridCellSize間隔で敷き詰めたグリッド点の一覧をランダムな順序で返す。
	// PickEnemySpawnPositionが順に消費していくことで、要求数がグリッドの容量以内である限り
	// 敵同士が絶対に重ならない配置になる
	std::vector<Vector3> BuildShuffledSpawnGrid();

	// 1体分のEnemy GameObjectを、templateTagを持つテンプレートGameObjectの見た目・当たり判定
	// サイズ・ReflexEnemyComponentのパラメータを複製して生成する
	// （Resources/Play/scene.jsonの既存Enemy構成を踏襲。テンプレートが見つからない場合は既定値）
	void SpawnEnemyAt(const Vector3& position, const std::string& templateTag);

	// templateTagを持つParticleTemplateComponent付きGameObjectの見た目・ParticleEmitterComponent
	// 設定を読み取り、position位置から全方位ランダムな方向へ指定個数分のParticleComponent付き
	// GameObjectを一括生成する（敵撃破時等、「四方八方に飛び散る」演出向け）。
	// テンプレートが見つからない場合は何も生成しない（EnemySpawnerと違い、パーティクルは
	// 演出のみでゲーム進行に必須ではないため、フォールバック見た目は用意しない）
	void SpawnParticleBurstAt(const Vector3& position, const std::string& templateTag);

	// tagを持つ空のGameObject（ヒエラルキー上のフォルダ代わり）を探し、無ければ新規作成して返す。
	// SpawnEnemyAt/SpawnParticleBurstAtが生成物をこの下にぶら下げることで、大量にスポーンしても
	// ヒエラルキーがフラットに埋まらないようにする
	GameObject& GetOrCreateGroupFolder(const std::string& tag);

	// テンプレートの描画形状（Cube/Sphere/Triangle）を表す。RenderComponentBaseはcolor等の
	// 共通項目しか持たないため、複製先に同じ具体型をAddComponentするには種類の判定が要る
	enum class TemplateShape { kCube, kSphere, kTriangle };

	// テンプレートのコライダー形状（OBB/Sphere）。サイズはhalfSize.x/radiusという別名の
	// スカラー値だが、どちらも「中心からの片側の長さ」という同じ意味として1つのfloatに統一する
	enum class TemplateColliderShape { kNone, kObb, kSphere };

	// RenderComponentBaseの具体型からTemplateShapeを判定する。SpawnEnemyAt/SpawnParticleBurstAtの
	// 両方が同じ判定を必要とするため共通化した
	static TemplateShape DetermineTemplateShape(GameObject& templateObj);

	// テンプレートGameObjectから読み取ったスポーンパラメータの集約。SpawnEnemyAtが
	// 「読み取り」と「組み立て」の2段階に分かれるようにするための橋渡し役。
	// 既定値は「テンプレートが見つからなかった場合のフォールバック値」（従来のSpawnEnemyAt
	// ローカル変数の初期値と同一）
	struct EnemyTemplateData {
		Vector4 color = { 0.9f, 0.2f, 0.2f, 1.0f };
		TemplateShape shape = TemplateShape::kCube;
		TemplateColliderShape colliderShape = TemplateColliderShape::kObb;
		float size = 0.5f; // kFallbackHalfSize相当（PlayScene.cpp冒頭の無名namespace定数と同じ値）
		float hitShakeStrength = 0.25f;
		float hitShakeDuration = 0.15f;
		float hitStopDuration = 0.05f;
		float maxHp = 10.0f;
		std::string textureName; // 空ならテンプレートにTextureSelectorComponentが無い（複製先にも付けない）

		bool hasHealthBar = false;
		float healthBarWidth = 1.2f;
		float healthBarHeight = 0.15f;
		float healthBarHeightOffset = 1.0f;
		Vector4 healthBarBackgroundColor = { 0.15f, 0.15f, 0.15f, 0.8f };
		Vector4 healthBarFillColor = { 0.2f, 0.9f, 0.2f, 1.0f };

		bool hasRotator = false;
		bool rotatorRandomizeOnSpawn = false;
		float rotatorSpeedX = 0.0f;
		float rotatorSpeedY = 90.0f;
		float rotatorSpeedZ = 0.0f;
		float rotatorRandomSpeedMin = 30.0f;
		float rotatorRandomSpeedMax = 180.0f;

		bool hasHitSound = false;
		std::string hitSoundAudioName;
		float hitSoundVolume = 1.0f;

		bool hasSpawnMove = false;
		float spawnMoveZOffset = 10.0f;
		float spawnMoveDuration = 0.5f;
		Easing::Type spawnMoveEasing = Easing::Type::kOutCubic;
	};

	// templateTagを持つテンプレートGameObjectから見た目・当たり判定・ReflexEnemyComponent
	// パラメータ等を読み取る（見つからない場合はEnemyTemplateDataの既定値＝従来のフォールバック
	// 値のまま返す）。SpawnEnemyAtの「読み取り」部分を独立させたもの
	EnemyTemplateData ReadEnemyTemplateData(const std::string& templateTag);

	// ReadEnemyTemplateDataの結果から実際にEnemy GameObjectを組み立てる。SpawnEnemyAtの
	// 「組み立て」部分を独立させたもの
	void BuildEnemyFromTemplateData(const Vector3& position, const std::string& templateTag, const EnemyTemplateData& data);

	// 準備フェーズ用のスポーン待ちキュー（テンプレートタグの列）。BeginPreparingPhaseが
	// pendingRespawnTags_から構築し、UpdatePreparingPhaseが先頭から1つずつ消費する
	std::vector<std::string> respawnQueue_;

	// 準備フェーズ中の経過時間。kRespawnInterval秒に達するたびリセットして1体スポーンする
	float respawnTimer_ = 0.0f;

	// 実行フェーズ中（まだ準備フェーズに入る前）に倒された敵のspawnedFromTagを溜めておく場所。
	// ProcessPendingDestroysが撃破のたびにここへ追加するだけに留め、実際のスポーンは
	// BeginPreparingPhase/UpdatePreparingPhaseが準備フェーズ中に行う
	std::vector<std::string> pendingRespawnTags_;

	// OnInitialize()はLoadScene()より前に呼ばれるため、その時点ではまだシーンJSON
	// （EnemySpawner・テンプレート等）が読み込まれておらず初回スポーンができない。
	// trueで初期化し、HandleSceneTransitionInputの最初の呼び出しで一度だけ
	// RespawnEnemiesIfCleared()を実行してfalseに落とす
	bool needsInitialSpawn_ = true;
};
