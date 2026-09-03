#include "PlayScene.h"
#include "GameTags.h"
#include "GameSession.h"
#include "../Engine/InputDevice/InputDevice.h"
#include "../Engine/GameObject/ComponentRegistry.h"
#include "../Math/VectorMath.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <string>

namespace {
	// 準備フェーズ中、1体スポーンしてから次の1体をスポーンするまでの間隔（秒）の既定値。
	// 通常はEnemySpawnerのReflexEnemySpawnerComponent::respawnInterval（Inspectorで調整可能）を
	// 使うため、これはEnemySpawnerが見つからない場合のフォールバックとしてのみ使われる
	constexpr float kRespawnInterval = 0.3f;

	// テンプレートGameObjectをPlay中に隠す際の退避先Y座標（フィールド±8の外側で確実に見えない位置）
	constexpr float kTemplateHideY = -1000.0f;

	// 残りラウンド数がこの値以下になったらRoundCountAlphabetの警告演出（赤色固定＋パルス拡大）を
	// 開始する閾値
	constexpr int kRoundWarningThreshold = 5;

	// 警告演出の色（常に赤固定）
	constexpr Vector4 kRoundWarningColor = { 1.0f, 0.15f, 0.15f, 1.0f };

	// パルス（ReflexPlayerComponentの移動先マーカーの波紋演出と同じ「最小サイズから広がって
	// 最大サイズに達したら、また最小サイズから再発生する」片道の繰り返し）1周期の長さ(秒)。
	// 残りラウンド数が少ないほど周期を短くして切迫感を出す：
	// period = kPulsePeriodBase + kPulsePeriodPerRound * remaining
	// （remaining=5で1.0秒、remaining=1で0.36秒、remaining=0で0.2秒程度になる）
	constexpr float kPulsePeriodBase = 0.2f;
	constexpr float kPulsePeriodPerRound = 0.16f;

	// パルスのスケール範囲（1.0倍を基準に、最小〜最大の倍率）
	constexpr float kPulseMinScale = 1.0f;
	constexpr float kPulseMaxScale = 1.3f;
}

void PlayScene::DrawSceneSpecificInspectorExtensions(GameObject& selected) {
	// ReflexEnemySpawnerComponent専用UI：spawnEntries[i].tagはシーン内のテンプレート
	// （ReflexEnemyComponent::isTemplate=trueのGameObject）のタグから選ぶコンボにする。
	// 自由入力にするとタグの手打ちミス（例："A"のつもりで"AEnemy"と書く）でテンプレートが
	// 見つからず、PlayScene::SpawnEnemyAtが既定値にフォールバックしてしまう事故が起きるため
	auto* spawnerConfig = selected.GetComponent<ReflexEnemySpawnerComponent>();
	if (!spawnerConfig) return;

	std::vector<std::string> templateTags;
	for (auto& obj : objects_) {
		auto* enemy = obj->GetComponent<ReflexEnemyComponent>();
		if (enemy && enemy->isTemplate && !obj->tag.empty()) {
			templateTags.push_back(obj->tag);
		}
	}

	ImGui::Text("敵の種類（テンプレートのタグ＋出現数）");
	if (templateTags.empty()) {
		ImGui::TextDisabled("  (敵テンプレート（isTemplate=true）を持つGameObjectがありません)");
	}
	for (size_t i = 0; i < spawnerConfig->spawnEntries.size(); i++) {
		ImGui::PushID(static_cast<int>(i));
		auto& entry = spawnerConfig->spawnEntries[i];

		if (ImGui::BeginCombo("テンプレートのタグ", entry.tag.c_str())) {
			for (const auto& t : templateTags) {
				bool isSelected = (entry.tag == t);
				if (ImGui::Selectable(t.c_str(), isSelected)) entry.tag = t;
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::DragInt("出現数", &entry.count, 0.2f, 0, 20);

		bool canRemove = spawnerConfig->spawnEntries.size() > 1;
		ImGui::BeginDisabled(!canRemove);
		if (ImGui::Button("この種類を削除")) {
			spawnerConfig->spawnEntries.erase(spawnerConfig->spawnEntries.begin() + i);
		}
		ImGui::EndDisabled();
		ImGui::Separator();
		ImGui::PopID();
	}
	if (ImGui::Button("種類を追加")) {
		spawnerConfig->spawnEntries.push_back(ReflexEnemySpawnerComponent::SpawnEntry{
			templateTags.empty() ? "" : templateTags.front(), 4 });
	}

	ImGui::Separator();
	ImGui::DragFloat("スポーン間隔の最小（秒）", &spawnerConfig->spawnIntervalMin, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat("スポーン間隔の最大（秒）", &spawnerConfig->spawnIntervalMax, 0.01f, 0.0f, 5.0f);

	ImGui::Separator();
	ImGui::Text("スポーン時のサイズランダム化（テンプレートに対する倍率）");
	ImGui::DragFloat("最小倍率", &spawnerConfig->sizeScaleMin, 0.02f, 0.1f, 5.0f);
	ImGui::DragFloat("最大倍率", &spawnerConfig->sizeScaleMax, 0.02f, 0.1f, 5.0f);

	ImGui::Separator();
	ImGui::DragFloat("プレイヤーとの最小距離", &spawnerConfig->playerExclusionRadius, 0.1f, 0.0f, 20.0f);
	// 最大が最小を下回ると乱数範囲が壊れる（std::uniform_real_distributionが未定義動作になる）ため、
	// UI操作直後に矛盾した値になった場合はここで最小側へ揃えて防ぐ
	if (spawnerConfig->sizeScaleMax < spawnerConfig->sizeScaleMin) {
		spawnerConfig->sizeScaleMax = spawnerConfig->sizeScaleMin;
	}

	ImGui::Separator();
	ImGui::Text("敵のスポーン範囲（壁の内側に収まるよう設定すること）");
	ImGui::DragFloat("範囲 X最小", &spawnerConfig->spawnRangeMinX, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat("範囲 X最大", &spawnerConfig->spawnRangeMaxX, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat("範囲 Y最小", &spawnerConfig->spawnRangeMinY, 0.1f, -100.0f, 100.0f);
	ImGui::DragFloat("範囲 Y最大", &spawnerConfig->spawnRangeMaxY, 0.1f, -100.0f, 100.0f);
	if (spawnerConfig->spawnRangeMaxX < spawnerConfig->spawnRangeMinX) {
		spawnerConfig->spawnRangeMaxX = spawnerConfig->spawnRangeMinX;
	}
	if (spawnerConfig->spawnRangeMaxY < spawnerConfig->spawnRangeMinY) {
		spawnerConfig->spawnRangeMaxY = spawnerConfig->spawnRangeMinY;
	}
	ImGui::DragFloat("敵同士の最小距離", &spawnerConfig->minSpawnDistance, 0.05f, 0.0f, 20.0f);
	ImGui::DragFloat("スポーン用グリッドのセル間隔", &spawnerConfig->spawnGridCellSize, 0.05f, 0.1f, 20.0f);

	ImGui::Separator();
	ImGui::Text("クリア条件");
	ImGui::DragInt("目標ラウンド数", &spawnerConfig->totalRounds, 0.2f, 1, 999);
}

void PlayScene::OnInitialize() {
	// Camera座標・実行タイマー・撃破数・コンボ数・スコアの動的HUD表示は、旧TextRenderComponent
	// ベースの仕組み（BuildHudDefinitions/CreateHud）ごと削除済み（分析・削除の経緯はメモリ参照）。
	// 残りラウンド数はKillCountAlphabetと同じ運用（タグ"RoundCountAlphabet"のAlphabetTextComponent
	// にHandleSceneTransitionInputがTextProviderを紐付ける）のため、ここでは何もしない

	// 敵の種類は「シーン上に配置されたテンプレートGameObject」（ReflexEnemyComponent::isTemplate=true、
	// 例：tag="A"）で表す。テンプレート自体が本物の敵として動いてしまわないよう、当たり判定を外し・
	// ReflexEnemyComponentを無効化し・フィールド外へ退避させておく
	// （見た目の描画コンポーネントは複製元として残す）。EnemySpawnerのタグ一覧経由ではなく
	// シーン全体を走査するため、EnemySpawnerに登録し忘れたテンプレートも確実に隠せる
	for (auto& obj : objects_) {
		auto* enemy = obj->GetComponent<ReflexEnemyComponent>();
		if (!enemy || !enemy->isTemplate) continue;
		obj->RemoveComponent<OBBColliderComponent>();
		obj->RemoveComponent<SphereColliderComponent>();
		enemy->enabled = false;
		obj->GetTransform().translation.y = kTemplateHideY;
	}

	// パーティクルテンプレート（ParticleTemplateComponent付き）も敵テンプレートと同じ理由で
	// フィールド外へ退避させる。当たり判定を持たないため外す処理は不要
	for (auto& obj : objects_) {
		if (!obj->GetComponent<ParticleTemplateComponent>()) continue;
		obj->GetTransform().translation.y = kTemplateHideY;
	}

	// 注意：OnInitialize()はSceneBase::Initialize()内でLoadScene()より前に呼ばれるため、
	// この時点ではまだResources/Play/scene.jsonの内容（EnemySpawner・テンプレート等）が
	// 読み込まれていない。ここでResetAllEnemies()を呼んでもFindObjectByTagが
	// 何も見つけられず常に空振りする（初回スポーンが出ない原因だった）。
	// 実際の初回スポーンはHandleSceneTransitionInput側の初回フレーム判定（needsInitialSpawn_）で行う
}

ReflexPlayerComponent* PlayScene::GetReflexPlayer() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	if (!player) return nullptr;
	return player->GetComponent<ReflexPlayerComponent>();
}

ComboPopupComponent* PlayScene::GetComboPopup() {
	GameObject* player = FindObjectByTag(GameTags::kPlayer);
	if (!player) return nullptr;
	return player->GetComponent<ComboPopupComponent>();
}

int PlayScene::GetTotalRounds() {
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			return config->totalRounds;
		}
	}
	return 30;
}

void PlayScene::HandleSceneTransitionInput() {
	// 初回フレームのみ：OnInitialize()の時点ではまだLoadScene()が済んでおらず
	// EnemySpawner/テンプレートが存在しないため、ここ（LoadScene()完了後に必ず呼ばれる
	// 最初のフレーム）で初回スポーンを行う。ResetAllEnemies()は保存シーンJSONに残っていた敵を
	// 問答無用で全部消してから配置し直すため、Playシーンを開くたびに毎回敵構成がリセットされる
	if (needsInitialSpawn_) {
		needsInitialSpawn_ = false;
		ResetAllEnemies();

		// タグ"KillCountAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、
		// killCount_を文字列化するTextProviderを紐付ける（ユーザーがInspectorで
		// AlphabetTextComponentを追加し、タグだけ付ければ自動でキルカウント表示になる）。
		// LoadScene()完了後のこのタイミングでのみ行えば十分（Providerはラムダとして
		// 保持され続けるため、毎フレーム呼び直す必要はない）
		if (GameObject* killCountObj = FindObjectByTag(GameTags::kKillCountAlphabet)) {
			if (auto* alphabetText = killCountObj->GetComponent<AlphabetTextComponent>()) {
				alphabetText->SetTextProvider([this]() {
					return std::to_string(killCount_);
				});
			}
		}

		// タグ"RoundCountAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、
		// 残りラウンド数（GetTotalRounds() - roundCount_）を文字列化するTextProviderを紐付ける。
		// KillCountAlphabetと同じ運用（ユーザーがInspectorでタグを付けるだけで自動的に動く）
		if (GameObject* roundCountObj = FindObjectByTag(GameTags::kRoundCountAlphabet)) {
			if (auto* alphabetText = roundCountObj->GetComponent<AlphabetTextComponent>()) {
				alphabetText->SetTextProvider([this]() {
					int remaining = (std::max)(GetTotalRounds() - roundCount_, 0);
					return std::to_string(remaining);
				});
			}
		}

		// タグ"ScoreAlphabet"のGameObjectにAlphabetTextComponentが付いていれば、
		// 現在のスコア（score_）を文字列化するTextProviderを紐付ける。KillCountAlphabetと同じ運用
		if (GameObject* scoreObj = FindObjectByTag(GameTags::kScoreAlphabet)) {
			if (auto* alphabetText = scoreObj->GetComponent<AlphabetTextComponent>()) {
				// score_そのものではなくdisplayScore_（UpdateScoreDisplayが1ずつ近づける表示専用の値）
				// を文字列化する。加算された瞬間に一気に反映せず、一の位から連続でカウントアップ
				// して見えるようにするため
				alphabetText->SetTextProvider([this]() {
					return std::to_string(displayScore_);
				});
			}
		}
	}

	// ColliderSystem::ResolveAndDrawの直後（Renderの最後）に呼ばれるこのタイミングで、
	// 今フレーム体当たりされた敵をまとめて回収する
	ProcessPendingDestroys();

	// タグ"Player"のReflexPlayerComponentが実行フェーズを完了して準備フェーズに入った瞬間だけ、
	// 直前の実行フェーズ中に倒した敵の補充スポーンを開始する（毎フレーム判定ではなく、
	// 実行フェーズが完全に終わったタイミングに揃える）
	if (auto* reflexPlayer = GetReflexPlayer()) {
		if (reflexPlayer->ConsumeExecutionFinished()) {
			BeginPreparingPhase();
		}
		if (reflexPlayer->GetPhase() == ReflexPlayerComponent::Phase::kPreparing) {
			UpdatePreparingPhase(lastDeltaTime_);
		}
		UpdateExecutionPhaseStats(reflexPlayer, lastDeltaTime_);
	}

	// 残りラウンド数が少なくなったらRoundCountAlphabetを赤くパルスさせて警告する
	UpdateRoundCountWarningVisual(lastDeltaTime_);

	// ScoreAlphabetのHUD表示を、score_へ向けて1ずつカウントアップさせる
	UpdateScoreDisplay(lastDeltaTime_);

	// ESCでいつでもTitleへ戻れるようにする
	if (Input::IsTriggered(DIK_ESCAPE)) nextScene_ = "Title";

	// デバッグ用：F2でその場のスコアのままClearSceneへ直接遷移する（目標ラウンド数まで
	// 実際にプレイしなくても、Clear画面の見た目・演出だけをすぐ確認できるようにするため）
	if (Input::IsTriggered(DIK_F2)) {
		GameSession::GetInstance().SetScore(score_);
		nextScene_ = "Clear";
	}
}

GameObject& PlayScene::GetOrCreateGroupFolder(const std::string& tag) {
	if (GameObject* existing = FindObjectByTag(tag)) return *existing;

	// フォルダ自体は見た目・当たり判定を持たない空のGameObject。名前とtagは同じ文字列にする
	// （tagで検索して使い回すのはこの関数自身、名前はヒエラルキー上の表示用）。
	// excludeFromGizmoList=trueにするとヒエラルキーのルート一覧からも消えてしまう
	// （SceneBase::isRootVisible参照）ため、ここでは付けない。3D空間クリックで誤選択しない
	// ようexcludeFromPickingのみ立てる
	GameObject& folder = CreateObject(tag);
	folder.tag = tag;
	folder.excludeFromPicking = true;
	return folder;
}

void PlayScene::ProcessPendingDestroys() {
	std::vector<GameObject*> toDestroy;
	// 撃破された敵・被弾した敵（生死問わず）の座標。ループ完了後にまとめてパーティクルを出す
	// （objects_へのCreateObjectはイテレート中には行えないため）
	std::vector<Vector3> deathParticlePositions;
	std::vector<Vector3> hitParticlePositions;
	// ループ内で毎回FindObjectByTagし直さないよう、1回だけ取得しておく（無ければnullptrのまま、
	// コンボ演出が単に出ないだけで撃破処理自体には影響しない）
	ComboPopupComponent* comboPopup = GetComboPopup();
	for (auto& obj : objects_) {
		if (auto* enemy = obj->GetComponent<EnemyComponent>()) {
			if (enemy->pendingDestroy) toDestroy.push_back(obj.get());
		}
		if (auto* reflexEnemy = obj->GetComponent<ReflexEnemyComponent>()) {
			if (reflexEnemy->pendingHitParticle) {
				hitParticlePositions.push_back(obj->GetWorldTransform().translation);
				reflexEnemy->pendingHitParticle = false;
			}
			// HitSoundComponentはCreateObjectを伴わないため、objects_をイテレート中のこの場で
			// 直接鳴らして問題ない（同じGameObjectに付いていなければ何もしない）
			if (reflexEnemy->pendingHitSound) {
				if (auto* hitSound = obj->GetComponent<HitSoundComponent>()) hitSound->Play();
				reflexEnemy->pendingHitSound = false;
			}
			if (reflexEnemy->pendingDestroy) {
				toDestroy.push_back(obj.get());
				deathParticlePositions.push_back(obj->GetWorldTransform().translation);
				// 準備フェーズ方式：ここでは即座に補充せず、種類だけ覚えておく。実際のスポーンは
				// 実行フェーズ完了後の準備フェーズ（BeginPreparingPhase/UpdatePreparingPhase）で
				// 1体ずつ間隔を空けて行う
				if (!reflexEnemy->spawnedFromTag.empty()) {
					pendingRespawnTags_.push_back(reflexEnemy->spawnedFromTag);
				}
				// HUD用の撃破数・コンボ数。killCount_は累計、comboCount_は計画フェーズへ戻るまで
				// 積み上がり続ける（UpdateExecutionPhaseStatsがフェーズ切替の瞬間に0へ戻す）
				killCount_++;
				comboCount_++;

				// スコアは「1体倒すごとに現在のコンボ数を加算」する方式。comboCount_を先に
				// インクリメントした後の値を使うため、同じ実行フェーズ内で連続して倒すほど
				// 1体あたりの獲得スコアが増えていく（コンボを繋げることへの報酬になる）
				score_ += comboCount_;
				phaseScore_ += comboCount_;

				// プレイヤー頭上には、連続撃破数（comboCount_）そのものではなく、この実行フェーズ
				// 中に稼いだ累計スコア（phaseScore_、HUDのCombo欄と同じ値）をポップアップ表示する。
				// ComboPopupComponentが付いていない（プレイヤーに未追加）場合は何もしない
				if (comboPopup) comboPopup->RequestPopup(phaseScore_);
			}
		}
		if (auto* particle = obj->GetComponent<ParticleComponent>()) {
			if (particle->pendingDestroy) toDestroy.push_back(obj.get());
		}
	}
	if (!toDestroy.empty()) DeleteObjects(toDestroy);

	for (const Vector3& position : hitParticlePositions) {
		enemySpawnManager_.SpawnParticleBurstAt(*this, position, "EnemyHitParticle");
	}
	for (const Vector3& position : deathParticlePositions) {
		enemySpawnManager_.SpawnParticleBurstAt(*this, position, "EnemyDeathParticle");
	}
}

void PlayScene::ResetAllEnemies() {
	// tag=="Enemy"のオブジェクトを問答無用で全部削除する（保存シーンJSONから復元されていた分・
	// 前回のプレイの残り、どちらも対象）。「既に敵がいれば何もしない」ガードを設けず、
	// Playシーンを開くたびに毎回この関数で敵構成をリセットしたいため
	std::vector<GameObject*> existingEnemies;
	for (auto& obj : objects_) {
		if (obj->tag == GameTags::kEnemy) existingEnemies.push_back(obj.get());
	}
	if (!existingEnemies.empty()) DeleteObjects(existingEnemies);

	SpawnEnemiesFromConfig();
}

void PlayScene::SpawnEnemiesFromConfig() {
	// タグ"EnemySpawner"のGameObjectにReflexEnemySpawnerComponentを付けておくと、
	// Inspectorで設定した「タグ＋出現数」のリストがここで反映される。見つからない場合は何も出さない
	GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner);
	if (!spawner) return;
	auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>();
	if (!config) return;

	// spawnEntries（種類ごとの個数）を1体ずつのタグ列に展開し、種類も含めて丸ごとシャッフルする。
	// 一気に全部出さず、UpdatePreparingPhaseの「準備フェーズ＝1体ずつランダム間隔で出す」仕組みを
	// そのまま流用する（respawnQueue_はback()から1つずつpop_backで消費するので、シャッフル順が
	// そのままスポーン順になる）
	std::vector<std::string> initialQueue;
	for (const auto& entry : config->spawnEntries) {
		for (int i = 0; i < entry.count; i++) {
			initialQueue.push_back(entry.tag);
		}
	}
	if (initialQueue.empty()) return;

	static std::mt19937 rng{ std::random_device{}() };
	std::shuffle(initialQueue.begin(), initialQueue.end(), rng);

	respawnQueue_ = std::move(initialQueue);
	respawnTimer_ = 0.0f;
	currentSpawnInterval_ = PickNextSpawnInterval();

	// これから入るkPreparingは「実行フェーズ完了後の補充」ではなく起動直後の初回スポーンのため、
	// このkPreparing→kPlanning遷移をUpdateExecutionPhaseStatsのラウンドカウントに数えないよう
	// フラグを立てておく
	isInitialSpawnInProgress_ = true;

	// プレイヤーはこの時間差スポーンが終わるまで操作不可にする（実行フェーズ完了後の
	// 補充スポーンと同じ「準備フェーズ」を、計画フェーズから明示的に開始する形で流用する）
	if (auto* reflexPlayer = GetReflexPlayer()) {
		reflexPlayer->BeginPreparing();
	}
}

void PlayScene::UpdateScoreDisplay(float deltaTime) {
	if (displayScore_ >= score_ && scoreAnimTarget_ == score_) return;

	// score_が演出の目標値と食い違っている＝新しく加算された（連続撃破でさらに加算された
	// 場合も含む）ので、その時点のdisplayScore_を始点、score_を終点として演出を仕切り直す。
	// 経過時間もリセットするため、連続で敵を倒し続けても常にkScoreCountUpDuration秒後に
	// 追いつく（差分の大小に関わらず所要時間が一定になる）
	if (scoreAnimTarget_ != score_) {
		scoreAnimStart_ = displayScore_;
		scoreAnimTarget_ = score_;
		scoreAnimElapsed_ = 0.0f;
	}

	scoreAnimElapsed_ += deltaTime;
	float t = (std::min)(scoreAnimElapsed_ / kScoreCountUpDuration, 1.0f);
	displayScore_ = scoreAnimStart_ + static_cast<int>(
		std::lround((scoreAnimTarget_ - scoreAnimStart_) * t));
}

void PlayScene::UpdateExecutionPhaseStats(ReflexPlayerComponent* reflexPlayer, float deltaTime) {
	ReflexPlayerComponent::Phase currentPhase = reflexPlayer->GetPhase();

	// 実行フェーズ中だけ経過秒数を積算する（計画フェーズ・準備フェーズ中は止めておく）
	if (currentPhase == ReflexPlayerComponent::Phase::kExecuting) {
		executionTimer_ += deltaTime;
	}

	// kPreparing→kPlanning（準備フェーズが終わって計画フェーズへ戻った瞬間）を検知したら、
	// 次の実行フェーズに備えてタイマー・コンボを0にリセットする。killCount_（累計）はリセットしない。
	// 敵を1体も倒さずに実行フェーズを終えた場合、BeginPreparingPhaseがrespawnQueue_が空のまま
	// 即座にFinishPreparing()を呼ぶため、kPreparingを1フレームも経由せずkExecuting→kPlanningへ
	// 直接遷移する。この場合も同じく1ラウンドの区切りとして扱う必要があるため、kExecuting→
	// kPlanningもここで検知対象に含める（さもないと「敵を倒さなかった回はラウンドが
	// 進まない」不具合になる）
	if ((previousPhase_ == ReflexPlayerComponent::Phase::kPreparing ||
		 previousPhase_ == ReflexPlayerComponent::Phase::kExecuting) &&
		currentPhase == ReflexPlayerComponent::Phase::kPlanning) {
		executionTimer_ = 0.0f;
		comboCount_ = 0;
		phaseScore_ = 0;

		// 実行フェーズ終了に合わせて、表示中のコンボポップアップも演出の途中でも即座に消す
		if (auto* comboPopup = GetComboPopup()) {
			comboPopup->ClearAll();
		}

		// 「準備フェーズが終わり、計画フェーズに戻った瞬間」を1ラウンドの区切りとして数える。
		// ただし起動直後の初回スポーン完了によるこの遷移は「1ラウンドを実際にプレイし終えた」
		// わけではないため数えない（isInitialSpawnInProgress_のコメント参照）
		if (isInitialSpawnInProgress_) {
			isInitialSpawnInProgress_ = false;
		} else {
			// 目標ラウンド数に達したら、その時点のスコアをランキングに登録するためClearSceneへ遷移する
			roundCount_++;
			if (roundCount_ >= GetTotalRounds()) {
				// ClearSceneはGameObjectを持たないPlaySceneのメンバへ直接アクセスできないため、
				// シーンをまたいで値を持ち越せるGameSessionシングルトン経由で最終スコアを渡す
				GameSession::GetInstance().SetScore(score_);
				nextScene_ = "Clear";
			}
		}
	}

	previousPhase_ = currentPhase;
}

void PlayScene::UpdateRoundCountWarningVisual(float deltaTime) {
	GameObject* roundCountObj = FindObjectByTag(GameTags::kRoundCountAlphabet);
	if (!roundCountObj) return;
	auto* alphabetText = roundCountObj->GetComponent<AlphabetTextComponent>();
	if (!alphabetText) return;

	int remaining = (std::max)(GetTotalRounds() - roundCount_, 0);

	if (remaining > kRoundWarningThreshold) {
		// 閾値を上回っている間は既定の見た目（等倍・白）に戻し、タイマーも次に閾値以下へ
		// 入った瞬間に同じ位相（scale最小）から始まるようリセットしておく
		alphabetText->displayScaleMultiplier = 1.0f;
		alphabetText->displayColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		roundWarningPulseTimer_ = 0.0f;
		return;
	}

	// 残りが少ないほど周期を短くする（remaining=0のときが最速）
	float period = kPulsePeriodBase + kPulsePeriodPerRound * static_cast<float>(remaining);
	roundWarningPulseTimer_ = std::fmod(roundWarningPulseTimer_ + deltaTime, period);

	// ReflexPlayerComponent::DrawPlanningVisualizationの波紋アニメーションと同じ考え方：
	// t（0=周期の始まり、1=周期の終わり）をkInOutSineでイージングし、最小→最大サイズへ
	// 片道で広がったら、また最小サイズから再発生する（往復ではなく毎回リセットする波紋型）
	float t = period > 0.0f ? roundWarningPulseTimer_ / period : 0.0f;
	float eased = Easing::Apply(Easing::Type::kInOutSine, t);
	alphabetText->displayScaleMultiplier = kPulseMinScale + (kPulseMaxScale - kPulseMinScale) * eased;
	alphabetText->displayColor = kRoundWarningColor;
}

float PlayScene::PickNextSpawnInterval() {
	float intervalMin = kRespawnInterval;
	float intervalMax = kRespawnInterval;
	if (GameObject* spawner = FindObjectByTag(GameTags::kEnemySpawner)) {
		if (auto* config = spawner->GetComponent<ReflexEnemySpawnerComponent>()) {
			intervalMin = config->spawnIntervalMin;
			intervalMax = config->spawnIntervalMax;
		}
	}
	// maxがminを下回っているとstd::uniform_real_distributionが未定義動作になるため、
	// Inspector側のガードをすり抜けて不正な値が来た場合の保険として上限を下限でクランプする
	// （PlayScene::SpawnEnemyAtのsizeScaleMin/Max抽選と同じパターン）
	intervalMax = (std::max)(intervalMax, intervalMin);
	static std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(intervalMin, intervalMax);
	return dist(rng);
}

void PlayScene::BeginPreparingPhase() {
	// pendingRespawnTags_（実行フェーズ中に倒した敵の種類の一覧）をそのままキューへ移す。
	// 消費順に意味は無いためシャッフルはしない（間隔を空けて1体ずつ出すだけの演出）
	respawnQueue_ = pendingRespawnTags_;
	pendingRespawnTags_.clear();
	respawnTimer_ = 0.0f;
	currentSpawnInterval_ = PickNextSpawnInterval();

	// 倒した敵が1体もいなかった場合（何もせず実行フェーズを終えた等）は補充するものが無いため、
	// 準備フェーズの演出をスキップして即座に計画フェーズへ戻す
	if (respawnQueue_.empty()) {
		if (auto* reflexPlayer = GetReflexPlayer()) {
			reflexPlayer->FinishPreparing();
		}
	}
}

void PlayScene::UpdatePreparingPhase(float deltaTime) {
	if (respawnQueue_.empty()) return;

	respawnTimer_ += deltaTime;
	if (respawnTimer_ < currentSpawnInterval_) return;
	respawnTimer_ = 0.0f;

	// 呼び出しのたびにグリッドを作り直す：直前にスポーンした敵が占有したマスを
	// 次の1体が避けられるようにするため（BuildShuffledSpawnGridは現在生存中の敵の
	// 座標を毎回見て占有マスを除外している）
	std::vector<Vector3> cells = enemySpawnManager_.BuildShuffledSpawnGrid(*this);
	Vector3 position = enemySpawnManager_.PickEnemySpawnPosition(*this, cells);
	enemySpawnManager_.SpawnEnemyAt(*this, position, respawnQueue_.back());
	respawnQueue_.pop_back();

	// このタイミングで最後の1体を出し終えたなら、準備フェーズを終えて計画フェーズへ戻す
	if (respawnQueue_.empty()) {
		if (auto* reflexPlayer = GetReflexPlayer()) {
			reflexPlayer->FinishPreparing();
		}
	} else {
		// まだ残りがある場合は次の1体分の待ち時間を新しく抽選する
		currentSpawnInterval_ = PickNextSpawnInterval();
	}
}


REGISTER_SCENE(PlayScene, "Play");
